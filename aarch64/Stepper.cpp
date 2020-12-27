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
// AArch64 specific debugging behaviour

#include "../debug/Stepper.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <string.h>

namespace penguinTrace
{
  const uint32_t BREAKPOINT_WORD = 0xd4200000UL;
  const uint32_t RETURN_WORD     = 0xd65f0000UL;
  const uint32_t RETURN_MASK     = 0xfffffc1fUL;

  uint64_t Stepper::breakPC(uint64_t pc)
  {
    return pc;
  }

  bool Stepper::isSyscall(uint64_t pc)
  {
    uint32_t instr = getChildWord(pc);
    return (instr & 0xffe0001f) == 0xd4000001;
  }

  Stepper::Syscall Stepper::getSyscall()
  {
    uint64_t num = registerValues["x8"];
    std::vector<uint64_t> args;
    return Syscall(num, args);
  }

  uint64_t Stepper::callReturnAddr(uint64_t pc)
  {
    return pc+MIN_INSTR_BYTES;
  }

} /* namespace penguinTrace */
