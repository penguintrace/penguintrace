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
// LLVM Object File Parser wrapper

#ifndef OBJECT_LLVMPARSER_H_
#define OBJECT_LLVMPARSER_H_
#ifdef USE_LIBLLVM

#include <memory>

#include "Parser.h"

#include "../common/ComponentLogger.h"

#include <llvm-c/Core.h>
#include <llvm-c/Object.h>
#include <llvm-c/Disassembler.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/TargetMachine.h>

namespace penguinTrace
{
  namespace object
  {

    class LLVMParser : public Parser
    {
      public:
        LLVMParser(std::string filename, std::unique_ptr<ComponentLogger> l);
        virtual ~LLVMParser();
        bool parse();
        void close();
      private:
        std::unique_ptr<ComponentLogger> logger;
        LLVMObjectFileRef objFile;
    };

  } /* namespace object */
} /* namespace penguinTrace */

#endif
#endif /* OBJECT_LLVMPARSER_H_ */
