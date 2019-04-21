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
// x86_64 specific definitions

#include "Arch.h"

#include "../common/Exception.h"

namespace penguinTrace
{
  const char * PC_REG_NAME = "rip";
  const char * SP_REG_NAME = "rsp";
  const char * FB_REG_NAME = "rbp";

  std::string dwarfRegString(uint64_t reg)
  {
    switch (reg)
    {
      case 0:  return "rax";
      case 1:  return "rdx";
      case 2:  return "rcx";
      case 3:  return "rbx";
      case 4:  return "rsi";
      case 5:  return "rdi";
      case 6:  return "rbp";
      case 7:  return "rsp";
      case 8:  return "r8";
      case 9:  return "r9";
      case 10: return "r10";
      case 11: return "r11";
      case 12: return "r12";
      case 13: return "r13";
      case 14: return "r14";
      case 15: return "r15";
      case 16: return "ra";
    }

    throw Exception("Unknown register", __EINFO__);
  }
}

