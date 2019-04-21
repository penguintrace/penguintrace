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
// LLVM Disassembler Wrapper

#ifndef OBJECT_DISASSEMBLER_H_
#define OBJECT_DISASSEMBLER_H_

#ifdef USE_LIBLLVM
#include <llvm-c/Core.h>
#include <llvm-c/Object.h>
#include <llvm-c/Disassembler.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/TargetMachine.h>
#endif

#include <vector>
#include <string>
#include <memory>
#include <map>

#include "Symbol.h"

namespace penguinTrace
{
  namespace object
  {

    class Disassembler
    {
      public:
        typedef std::shared_ptr<object::Symbol> SymbolPtr;
        typedef std::map<uint64_t, SymbolPtr> SymbolAddrMap;
        Disassembler(SymbolAddrMap& symbols);
        virtual ~Disassembler();
        std::string disassemble(uint64_t pc, std::vector<uint8_t>& data, int* consumed)
        {
          return impl.Disassemble(pc, data, consumed);
        }
      private:
        class Impl
        {
          public:
            Impl(SymbolAddrMap& symbols) : symbols(symbols), disRef(nullptr) { }
            ~Impl();
            std::string Disassemble(uint64_t pc, std::vector<uint8_t>& data, int* consumed);
          private:
            SymbolAddrMap& symbols;
#ifdef USE_LIBLLVM
            LLVMDisasmContextRef disRef;
#else
            void* disRef;
#endif
        };
        Impl impl;
    };

  } /* namespace object */
} /* namespace penguinTrace */

#endif /* OBJECT_DISASSEMBLER_H_ */
