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
// Basic JSON Serialiser

#include "Serialize.h"

namespace penguinTrace
{
  namespace server
  {

    std::unique_ptr<Serialize> Serialize::sessionState(Session& session)
    {
      std::unique_ptr<Serialize> resp(new Serialize());

      resp->stream << "{";
      resp->addBool("state", true);
      resp->stream << ",";
      resp->addBool("retry", false);
      resp->stream << ",";
      resp->addString("arch", MACHINE_ARCH);
      resp->stream << ",";

      if (session.getCompileFailures()->size() > 0)
      {
        resp->addBool("done", false);
        resp->stream << ",";
        resp->addBool("compile", false);
        resp->stream << ",";
        resp->addQueue<CompileFailureReason>("failures",
            *session.getCompileFailures(), [&](CompileFailureReason c)
        {
          std::stringstream s;

          s << "{\"category\": \"" << c.getCategory();
          s << "\", \"desc\": \"" << jsonEscape(c.getDescription());
          s << "\"";
          if (c.hasLocation())
          {
            s << ", \"line\": " << c.getLine();
            s << ", \"column\": " << c.getColumn();
          }
          s << "}";

          return s.str();
        });

        resp->stream << "}";
        // Once consumed errors, can remove session
        session.setRemove();
      }
      else
      {
        resp->addString("source", jsonEscape(session.getSource()));
        resp->stream << ",";
        resp->addString("lang", session.getLang());
        resp->stream << ",";
        resp->addBool("done", session.getStepper()->isDone());
        resp->stream << ",";
        uint64_t pc = session.getStepper()->getLastPC();
        auto loc = session.getDwarfInfo()->locationByPC(pc, true);
        resp->addBool("compile", true);
        resp->stream << ",";
        resp->addMap<object::LineDisassembly>("disassembly", "dis",
            *session.getDisasmMap(), [&](object::LineDisassembly dis)
            {
              return dis.getCodeDis();
            });
        resp->stream << "," << std::endl;
        resp->addMap<object::Parser::SectionPtr>("sections", "name",
            session.getParser()->getSectionAddrMap(),
            [&](object::Parser::SectionPtr s)
            {
              return tryDemangle(s->getName());
            });
        resp->stream << "," << std::endl;
        resp->addMap<object::Parser::SymbolPtr>("symbols", "name",
            session.getParser()->getSymbolAddrMap(),
            [&](object::Parser::SymbolPtr s)
            {
              return tryDemangle(s->getName());
            });
        resp->stream << "," << std::endl;

        resp->addMap("regs", session.getStepper()->getRegValues());

        resp->stream << ", \"pc\": ";
        resp->stream << pc;
        resp->stream << ",";
        if (loc.found())
        {
          resp->stream << " \"location\": {";
          resp->addInt("line", loc.line());
          resp->stream << ",";
          resp->addInt("column", loc.column());
          resp->stream << "},";
        }
        resp->addQueue("stdout", session.getStepper()->getStdout());
        resp->stream << ",";
        resp->addBreakpoints(session);
        resp->stream << ",";
        resp->addStackTrace(session);
        resp->stream << "}";

      }

      return resp;
    }

    std::unique_ptr<Serialize> Serialize::stepState(Session& session)
    {
      std::unique_ptr<Serialize> resp(new Serialize());
      uint64_t pc = session.getStepper()->getLastPC();

      auto disItr = session.getDisasmMap ()->find (pc);
      std::string disasmStr =
          (disItr != session.getDisasmMap ()->end ()) ?
              disItr->second.getCodeDis () :
              session.getStepper ()->getLastDisasm ();

      auto loc = session.getDwarfInfo()->locationByPC(pc, true);

      resp->stream << "{";
      // stepState only called after checking there are no pending commands
      resp->addBool("state", true);
      resp->stream << ",";
      resp->addBool("step", true);
      resp->stream << ",";
      resp->addBool("retry", false);
      resp->stream << ",";
      resp->addBool("done", session.getStepper()->isDone());
      resp->stream << ",";
      resp->addString("arch", MACHINE_ARCH);
      resp->stream << ",";

      resp->addMap("regs", session.getStepper()->getRegValues());
      resp->stream << ",";
      resp->addMap("vars", session.getStepper()->getVarValues());

      resp->stream << ", \"pc\": ";
      resp->stream << pc;
      resp->stream << ", \"disasm\": \"";
      resp->stream << jsonEscape (disasmStr);
      resp->stream << "\", ";

      if (loc.found())
      {
        resp->stream << " \"location\": {";
        resp->addInt("line", loc.line());
        resp->stream << ",";
        resp->addInt("column", loc.column());
        resp->stream << "},";
      }

      resp->addQueue("stdout", session.getStepper()->getStdout());
      resp->stream << ",";
      resp->addBreakpoints(session);
      resp->stream << ",";
      resp->addStackTrace(session);
      resp->stream << "}";

      return resp;
    }

    std::unique_ptr<Serialize> Serialize::bkptState(Session &session, bool ok)
    {
      std::unique_ptr<Serialize> resp(new Serialize());

      resp->stream << "{";
      resp->addBool("bkpt", ok);
      resp->stream << ",";
      resp->addBool("error", false);
      resp->stream << ",";
      resp->addBreakpoints(session);
      resp->stream << "}";

      return resp;
    }

    void Serialize::addBreakpoints(Session& session)
    {
      auto bkpts = session.getStepper()->getBreakpoints();
      auto pendBkpts = session.getStepper()->getPendingBreakpoints();
      std::list<uint64_t> bkptList;

      for (auto b : bkpts)
      {
        bkptList.push_back(b.first);
      }
      for (auto b : pendBkpts)
      {
        bkptList.push_back(b);
      }
      addArray("bkpts", bkptList.begin(), bkptList.end(), [&](uint64_t v) {
        std::stringstream s;
        s << v;
        return s.str();
      });

      stream << ",";

      std::set<uint64_t> bkptLineList;
      for (auto b : bkptList)
      {
        auto loc = session.getDwarfInfo()->exactLocationByPC(b);
        if (loc.found())
        {
          bkptLineList.insert(loc.line());
        }
      }
      addArray("bkptLines", bkptLineList.begin(), bkptLineList.end(), [&](uint64_t v) {
        std::stringstream s;
        s << v;
        return s.str();
      });
    }

    void Serialize::addStackTrace(Session& session)
    {
      auto stack = session.getStepper()->getStackTrace();
      addArray("stacktrace", stack.begin(), stack.end(), [&](std::string str) {
        std::stringstream s;
        s << '"' << jsonEscape(str) << '"';
        return s.str();
      });
    }

    void Serialize::addBool(std::string name, bool value)
    {
      stream << '"' << name << "\":";
      stream << (value ? "true" : "false");
      stream << std::endl;
    }

    void Serialize::addString(std::string name, std::string value)
    {
      stream << '"' << name << "\":";
      stream << '"' << value << '"';
      stream << std::endl;
    }

    void Serialize::addInt(std::string name, uint32_t value)
    {
      stream << '"' << name << "\":";
      stream << value;
      stream << std::endl;
    }

    void Serialize::addQueue(std::string name, std::queue<std::string>& queue)
    {
      stream << '"' << name << "\": [";
      bool first = true;
      while (!queue.empty())
      {
        if (!first)
        {
          stream << ",";
        }
        first = false;
        stream << "\"";
        stream << jsonEscape(queue.front());
        stream << "\"";
        queue.pop();
      }
      stream << "]";
    }

    void Serialize::addMap(std::string name, std::map<std::string, uint64_t> values)
    {
      addArray(name, values.begin(), values.end(), [&](std::pair<std::string, uint64_t> v) {
        std::stringstream s;
        uint32_t high = (v.second >> 32) & 0xffffffff;
        uint32_t low = v.second & 0xffffffff;
        s << "{ \"name\": \"";
        s << v.first;
        s << "\",\"high\": " << high;
        s << ",\"low\": " << low;
        s << "}";
        return s.str();
      });
    }

    void Serialize::addMap(std::string name, std::map<std::string, std::string> values)
    {
      addArray(name, values.begin(), values.end(), [&](std::pair<std::string, std::string> v) {
        std::stringstream s;
        s << "{ \"name\": \"";
        s << v.first;
        s << "\",\"value\": \"" << jsonEscape(v.second);
        s << "\"}";
        return s.str();
      });
    }

    void Serialize::addMap(std::string name, std::string propName, std::map<uint64_t, std::string> values)
    {
      addArray(name, values.begin(), values.end(), [&](std::pair<uint64_t, std::string> v) {
        std::stringstream s;
        s << "{\"pc\": ";
        s << v.first;
        s << ",\"" << propName << "\": \"" << jsonEscape(v.second);
        s << "\"}";
        return s.str();
      });
    }

    std::ostream & operator<<(std::ostream &out, const Serialize &obj)
    {
      obj.print(out);
      return out;
    }

    Serialize::Serialize()
    {

    }

    Serialize::~Serialize()
    {

    }

  } /* namespace server */
} /* namespace penguinTrace */
