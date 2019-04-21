// ----------------------------------------------------------------
// Copyright (C) 2019 Alex Beharrell
//
// This file is part of penguinTrace.
//
// penguinTrace is free software: you can redistribute it and/or
// modify it under the terms of the GNU Affero General Public
// License as published by the Free Software Foundation, either
// version 3 of the License, or any later version.
//
// penguinTrace is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with penguinTrace. If not, see
// <https://www.gnu.org/licenses/>.
// ----------------------------------------------------------------
//
// penguinTrace Session Command Wrappers

#include "SessionCmd.h"

#include "SessionManager.h"

#include "../object/ParserFactory.h"
#include "../object/LineDisassembly.h"
#include "../object/Disassembler.h"
#include "../debug/Stepper.h"

#include <thread>

namespace penguinTrace
{

  SessionCmd::SessionCmd()
  {
  }

  SessionCmd::~SessionCmd()
  {
  }

  void StepCmd::run()
  {
    stepper->step(step);

    if (STEPPER_ALWAYS_SINGLE_STEP)
    {
      bool done = stepper->singleStepDone(step);

      while (!done)
      {
        stepper->step(step);
        done = stepper->singleStepDone(step);
      }
    }

    if (stepper->shouldStepAgain() && stepper->active())
    {
      stepper->step(STEP_INSTR);
    }

    if (stepper->shouldContinueToEnd() && stepper->active())
    {
      stepper->step(STEP_CONT);
    }
  }

  std::string StepCmd::repr()
  {
    return "StepCmd";
  }

  void StdinCmd::run()
  {
    stepper->sendStdin(string);
  }

  std::string StdinCmd::repr()
  {
    return "StdinCmd";
  }

  void CompileCmd::run()
  {
    // Empty any unconsumed compile failures in session
    while (!failures->empty())
    {
      failures->pop();
    }

    compiler->execute();

    failures->swap(compiler->failures());
  }

  std::string CompileCmd::repr()
  {
    return "CompileCmd";
  }

  const std::string base64_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "0123456789+/=";

  void UploadCmd::run()
  {
    // Empty any unconsumed compile failures in session
    while (!failures->empty())
    {
      failures->pop();
    }

    if ((obj.length() % 4) != 0)
    {
      failures->push(
        CompileFailureReason("Upload Error",
                             "Cannot decode - Base64 must have a length which is a multiple of 4"));
    }
    else
    {
      std::ofstream ofs(filename);

      uint8_t inValue[4];
      uint64_t outValue;

      for (int i = 0; i < obj.length(); i+=4)
      {
        std::cout << std::endl;
        for (int j = 0; j < 4; ++j)
        {
          char c = obj.at(i+j);
          auto pos = base64_map.find(c);
          if (pos == std::string::npos)
          {
            failures->push(
              CompileFailureReason("Upload Error",
                                   "Cannot decode - Invalid Base64 character"));
            return;
          }
          inValue[j] = pos;
        }

        // Determine padding bits (= is at position 64)
        int numOutputBytes = inValue[2] == 64 ? 1 :
                             inValue[1] == 64 ? 2 :
                                                3;

        outValue = 0;
        for (int n = 3; n >= 0; --n)
        {
          outValue |= (inValue[n] & 0x3f) << ((3-n)*6);
        }

        for (int n = 0; n < numOutputBytes; ++n)
        {
          const char b = (outValue >> ((2-n)*8)) & 0xff;
          ofs.write(&b, 1);
        }
      }
      ofs.close();
    }
  }

  std::string UploadCmd::repr()
  {
    return "UploadCmd";
  }

  void ParseCmd::run()
  {
    auto session = sMgr->lockSession(sessionName);

    if (session.valid())
    {
      std::ifstream exeFile(session->executable(), std::ios::binary | std::ios::ate);
      std::streamsize size = exeFile.tellg();
      exeFile.seekg(0, std::ios::beg);

      auto exeBuf = std::unique_ptr<std::vector<char> >(new std::vector<char>(size));
      if (exeFile.read(exeBuf->data(), size))
      {
        session->setBuffer(std::move(exeBuf));
        log->log(Logger::DBG, "Read executable into buffer");
      }
      else
      {
        log->log(Logger::WARN, "Failed to read executable, will not be able to download");
      }

      auto parser = object::ParserFactory::getParser(session->executable(), log->subLogger("PARSE"));
      if (session->getCompileFailures()->empty() && parser && parser->parse())
      {
        session->setParser(std::move(parser), log->subLogger("DWARF"));
        object::Disassembler disassembler(session->getParser()->getSymbolAddrMap());

        for (auto section : session->getParser()->getSectionAddrMap())
        {
          auto secPtr = section.second;
          if (secPtr->getName().length() > 0)
          {
            size_t bytesLeft = secPtr->getSize();
            uint64_t pc = secPtr->getAddress();
            std::istream is(&secPtr->getContents());

            while (bytesLeft > 0)
            {
              size_t pos = is.tellg();
              std::vector<uint8_t> bytes;
              int maxLength = bytesLeft > MAX_INSTR_BYTES ? MAX_INSTR_BYTES : bytesLeft;
              for (int i = 0; i < maxLength; ++i)
              {
                bytes.push_back(is.get());
              }

              int consumed;
              std::string codeDis;
              if (secPtr->isCode())
              {
                codeDis = disassembler.disassemble(pc, bytes, &consumed);
              }
              else
              {
                std::stringstream hexStr;
                consumed = bytes.size() < 4 ? bytes.size() : 4;
                for (int i = 0; i < 4; ++i)
                {
                  if (i != 0)
                  {
                    hexStr << " ";
                  }
                  if (i < consumed)
                  {
                    hexStr << HexPrint(bytes[i], 2);
                  }
                  else
                  {
                    hexStr << "    ";
                  }
                }
                hexStr << " ";
                for (int i = 0; i < consumed; ++i)
                {
                  hexStr << makePrintable(bytes[i], '.');
                }
                codeDis = hexStr.str();
              }
              object::LineDisassembly disObj(bytes.data(), consumed, pc, codeDis);

              session->getDisasmMap()->insert(std::pair<uint64_t, object::LineDisassembly>(pc, disObj));

              is.seekg(pos+disObj.getLength());
              pc += disObj.getLength();
              bytesLeft -= disObj.getLength();
            }
          }
        }

        log->log(Logger::TRACE, "Disassembly Map:");
        for (auto dis : *session->getDisasmMap())
        {
          log->log(Logger::TRACE, [&]() {
            std::stringstream s;
            s << HexPrint(dis.first, ADDR_WIDTH_CHARS) << ": ";
            s << dis.second.getCodeDis();
            return s.str();
          });
        }

        auto argsQueue = std::unique_ptr<std::queue<std::string> >(new std::queue<std::string>());
        auto cfgIt = session->getParser()->getSectionNameMap().find(ELF_CONFIG_SECTION);
        auto srcIt = session->getParser()->getSectionNameMap().find(ELF_SOURCE_SECTION);

        std::map<std::string, std::string> requestArgs;

        if (cfgIt != session->getParser()->getSectionNameMap().end() &&
            srcIt != session->getParser()->getSectionNameMap().end())
        {
          std::istream cis(&cfgIt->second->getContents());
          std::stringstream cfgStrm;
          cfgStrm << cis.rdbuf();
          const std::vector<std::string>& splitQuery = split(cfgStrm.str(), '&');
          for (auto it = splitQuery.begin(); it != splitQuery.end(); ++it)
          {
            const std::vector<std::string>& splitQueryInner = splitFirst(*it, '=');
            requestArgs[splitQueryInner[0]] = splitQueryInner[1];
          }

          auto langIt = requestArgs.find("lang");
          if (langIt != requestArgs.end())
          {
            session->setLang(langIt->second);
          }

          std::istream sis(&srcIt->second->getContents());
          std::stringstream srcStrm;
          srcStrm << sis.rdbuf();
          session->setSource(srcStrm.str());
        }
        else
        {
          session->getCompileFailures()->push(
            CompileFailureReason("Upload Error",
                                 "Couldn't find config/source section in uploaded ELF"));
        }

        // Step on compile
        auto argsIt = requestArgs.find("args");

        if (argsIt != requestArgs.end())
        {
          std::string decoded = urlDecode(argsIt->second);
          auto args = split(decoded, ' ');

          for (auto arg : args)
          {
            argsQueue->push(arg);
          }
        }

        std::unique_ptr<Stepper> stepper(
            new Stepper(session->executable(), session->getParser(),
                        std::move(argsQueue), log->subLogger("STEP")));

        if (stepper->init())
        {
          stepper->step(STEP_INSTR);
          session->setStepper(std::move(stepper));
        }
        else
        {
          session->getCompileFailures()->push(
            CompileFailureReason("Stepper Error",
                                 "Couldn't initialise stepper"));
        }
      }
      else
      {
        session->getCompileFailures()->push(
            CompileFailureReason("Parser Error",
                                 "Couldn't get parser for executable"));
      }
    }
    else
    {
      session->getCompileFailures()->push(
          CompileFailureReason("Session Error", "Couldn't get session"));
    }
  }

  std::string ParseCmd::repr()
  {
    return "ParseCmd";
  }

} /* namespace penguinTrace */
