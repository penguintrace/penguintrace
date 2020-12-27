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

#include "../debug/Stepper.h"

#include <sys/types.h>
#include <sys/ptrace.h>

namespace penguinTrace
{

  int Stepper::traceMe() {
    return ptrace(PT_TRACE_ME, 0, nullptr, 0);
  }

  int Stepper::traceContinue() {
    return ptrace(PT_CONTINUE, 0, nullptr, 0);
  }

  int Stepper::traceStep() {
    return ptrace(PT_STEP, 0, nullptr, 0);
  }

  uint32_t Stepper::getChildWord(uint64_t addr) {
    // TODO
    return 0;   
  }

  long Stepper::getChildLong(uint64_t addr) {
    // TODO
    return 0;   
  }

  int Stepper::putChildLong(uint64_t addr, long data) {
    // TODO
    return -1;
  }

} /* namespace penguinTrace */
