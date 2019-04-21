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
// Disassembly with location information

#ifndef OBJECT_LINEDISASSEMBLY_H_
#define OBJECT_LINEDISASSEMBLY_H_

#include <stdint.h>
#include <string>

#include "../common/Common.h"
#include "../common/StreamOperations.h"

#include "Section.h"
#include "Symbol.h"

namespace penguinTrace
{
  namespace object
  {

    class LineDisassembly
    {
      public:
        LineDisassembly(uint8_t* d, size_t len, uint64_t pc, std::string codeDis)
        : length(len), pc(pc), codeDis(codeDis)
        {
          for (unsigned i = 0; i < MAX_INSTR_BYTES; i++)
          {
            if (i < len)
            {
              data[i] = *(d+i);
            }
            else
            {
              data[i] = 0;
            }
          }
        }
        ~LineDisassembly()
        {

        }
        uint8_t getData(int i)
        {
          assert(i < MAX_INSTR_BYTES);
          return data[i];
        }
        size_t getLength()
        {
          return length;
        }
        uint64_t getPC()
        {
          return pc;
        }
        std::string getCodeDis()
        {
          return codeDis;
        }
        std::string toString(penguinTrace::object::Section* section, penguinTrace::object::Symbol* symbol, int width)
        {
          std::stringstream s;

          if (section != nullptr)
          {
            s << "Section: ";
            s << section->getName();
            s << std::endl;
          }
          if (symbol != nullptr)
          {
            if (symbol->getName().length() > 0)
            {
              s << "Symbol <";
              s << symbol->getName();
              s << ">" << std::endl;
            }
          }
          s << penguinTrace::HexPrint(pc, 16) << ":";
          dualLoop(width, length, [&](int i)
          {
            s << " " << penguinTrace::HexPrint(getData(i), 2);
          },
                   [&]()
                   {
                     s << "     ";
                   });
          s << " ";
          dualLoop(width, length, [&](int i)
          {
            s << makePrintable(getData(i), ' ');
          },
                   [&]()
                   {
                     s << ' ';
                   });
          s << " " << codeDis;

          return s.str();
        }
      private:
        uint8_t     data[MAX_INSTR_BYTES];
        size_t      length;
        uint64_t    pc;
        //std::string dataDis;
        std::string codeDis;
    };

  } /* namespace object */
} /* namespace penguinTrace */

#endif /* OBJECT_LINEDISASSEMBLY_H_ */
