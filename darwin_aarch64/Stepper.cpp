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

#include <sys/uio.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <fcntl.h>

namespace penguinTrace
{
  void Stepper::getRegisters()
  {
    // TODO
  }

  uint64_t Stepper::getPC()
  {
    // TODO
    return 0;
  }

  void Stepper::setPC(uint64_t pc)
  {
    // TODO
  }

  void Stepper::errorSyscall(uint64_t pc)
  {
    // TODO
  }

  bool Stepper::isLibraryCall(uint64_t pc)
  {
    bool found = false;
    bool inImage = false;
    uint64_t addr = 0;
    uint32_t instr = (uint32_t)getChildWord(pc);
    if ((instr & 0xfc000000) == 0x94000000) // BL
    {
      found = true;
      uint64_t offset = (instr & 0x03ffffff) << 2;
      if ((instr & 0x02000000) == 0x02000000)
      {
        offset |= 0xfffffffffc000000ULL;
      }
      addr = (uint64_t)((int64_t)pc + (int64_t)offset);
    }
    else if ((instr & 0xfffffc1f) == 0xd63f0000) // BLR
    {
      found = true;
      //int regNum = (instr & 0x3e0) >> 5;
      // TODO move to get reg function
      //user_pt_regs regs;
      //struct iovec ioregs;
      //ioregs.iov_base = &regs;
      //ioregs.iov_len = sizeof(regs);

      // TODO get regs
      //ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);
      //addr = regs.regs[regNum];
    }

    for (int i = 0; i < memoryRanges.size(); ++i)
    {
      bool inRange = (addr >= memoryRanges[i].start) && (addr < memoryRanges[i].end);
      if (inRange)
      {
        inImage = (memoryRanges[i].name.find(filename) != std::string::npos);
      }
    }

    // Also need to exclude calls into PLT (as they do not update link register)
    if (inImage)
    {
      auto pltItr = parser->getSectionNameMap().find(".plt");
      if (pltItr != parser->getSectionNameMap().end())
      {
        // Image relative PC
        inImage &= !pltItr->second->contains(addr);
      }
    }

    return found && !inImage;
  }

} /* namespace penguinTrace */
