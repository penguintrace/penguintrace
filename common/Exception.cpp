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
// Custom Exceptions

#include "Exception.h"

#include <sstream>

#include <execinfo.h>

#include <iostream>

#include "Logger.h"
#include "../dwarf/Info.h"
#include "../object/ParserFactory.h"

namespace penguinTrace
{

  Exception::Exception(std::string reason, std::string file, int line)
  : reason(reason), file(file), line(line)
  {
    void* btraceBuf[BTRACE_DEPTH];
    int btraceSize = backtrace(btraceBuf, BTRACE_DEPTH);
    if (btraceSize > 0)
    {
      // Skip first to ignore this constructor
      for (int i = 1; i < btraceSize; i++)
      {
        bt.push_back(reinterpret_cast<uint64_t>(btraceBuf[i])-MIN_INSTR_BYTES);
      }
    }
  }

  const char * Exception::what() const throw()
  {
    printInfo();
    return "penguinTrace Exception";
  }

  void Exception::printInfo() const
  {
    std::stringstream s;
    s << "Exception: " << reason << std::endl;
    s << "@" << file << ":" << line;

    std::cout << std::endl << s.str() << std::endl << std::endl;

    std::stringstream btStrm;
    bool dwarfOk = false;
    auto l = std::unique_ptr<Logger>(new Logger(Logger::TRACE));
    std::stringstream exe;
    exe << "/proc/self/exe";
    auto p = object::ParserFactory::getParser(exe.str(), l->subLogger("PARSER"));
    dwarfOk = p->parse();
    auto info = std::unique_ptr<dwarf::Info>(new dwarf::Info(p.get(), l->subLogger("DWARF")));
    if (dwarfOk)
    {
      btStrm << std::endl << "Stacktrace:" << std::endl;
      for (auto it = bt.begin(); it != bt.end(); ++it)
      {
        uint64_t addr = *it;
        auto loc = info->locationByPC(addr, false);
        auto die = info->functionByPC(addr);

        if (die == nullptr)
        {
          break;
        }

        btStrm << "  ";
        if (die->hasName()) btStrm << die->getNamespace() << die->getName() << "(...)";
        else                btStrm << "<anon func>(...)";

        btStrm << "  [";

        if (loc.found())
        {
          btStrm << loc.filename();
          btStrm << ":" << loc.line();
        }
        else
        {
          btStrm << "?? " << HexPrint(addr, 2);
        }
        btStrm << "]";
        btStrm << std::endl;
      }
      std::cout << btStrm.str() << std::endl;
    }
  }

} /* namespace penguinTrace */
