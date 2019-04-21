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

#ifndef ARCH_H_
#define ARCH_H_

#include <cassert>
#include <string>
#include <stdint.h>

namespace penguinTrace
{
  const int MAX_INSTR_BYTES = 15;
  const int MIN_INSTR_BYTES = 1;

  const int ADDR_BYTES = 8;

  extern const char * PC_REG_NAME;
  extern const char * SP_REG_NAME;
  extern const char * FB_REG_NAME;

  std::string dwarfRegString(uint64_t reg);
}

#endif
