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
// x86_64 specific debugging behaviour

#include "../debug/Stepper.h"

#include <linux/elf.h>

#include <sys/uio.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <sys/syscall.h>

namespace penguinTrace
{
  const uint32_t BREAKPOINT_WORD = 0xccUL;
  const uint32_t RETURN_WORD     = 0xc3UL;
  const uint32_t RETURN_MASK     = 0xffUL;

  void Stepper::getRegisters()
  {
  user_regs_struct regs;
  struct iovec ioregs;
  ioregs.iov_base = &regs;
  ioregs.iov_len = sizeof(regs);

  ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

  registerValues["r15"]    = regs.r15;
  registerValues["r14"]    = regs.r14;
  registerValues["r13"]    = regs.r13;
  registerValues["r12"]    = regs.r12;
  registerValues["rbp"]    = regs.rbp;
  registerValues["rbx"]    = regs.rbx;
  registerValues["r11"]    = regs.r11;
  registerValues["r10"]    = regs.r10;
  registerValues["r9"]     = regs.r9;
  registerValues["r8"]     = regs.r8;
  registerValues["rax"]    = regs.rax;
  registerValues["rcx"]    = regs.rcx;
  registerValues["rdx"]    = regs.rdx;
  registerValues["rsi"]    = regs.rsi;
  registerValues["rdi"]    = regs.rdi;
  registerValues["rip"]    = regs.rip;
  registerValues["cs"]     = regs.cs;
  registerValues["eflags"] = regs.eflags;
  registerValues["rsp"]    = regs.rsp;
  registerValues["ss"]     = regs.ss;
  }

  uint64_t Stepper::getPC()
  {
    user_regs_struct regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    cachedPC = regs.rip;
    cachedPCvalid = true;
    return regs.rip;
  }

  void Stepper::setPC(uint64_t pc)
  {
    cachedPCvalid = false;
    user_regs_struct regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    int ret1 = ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    regs.rip = pc;

    int ret2 = ptrace(PTRACE_SETREGSET, childPid, NT_PRSTATUS, &ioregs);
    assert(ret1 == 0 && ret2 == 0);
  }

  uint64_t Stepper::breakPC(uint64_t pc)
  {
    return pc-1;
  }

  bool Stepper::isSyscall(uint64_t pc)
  {
    uint32_t instr = (uint32_t)ptrace(PTRACE_PEEKTEXT, childPid, pc, nullptr);
    return (instr & 0xffff) == 0x050f;
  }

  Stepper::Syscall Stepper::getSyscall()
  {
    uint64_t num = registerValues["rax"];
    std::vector<uint64_t> args;
    return Syscall(num, args);
  }

  void Stepper::errorSyscall(uint64_t pc)
  {
    setPC(pc+2);
    user_regs_struct regs;
    struct iovec ioregs;
    ioregs.iov_base = &regs;
    ioregs.iov_len = sizeof(regs);

    int ret1 = ptrace(PTRACE_GETREGSET, childPid, NT_PRSTATUS, &ioregs);

    regs.rax = -1;

    int ret2 = ptrace(PTRACE_SETREGSET, childPid, NT_PRSTATUS, &ioregs);
    assert(ret1 == 0 && ret2 == 0);
  }

  bool Stepper::isLibraryCall(uint64_t pc)
  {
    return false;
  }

  uint64_t Stepper::callReturnAddr(uint64_t pc)
  {
    assert(false && "Can't calculate return addr in x86_64");
    return 0;
  }

} /* namespace penguinTrace */
