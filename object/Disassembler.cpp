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

#include "Disassembler.h"

#include <cassert>
#include <sstream>
#include <functional>

#include "../common/Common.h"
#include "../common/StreamOperations.h"

namespace penguinTrace
{
  namespace object
  {

    int opInfoCallback(void* disInfo, uint64_t pc, uint64_t offset, uint64_t size, int tagType, void* tagBuf)
    {
      return 0;
    }

    const char* symbolLookupCallback(void * disInfo, uint64_t refVal, uint64_t* refType, uint64_t refPc, const char** refName)
    {
      std::stringstream s;
      s << penguinTrace::HexPrint(refVal, 1);

      Disassembler::SymbolAddrMap* symbMap = reinterpret_cast<Disassembler::SymbolAddrMap*>(disInfo);
      for (auto it = symbMap->begin(); it != symbMap->end(); ++it)
      {
        if (it->second->contains(refVal))
        {
          s << " <" << tryDemangle(it->second->getName());
          uint64_t diff = refVal-it->first;
          if (diff != 0) s << "+" << penguinTrace::HexPrint(diff, 1);
          s << ">";
          break;
        }
      }
      s.seekg(0, std::ios::end);
      size_t len = s.tellg();
      s.seekg(0, std::ios::beg);
      char * c = new char[len+1];
      s.read(c, len);
      c[len] = '\0';
      *refType = 0;
      *refName = c;
      return c;
    }

    Disassembler::Disassembler(SymbolAddrMap& symbols) :
        impl(Disassembler::Impl(symbols))
    {
    }

    Disassembler::~Disassembler()
    {
    }

    Disassembler::Impl::~Impl()
    {
#ifdef USE_LIBLLVM
      if (disRef != nullptr)
      {
        LLVMDisasmDispose(disRef);
      }
#else
#error "No disassembler"
#endif
    }

#ifdef USE_LIBLLVM
    std::string Disassembler::Impl::Disassemble(uint64_t pc, std::vector<uint8_t>& data, int* consumed)
    {
      if (data.size() == 0)
      {
        return "?";
      }
      if (disRef == nullptr)
      {
        char* triplec = LLVMGetDefaultTargetTriple();
        std::string triple(triplec);
        LLVMDisposeMessage(triplec);
        std::cout << "Machine Triple = " << triple << std::endl;

        LLVMInitializeNativeTarget();
        //LLVMInitializeAllTargetMCs();
        LLVMInitializeNativeDisassembler();

        disRef = LLVMCreateDisasm(triple.c_str(), reinterpret_cast<void*>(&symbols), 0,
                  opInfoCallback, symbolLookupCallback);
        LLVMSetDisasmOptions(disRef, 0);
        assert(disRef != nullptr);
      }

      char buffer[256];
      buffer[0] = '\0';
      size_t bytesConsumed = LLVMDisasmInstruction(disRef, data.data(), data.size(), pc, buffer, sizeof(buffer));

      std::stringstream hexStr;
      int bytesToConsume = (bytesConsumed != 0) ? 0 : ((data.size() < MIN_INSTR_BYTES) ? data.size() : MIN_INSTR_BYTES);

      for (int i = 0; i < bytesToConsume; ++i)
      {
        if (i != 0)
        {
          hexStr << " ";
        }
        hexStr << HexPrint(data[0], 2);
      }

      int wsCount = 0;
      for (unsigned i = 0; (bytesConsumed > 0) && (i < sizeof(buffer)); ++i)
      {
        if (buffer[i] == '\t' || buffer[i] == '\n' || buffer[i] == '\r'
            || buffer[i] == ' ')
        {
          wsCount++;
        }
        else
        {
          break;
        }
      }

      std::string codeDis = (bytesConsumed == 0) ? hexStr.str() : std::string(buffer+wsCount);

      if (consumed != nullptr)
      {
        *consumed = bytesToConsume + bytesConsumed;
      }

      return codeDis;
    }
#else
#error "No disassembler"
#endif

  } /* namespace object */
} /* namespace penguinTrace */
