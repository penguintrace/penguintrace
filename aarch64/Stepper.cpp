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

#include <linux/elf.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <string.h>

#include <sys/uio.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <asm/ptrace.h>
#include <fcntl.h>

namespace penguinTrace
{
  const uint32_t BREAKPOINT_WORD = 0xd4200000UL;
  const uint32_t RETURN_WORD     = 0xd65f0000UL;
  const uint32_t RETURN_MASK     = 0xfffffc1fUL;

  void Stepper::getRegisters()
  {
    user_pt_regs regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    registerValues["x0"]  = regs.regs[0];
    registerValues["x1"]  = regs.regs[1];
    registerValues["x2"]  = regs.regs[2];
    registerValues["x3"]  = regs.regs[3];
    registerValues["x4"]  = regs.regs[4];
    registerValues["x5"]  = regs.regs[5];
    registerValues["x6"]  = regs.regs[6];
    registerValues["x7"]  = regs.regs[7];
    registerValues["x8"]  = regs.regs[8];
    registerValues["x9"]  = regs.regs[9];
    registerValues["x10"]  = regs.regs[10];
    registerValues["x11"]  = regs.regs[11];
    registerValues["x12"]  = regs.regs[12];
    registerValues["x13"]  = regs.regs[13];
    registerValues["x14"]  = regs.regs[14];
    registerValues["x15"]  = regs.regs[15];
    registerValues["x16"]  = regs.regs[16];
    registerValues["x17"]  = regs.regs[17];
    registerValues["x18"]  = regs.regs[18];
    registerValues["x19"]  = regs.regs[19];
    registerValues["x20"]  = regs.regs[20];
    registerValues["x21"]  = regs.regs[21];
    registerValues["x22"]  = regs.regs[22];
    registerValues["x23"]  = regs.regs[23];
    registerValues["x24"]  = regs.regs[24];
    registerValues["x25"]  = regs.regs[25];
    registerValues["x26"]  = regs.regs[26];
    registerValues["x27"]  = regs.regs[27];
    registerValues["x28"]  = regs.regs[28];
    registerValues["x29"]  = regs.regs[29];
    registerValues["x30"]  = regs.regs[30];
    registerValues["sp"]   = regs.sp;
    registerValues["pc"]   = regs.pc;
    registerValues["pstate"] = regs.pstate;
  }

  uint64_t Stepper::getPC()
  {
    user_pt_regs regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    cachedPC = regs.pc;
    cachedPCvalid = true;
    return regs.pc;
  }

  void Stepper::setPC(uint64_t pc)
  {
    cachedPCvalid = false;
    user_pt_regs regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    regs.pc = pc;

    ptrace(PTRACE_SETREGSET, childPid, NT_PRSTATUS, &ioregs);
  }

  uint64_t Stepper::breakPC(uint64_t pc)
  {
    return pc;
  }

  bool Stepper::isSyscall(uint64_t pc)
  {
    uint32_t instr = (uint32_t)ptrace(PTRACE_PEEKTEXT, childPid, pc, nullptr);
    return (instr & 0xffe0001f) == 0xd4000001;
  }

  Stepper::Syscall Stepper::getSyscall()
  {
    uint64_t num = registerValues["x8"];
    std::vector<uint64_t> args;
    return Syscall(num, args);
  }


  void Stepper::errorSyscall(uint64_t pc)
  {
    setPC(pc+4);
    user_regs_struct regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    int ret1 = ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    regs.regs[0] = -1;

    int ret2 = ptrace(PTRACE_SETREGSET, childPid, NT_PRSTATUS, &ioregs);
    assert(ret1 == 0 && ret2 == 0);
  }

  bool Stepper::isLibraryCall(uint64_t pc)
  {
    bool found = false;
    bool inImage = false;
    uint64_t addr = 0;
    uint32_t instr = (uint32_t)ptrace(PTRACE_PEEKTEXT, childPid, pc, nullptr);
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
      int regNum = (instr & 0x3e0) >> 5;
      // TODO move to get reg function
      user_pt_regs regs;
      struct iovec ioregs;
      ioregs.iov_base = &regs;
      ioregs.iov_len = sizeof(regs);

      ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

      addr = regs.regs[regNum];
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

  uint64_t Stepper::callReturnAddr(uint64_t pc)
  {
    return pc+MIN_INSTR_BYTES;
  }

} /* namespace penguinTrace */
