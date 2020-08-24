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
// Compile a penguinTrace binary

#include <memory>
#include <thread>
#include <algorithm>

#include "debug/Compiler.h"

void logThread(penguinTrace::Logger* log, bool* run)
{
  while (*run)
  {
    log->printMessages();
    std::this_thread::yield();
  }
  log->printMessages();
}

int main(int argc, char **argv)
{
  std::string desc = "Compile a binary for penguinTrace";
  if (penguinTrace::Config::parse(argc, argv, true, desc))
  {
    bool running = true;
    std::string filename(
        penguinTrace::Config::get(penguinTrace::C_FILE_ARGUMENT).String());

    penguinTrace::Logger logger(penguinTrace::Logger::DBG);
    std::thread lThread(&logThread, &logger, &running);

    logger.log(penguinTrace::Logger::INFO, penguinTrace::Config::print());

    std::string extension = "";
    std::string lang = "c";
    if (filename.find('.') != std::string::npos)
    {
      auto f = penguinTrace::split(filename, '.');
      extension = *f.rbegin();
      std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

      if ((extension == "s") ||
          (extension == "asm"))
      {
        lang = "asm";
      }
      else if ((extension == "cxx") ||
               (extension == "cpp"))
      {
        lang = "cxx";
      }
      else if ((extension == "c"))
      {
        lang = "c";
      }
      else if ((extension == "rs"))
      {
        lang = "rust";
      }
      else
      {
        // Will fail on compile
        lang = extension;
      }

    }

    std::ifstream srcfStrm(filename);
    std::stringstream srcStrm;
    srcStrm << srcfStrm.rdbuf();
    std::string src = srcStrm.str();

    auto tempFile = penguinTrace::getTempFile(penguinTrace::Config::get(penguinTrace::C_TEMP_FILE_TPL).String(), false);
    std::string fname = "";

    if (tempFile.first > 0)
    {
      fname = tempFile.second;
      close(tempFile.first);

      logger.log(penguinTrace::Logger::INFO, "Compiling to '"+fname+"'");

      // TODO specifying cmd line args
      std::string argsStr = "lang="+lang+"&args=";
      std::map<std::string, std::string> args;

      const std::vector<std::string>& splitQuery = penguinTrace::split(argsStr, '&');
      for (auto it = splitQuery.begin(); it != splitQuery.end(); ++it)
      {
        const std::vector<std::string>& splitQueryInner = penguinTrace::splitFirst(*it, '=');
        args[splitQueryInner[0]] = splitQueryInner[1];
      }

      auto cLog = logger.subLogger("compile");
      penguinTrace::Compiler compiler(src, args, argsStr, fname, cLog.get());

      compiler.execute();

      auto failures = compiler.failures();

      if (failures.size() == 0)
      {
        logger.log(penguinTrace::Logger::INFO, "Compile OK");
      }
      else
      {
        while (!failures.empty())
        {
          auto fail = failures.front();
          failures.pop();
          std::stringstream s;
          s << fail.getCategory() << ": ";
          if (fail.hasLocation())
          {
            s << "(ln " << fail.getLine() << ") ";
          }
          s << fail.getDescription();
          logger.log(penguinTrace::Logger::ERROR, s.str());
        }
        logger.log(penguinTrace::Logger::ERROR, "Compile Failed");
        int ret = unlink(fname.c_str());
        if (ret != 0)
        {
          logger.log(penguinTrace::Logger::WARN, "Failed to delete output file '"+fname+"'");
        }
      }
    }
    else
    {
      logger.log(penguinTrace::Logger::ERROR, "Failed to create output file '"+fname+"'");
    }

    running = false;
    lThread.join();
  }

  return 0;
}
