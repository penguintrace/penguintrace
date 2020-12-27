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
// penguinTrace Process Stepper

#include "Stepper.h"

#include <asm/ptrace.h>
#include <pty.h>

namespace penguinTrace
{

  int Stepper::traceMe() {
    return ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
  }

  int Stepper::traceContinue() {
    return ptrace(PTRACE_CONT, 0, nullptr, nullptr);
  }

  int Stepper::traceStep() {
    return ptrace(PTRACE_SINGLESTEP, 0, nullptr, nullptr);
  }

  uint32_t Stepper::getChildWord(uint64_t addr) {
    return (uint32_t)getChildLong(addr);
  }

  long Stepper::getChildLong(uint64_t addr) {
    return ptrace(PTRACE_PEEKTEXT, childPid, addr, nullptr);
  }

  int Stepper::putChildLong(uint64_t addr, long data) {
      ptrace(PTRACE_POKETEXT, childPid, addr, data);
  }

} /* namespace penguinTrace */
