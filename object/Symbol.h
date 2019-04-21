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
// Code Symbol Wrapper

#ifndef OBJECT_SYMBOL_H_
#define OBJECT_SYMBOL_H_

#include <string>

namespace penguinTrace
{
  namespace object
  {

    class Symbol
    {
      public:
        Symbol(std::string name, uint64_t addr, size_t size)
            : name(name), virtualAddr(addr), size(size)
        {

        }
        virtual ~Symbol();
        std::string getName()
        {
          return name;
        }
        uint64_t getAddress()
        {
          return virtualAddr;
        }
        size_t getSize()
        {
          return size;
        }
        bool contains(uint64_t addr)
        {
          return addr >= virtualAddr && addr < (virtualAddr+size);
        }
      private:
        std::string  name;
        uint64_t     virtualAddr;
        size_t       size;
    };

  } /* namespace object */
} /* namespace penguinTrace */

#endif /* OBJECT_SYMBOL_H_ */
