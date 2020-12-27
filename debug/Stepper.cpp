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
#include "StepperOS.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

#include <string.h>

#include <sys/uio.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "../common/StreamOperations.h"

#include "../dwarf/Frame.h"

#include "syscall.h"

namespace penguinTrace
{

  Stepper::Stepper(std::string f, object::Parser* p,
      std::unique_ptr<std::queue<std::string> > args,
      std::unique_ptr<ComponentLogger> l) :
      logger(std::move(l)), filename(f), parser(p),
      dwarfInfo(p, logger->subLogger("DWARF")), disassembler(p->getSymbolAddrMap()),
      argv(std::move(args)), childPid(0), done(false), stepCount(0),
      seenFirstSymbol(false), pMaster(0), oldStderr(-1), tempDir(""), cachedPC(0),
      cachedPCvalid(false), lastDisasm("?"), stepAgain(false),
      continueToEnd(false), hitBreakpoint(false)
  {
    logger->log(Logger::DBG, [&]() {
      std::stringstream s;
      s << "Creating Stepper for '" << f << "'";
      return s.str();
    });

    if (f != parser->getFilename())
    {
      logger->log(Logger::WARN, "Parser filename not the same as Stepper filename");
    }

    firstSymbols.push("main");
    firstSymbols.push("_start");

    auto dirs = Config::get(C_LIB_DIRS).String();
    auto dirList = split(dirs, ':');
    for (auto d : dirList)
    {
      auto pathCompList = split(d, '/');
      std::string prefix = "";
      for (int i = 0; i < pathCompList.size(); i++)
      {
        std::string p = pathCompList[i];
        if (!(p.size() == 0))
        {
          prefix += "/";
        }
        if (p.size() != 0) // Handle trailing slash
        {
          bool end = i == (pathCompList.size()-1);
          prefix += p;
          bool inList = false;
          for (int j = 0; j < mapDirs.size(); j++)
          {
            auto m = mapDirs[j];
            if (m.second == prefix)
            {
              inList = true;
              if (end && ! m.first)
              {
                mapDirs[j].first = true;
              }
            }
          }

          if (!inList)
          {
            mapDirs.push_back({end, prefix});
          }

        }
      }
    }

    logger->log(Logger::DBG, [&]() {
      std::stringstream s;

      s << "Directories to mount in sandbox:";

      for (auto m : mapDirs)
      {
        s << std::endl << m.second;
        if (m.first)
          {
            s << " [mounted]";
          }
      }

      return s.str();
    });
  }

  Stepper::~Stepper()
  {
    tidyIsolation();
  }

  bool Stepper::init()
  {
    if (!Config::get(C_USE_PTY).Bool())
    {
      int i;
      // Create communication pipes
      i = pipe(pToChild);
      if (i != 0)
      {
        logger->error(Logger::ERROR, "Failed to create pipe");
        return false;
      }
      i = pipe(pOutToParent);
      if (i != 0)
      {
        logger->error(Logger::ERROR, "Failed to create pipe");
        return false;
      }
      i = pipe(pErrToParent);
      if (i != 0)
      {
        logger->error(Logger::ERROR, "Failed to create pipe");
        return false;
      }

      // Make parent read pipes non-blocking
      int outFlags = fcntl(pOutToParent[PIPE_RD], F_GETFL);
      int ret1 = fcntl(pOutToParent[PIPE_RD], F_SETFL, outFlags | O_NONBLOCK);
      int errFlags = fcntl(pErrToParent[PIPE_RD], F_GETFL);
      int ret2 = fcntl(pErrToParent[PIPE_RD], F_SETFL, errFlags | O_NONBLOCK);
      assert(ret1 == 0 && ret2 == 0);
    }

    bool ok = isolateSetup();
    if (!ok)
    {
      return false;
    }

    oldStderr = dup(FD_STDERR);

    pid_t p;
    if (Config::get(C_USE_PTY).Bool())
    {
      p = forkpty(&pMaster, NULL, NULL, NULL);
    }
    else
    {
      p = fork();
    }

    if (p == -1)
    {
      logger->error(Logger::ERROR, "Failed to fork child process");
      return false;
    }
    else if (p == 0)
    {

      std::stringstream s;

      // Child process
      std::vector<char *> args;
      std::vector<char *> env;
      // Storage for the strings for the lifetime of this scope
      std::queue<std::string> args_store;

      args.push_back(const_cast<char*>(Config::get(C_EXEC_NAME).String().c_str()));
      while (!argv->empty())
      {
        args_store.push(argv->front());
        args.push_back(const_cast<char*>(args_store.back().c_str()));
        argv->pop();
      }
      args.push_back(nullptr);

      // Empty environment
      env.push_back(nullptr);

      if (Config::get(C_USE_PTY).Bool())
      {
        termios t;
        if (tcgetattr(STDIN_FILENO, &t) == 0)
        {
          // Always canonical mode as newline appended on stdin data
          t.c_lflag &= ~ECHO;
          t.c_cc[VMIN] = 0;
          t.c_cc[VTIME] = 0;

          if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0)
          {
            writeWrap(oldStderr, "Failed to set terminal attributes\n");
          }
        }
        else
        {
          writeWrap(oldStderr, "Failed to get terminal attributes\n");
        }
      }
      else
      {
        // In Child, connect to pipes
        dup2(pToChild[PIPE_RD], FD_STDIN);
        dup2(pOutToParent[PIPE_WR], FD_STDOUT);
        dup2(pErrToParent[PIPE_WR], FD_STDERR);

        // Close FDs that the subprocess shouldn't see
        // E.g. the pipes themselves
        close(pToChild[PIPE_RD]);
        close(pToChild[PIPE_WR]);
        close(pOutToParent[PIPE_RD]);
        close(pOutToParent[PIPE_WR]);
        close(pErrToParent[PIPE_RD]);
        close(pErrToParent[PIPE_WR]);
      }

      if (!isolateTracee())
      {
        writeWrap(oldStderr, "Failed to isolate tracee process\n");
        exit(-1);
      }

      close(oldStderr);
      traceMe();
      execve(filename.c_str(), args.data(), env.data());
      std::stringstream err;
      err << "Failed to exec '" << filename << "'. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, err.str());
      exit(-1);
    }

    close(oldStderr);

    if (Config::get(C_USE_PTY).Bool())
    {
      int outFlags = fcntl(pMaster, F_GETFL);
      int ret = fcntl(pMaster, F_SETFL, outFlags | O_NONBLOCK);
      assert(ret == 0 && "Failed to make pty non-blocking");
    }
    else
    {
      // Close ends of pipes not used in parent
      close(pToChild[PIPE_RD]);
      close(pOutToParent[PIPE_WR]);
      close(pErrToParent[PIPE_WR]);
    }

    childPid = p;
    logger->log(Logger::DBG, [&]() {
      std::stringstream s;

      s << "pid=" << p;

      return s.str();
    });
    return true;
  }

  uint64_t Stepper::step(StepType step)
  {
    uint64_t pc;
    pushToPipe();
    cachedPCvalid = false;
    hitBreakpoint = false;
    doWait();
    if (done)
    {
      logger->log(Logger::INFO, "Trace process finished");
      // Child exited/signalled

      readFromPipes();

      if (Config::get(C_USE_PTY).Bool())
      {
        close(pMaster);
      }
      else
      {
        close(pToChild[PIPE_WR]);
        close(pOutToParent[PIPE_RD]);
        close(pErrToParent[PIPE_RD]);
      }
      return 0;
    }
    // Set queued breakpoints
    {
      std::lock_guard<std::mutex> lock(breakpointMutex);
      for (auto it : breakpointsToAdd)
      {
        insertBreak(it);
      }
      for (auto it : breakpointsToRemove)
      {
        if (breakpointMap.find(it) != breakpointMap.end())
        {
          removeBreak(it);
        }
      }
      breakpointsToAdd.clear();
      breakpointsToRemove.clear();
    }
    // TODO merge with getting PC
    //   (should only call getregset once per-step)
    getRegisters();
    stepCount++;

    logger->log(Logger::TRACE, [&]() {
      std::stringstream s;
      s << "STEP:" << std::endl;
      s << "  seenFirstSymbol = " << (seenFirstSymbol ? "1" : "0");

      return s.str();
    });

    if (!seenFirstSymbol)
    {
      pc = firstStep();
      // Not advanced to next symbol, but will wait at next step until
      //  breakpoint is hit
      seenFirstSymbol = true;
    }
    else
    {
      pc = allStep(step);
      readFromPipes();
    }

    logger->log(Logger::TRACE, [&]() {
      std::stringstream s;
      s << "  pc = " << HexPrint(pc, 8);

      for (auto it : breakpointMap)
      {
         s << std::endl;
         s << "bkpt @ " << HexPrint(it.first, 8);
      }

      return s.str();
    });

    if (!continueToEnd && !Config::get(C_STEP_AFTER_MAIN).Bool())
    {
      // Try and detect returning from main
      auto func = dwarfInfo.functionByPC(pc);
      bool isMain = false;
      if (func != nullptr)
      {
        isMain = func->hasName() && (func->getName() == "main");
      }
      if (isFunctionReturn(pc) && isMain)
      {
        logger->log(Logger::DBG, [&]() {
          std::stringstream s;
          s << "Detected return from main @";
          s << HexPrint(pc, 8);

          return s.str();
        });
        continueToEnd = true;
      }
    }

    // REVISIT cached PC handling is incorrect in places
    cachedPC = pc;
    return pc;
  }

  bool Stepper::isFunctionReturn(uint64_t pc)
  {
    uint32_t instr = getChildWord(pc);
    return (instr & RETURN_MASK) == RETURN_WORD;
  }

  uint64_t Stepper::firstStep()
  {
    auto symItr = parser->getSymbolNameMap().end();
    while (symItr == parser->getSymbolNameMap().end() && !firstSymbols.empty())
    {
      symItr = parser->getSymbolNameMap().find(firstSymbols.front());

      firstSymbols.pop();
    }

    getMemoryRanges();

    if (symItr == parser->getSymbolNameMap().end())
    {
      logger->log(Logger::WARN, "No starting symbol found");
      cachedPC = 0;
    }
    else
    {
      std::stringstream s;
      s << "Using starting symbol '" << symItr->first << "'";
      logger->log(Logger::INFO, s.str());

      // Set breakpoint at symbol
      insertBreak(symItr->second->getAddress());
      // Continue
      traceContinue();
      cachedPC = symItr->second->getAddress();
    }

    return cachedPC;
  }

  uint64_t Stepper::allStep(StepType step)
  {
    uint64_t pc = getPC();
    uint64_t bkptPC = breakPC(pc);

    if (breakpointMap.find(bkptPC) != breakpointMap.end())
    {
      if (!stepAgain)
      {
        removeBreak(bkptPC);
        hitBreakpoint = true;
      }
      pc = bkptPC;
      // Setting PC after a breakpoint is only needed for architectures that do
      //  not restore the PC, but no harm in doing on all architectures
      setPC(pc);
    }

    if (stepAgain)
    {
      stepAgain = false;
    }

    if (pc != 0)
    {
      // May be able to optimise to only disasm when required
      //  (e.g. outside of image)
      // Although useful for debug, e.g. at breakpoints
      lastDisasm = disasmAtAddr(pc);
      std::stringstream s;
      s << HexPrint(pc, 16) << " -> " << lastDisasm;
      logger->log(Logger::INFO, s.str());

      getVariables(pc);
      getStack(pc);

      if (isSyscall(pc))
      {
        auto sys = getSyscall();
        auto sysName = getSyscallName(sys.num);
        std::stringstream s;
        s << "syscall " << sysName << " (";
        bool first = true;
        for (auto i : sys.args)
        {
          if (!first) s << ", ";
          else        first = false;

          s << HexPrint(i, 1);
        }
        s << ")";
        logger->log(Logger::INFO, s.str());

        if ((sysName == "fork") ||
            (sysName == "vfork") ||
            (sysName == "execve") ||
            (sysName == "execveat") ||
            (sysName == "clone") ||
            (sysName == "UNKNOWN"))
        {
          errorSyscall(pc);
          std::stringstream s;
          s << "Blocking syscall - " << sysName;
          logger->log(Logger::WARN, s.str());
        }
      }
      // Before stepping, will want to check current instruction
      // Not tracing dynamic symbols:
      // - If BL/BLR - set breakpoint at next PC
      // Or:
      // - If LDXR, have to allow to continue execution
      //            to include LDXR/STXR pair

      // TODO could also continue stepping until PC is in image again
      //  or statically link?
      // TODO continue once returned from main - how to know when returned from main?
      //   check for retq/ret instruction?
      if ((step == STEP_INSTR) && STEPPER_STEP_OVER_LIBRARY_CALLS && isLibraryCall(pc))
      {
        logger->log(Logger::DBG, [&]() {
          std::stringstream s;
          s << "Stepping over library call @";
          s << HexPrint(pc, 16);
          return s.str();
        });
        insertBreak(callReturnAddr(pc));
        int retval = traceContinue();
        assert(retval == 0);
      }
      else
      {
        if ((step == STEP_INSTR) ||
            ((step == STEP_LINE) & STEPPER_ALWAYS_SINGLE_STEP))
        {
          int retval = traceStep();
          if (retval != 0)
          {
            std::stringstream s;
            s << "Step failed, pid=" << childPid;
            logger->error(Logger::ERROR, s.str());
          }
          assert(retval == 0);
        }
        else if (step == STEP_LINE)
        {
          uint64_t nextPc = dwarfInfo.nextLinePC(pc);
          if (nextPc != 0)
          {
            insertBreak(nextPc);
            int retval = traceContinue();
            assert(retval == 0);
            stepAgain = true;
          }
          else
          {
            // If can't find line information - single step
            int retval = traceStep();
            assert(retval == 0);
          }
        }
        else if (step == STEP_CONT)
        {
          int retval = traceContinue();
          assert(retval == 0);
          stepAgain = true;
        }
        else
        {
          throw Exception("Unknown step type", __EINFO__);
        }
      }
    }
    else
    {
      done = true;
    }
    return pc;
  }

  uint64_t Stepper::getRegValue(std::string reg)
  {
    assert((registerValues.find(reg) != registerValues.end()) && "Register not found");
    uint64_t value = registerValues[reg];
    std::stringstream s;
    s << "Getting value of " << reg;
    s << " = " << HexPrint(value, 16);
    logger->log(Logger::TRACE, s.str());

    return value;
  }

  uint64_t Stepper::getUnwindRegValue(std::string reg)
  {
    if (unwindRegisterValues.find(reg) == unwindRegisterValues.end())
    {
      std::stringstream s;
      s << "Register '" << reg << "' not found";
      throw Exception(s.str(), __EINFO__);
    }
    uint64_t value = unwindRegisterValues[reg];
    std::stringstream s;
    s << "Getting value of " << reg;
    s << " = " << HexPrint(value, 16);
    logger->log(Logger::TRACE, s.str());

    return value;
  }

  uint64_t Stepper::getMemValue(uint64_t ptr)
  {
    long data = getChildLong(ptr);
    std::stringstream s;
    s << "Getting value at " << HexPrint(ptr, 16) << " = " << HexPrint(data, 16);
    logger->log(Logger::TRACE, s.str());

    return data;
  }

  void Stepper::getVariables(uint64_t pc)
  {
    std::list<dwarf::DIE*> objects;
    dwarfInfo.dataObjects(pc, objects);

    variableValues.clear();

    if (!dwarfInfo.inFunctionPrologue(pc))
    {
      for (auto it : objects)
      {
        std::string value = it->getValueString(std::bind(&Stepper::getRegValue, this, std::placeholders::_1),
                                               std::bind(&Stepper::getMemValue, this, std::placeholders::_1),
                                               pc);
        if (value.length() == 0) continue;
        std::stringstream s;
        s << "# " << StringPad(it->getType()->typeString(), 30, false);
        s << ' ' << StringPad(it->getName(), 24);
        s << " = " << value;
        logger->log(Logger::INFO, s.str());
        variableValues[it->getName()] = value;
      }
    }
  }

  bool Stepper::unwindRegs()
  {
    bool doneSp = false;
    bool donePc = false;
    bool doneFb = false;
    // Copy registers into unwind registers
    uint64_t currentPc = unwindRegisterValues[PC_REG_NAME];
    std::stringstream s;
    s << "Unwinding stack from: " << HexPrint(currentPc, 8);
    logger->log(Logger::TRACE, s.str());

    dwarf::FDE* fde = dwarfInfo.getFDE(currentPc);
    if (fde == nullptr)
    {
      std::stringstream err;
      err << "Failed to get FDE for " << HexPrint(currentPc, 1);
      logger->log(Logger::WARN, err.str());
      return false;
    }
    assert(fde != nullptr);
    assert(fde->contains(currentPc));
    auto unwindInfo = fde->unwindInfo(currentPc);

    // TODO move logic to FDE?
    for (auto it : *unwindInfo)
    {
      bool cfa = it.first == " ";
      // CFA defined as " "
      std::stringstream s;
      s << "reg: " << (cfa ? "CFA": it.first);
      s << " - rule: " << it.second.toString(cfa);
      logger->log(Logger::TRACE, s.str());

      std::string regName = it.first;
      if (it.first == " ")
      {
        regName = SP_REG_NAME;
        doneSp = true;
      }
      else if (it.first == "ra")
      {
        regName = PC_REG_NAME;
        donePc = true;
      }
      else if (it.first == FB_REG_NAME)
      {
        doneFb = true;
      }

      uint64_t newValue = unwindRegisterValues[regName];

      if (it.second.getType() == dwarf::RegRule::SAME)
      {

      }
      else if (it.second.getType() == dwarf::RegRule::EXPR)
      {
        // TODO expr unwind
        throw Exception("Expression unwind unsupported", __EINFO__);
      }
      else if (it.second.getType() == dwarf::RegRule::OFFSET)
      {
        assert(!cfa);
        std::string regFrom = SP_REG_NAME;
        uint64_t baseValue = unwindRegisterValues[regFrom];
        if (it.second.op().isSigned())
        {
          int64_t offset = it.second.op().getSInt();
          newValue = getMemValue(baseValue+offset);
        }
        else
        {
          uint64_t offset = it.second.op().getInt();
          newValue = getMemValue(baseValue+offset);
        }
      }
      else if (it.second.getType() == dwarf::RegRule::VAL_OFFSET)
      {
        std::string regFrom = cfa ? it.second.regName() : SP_REG_NAME;
        uint64_t baseValue = unwindRegisterValues[regFrom];
        if (it.second.op().isSigned())
        {
          int64_t offset = it.second.op().getSInt();
          newValue = baseValue+offset;
        }
        else
        {
          uint64_t offset = it.second.op().getInt();
          newValue = baseValue+offset;
        }
      }
      else
      {
        throw Exception("Unhandled unwind rule", __EINFO__);
      }

      if (it.first == "ra")
      {
        // DWARF recommends subtracting one instruction from return address
        //  to get previous context
        newValue -= MIN_INSTR_BYTES;
      }

      unwindRegisterValues[regName] = newValue;
    }

    return doneSp && donePc && doneFb;
  }

  void Stepper::getStack(uint64_t pc)
  {
    stackTrace.clear();
    std::list<dwarf::DIE*> params;
    dwarfInfo.formalParams(pc, params);

    dwarf::DIE* function = dwarfInfo.functionByPC(pc);

    if (function)
    {
      std::stringstream s;
      if (function->hasType())
      {
        s << function->getType()->typeString() << " ";
      }
      if (function->hasName())
      {
        s << function->getName();
      }
      else
      {
        s << "???";
      }
      s << "(";
      if (!dwarfInfo.inFunctionPrologue(pc))
      {
        bool first = true;
        for (auto it : params)
        {
          if (!first) s << ", ";
          else first = false;
          s << it->getType()->typeString() << " ";
          s << it->getName() << "=";
          std::string value = it->getValueString(std::bind(&Stepper::getRegValue, this, std::placeholders::_1),
                                                 std::bind(&Stepper::getMemValue, this, std::placeholders::_1),
                                                 pc);
          s << value;
        }
      }
      else
      {
        s << "?";
      }
      s << ")";

      stackTrace.push_back(s.str());

      std::list<dwarf::DIE*> objects;
      dwarfInfo.dataObjects(pc, objects);
      for (auto it : objects)
      {
        if (it->isFormalParam()) continue;
        std::stringstream s;
        s << it->getType()->typeString() << " ";
        s << it->getName() << " = ";

        std::string value = it->getValueString(std::bind(&Stepper::getRegValue, this, std::placeholders::_1),
                                               std::bind(&Stepper::getMemValue, this, std::placeholders::_1),
                                               pc);
        s << value;
        stackTrace.push_back(s.str());
      }

      // Set up register values for unwinding
      unwindRegisterValues = registerValues;

      std::string currentFunction = function->hasName() ? function->getName() : "???";
      dwarf::DIE* nextFunction = nullptr;

      bool unwindOk = false;
      uint64_t nxtPc = 0;
      if (currentFunction != "main")
      {
        unwindOk = unwindRegs();
        nxtPc = unwindRegisterValues[PC_REG_NAME];
        std::stringstream s;
        s << "Searching for next function at:" << HexPrint(nxtPc, 8);
        logger->log(Logger::TRACE, s.str());
        nextFunction = dwarfInfo.functionByPC(nxtPc);
      }

      while (currentFunction != "main" && nextFunction && unwindOk)
      {
        std::stringstream s;
        currentFunction = nextFunction->hasName() ? nextFunction->getName() : "???";

        if (nextFunction->hasType())
        {
          s << nextFunction->getType()->typeString() << " ";
        }
        s << currentFunction << "(";
        if (!dwarfInfo.inFunctionPrologue(unwindRegisterValues[PC_REG_NAME]))
        {
          std::list<dwarf::DIE*> params;
          dwarfInfo.formalParams(unwindRegisterValues[PC_REG_NAME], params);
          bool first = true;
          for (auto it : params)
          {
            if (!first) s << ", ";
            else first = false;
            s << it->getType()->typeString() << " ";
            s << it->getName() << "=";

            std::string value = it->getValueString(std::bind(&Stepper::getUnwindRegValue, this, std::placeholders::_1),
                                                   std::bind(&Stepper::getMemValue,       this, std::placeholders::_1),
                                                   unwindRegisterValues[PC_REG_NAME]);
            s << value;
          }

          s << ")";
          stackTrace.push_back(s.str());

          std::list<dwarf::DIE*> objects;
          dwarfInfo.dataObjects(unwindRegisterValues[PC_REG_NAME], objects);
          for (auto it : objects)
          {
            if (it->isFormalParam()) continue;
            std::stringstream s;
            s << it->getType()->typeString() << " ";
            s << it->getName() << " = ";

            std::string value = it->getValueString(std::bind(&Stepper::getUnwindRegValue, this, std::placeholders::_1),
                                                   std::bind(&Stepper::getMemValue, this, std::placeholders::_1),
                                                   unwindRegisterValues[PC_REG_NAME]);
            s << value;
            stackTrace.push_back(s.str());
          }
        }

        unwindOk = unwindRegs();
        nextFunction = dwarfInfo.functionByPC(unwindRegisterValues[PC_REG_NAME]);
      }
    }
    for (auto it = stackTrace.begin(); it != stackTrace.end(); ++it)
    {
      logger->log(Logger::DBG, *it);
    }
  }

  void Stepper::insertBreak(uint64_t addr)
  {
    if (breakpointMap.find(addr) == breakpointMap.end())
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Inserting breakpoint @" << penguinTrace::HexPrint(addr, 16);
        return s.str();
      });
      long prevInstr = getChildLong(addr);
      breakpointMap[addr] = prevInstr;
      int ret = putChildLong(addr, BREAKPOINT_WORD);
      if (MIN_INSTR_BYTES == 1)
      {
        long newInstr = getChildLong(addr & 0xffffFFFFffffFFFC);
        assert(((newInstr >> ((addr & 0x3)*8)) & 0xff) == BREAKPOINT_WORD && "Breakpoint instruction not written");
      }
      if (ret != 0) perror("bkpt");
      assert(ret == 0 && "Failed to set breakpoint");
    }
    else
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Already have breakpoint @" << penguinTrace::HexPrint(addr, 16);
        return s.str();
      });
    }
  }

  bool Stepper::queueBreakpoint(uint64_t a, bool line)
  {
    std::lock_guard<std::mutex> lock(breakpointMutex);

    uint64_t addr = a;
    if (line)
    {
      auto loc = dwarfInfo.exactLocationByLine(a);
      if (loc.found())
      {
        addr = loc.pc();
      }
      else
      {
        return false;
      }
    }

    bool found = false;
    for (auto r : memoryRanges)
    {
      if ((addr >= r.start) && (addr < r.end))
      {
        found = true;

        logger->log(Logger::TRACE, [&]() {
          std::stringstream s;
          s << "Queueing breakpoint @" << HexPrint(addr, 8);
          s << " (" << r.name << ")";
          return s.str();
        });
      }
    }
    if (found) breakpointsToAdd.insert(addr);
    return found;
  }

  void Stepper::removeBreakpoint(uint64_t a, bool line)
  {
    std::lock_guard<std::mutex> lock(breakpointMutex);

    uint64_t addr = a;
    if (line)
    {
      auto loc = dwarfInfo.exactLocationByLine(a);
      if (loc.found())
      {
        addr = loc.pc();
      }
      else
      {
        return;
      }
    }

    auto it = breakpointsToAdd.find(addr);
    if (it != breakpointsToAdd.end())
    {
      breakpointsToAdd.erase(addr);
    }

    breakpointsToRemove.insert(addr);
  }

  void Stepper::removeBreak(uint64_t addr)
  {
    logger->log(Logger::DBG, [&]() {
      std::stringstream s;
      s << "Removing breakpoint @" << penguinTrace::HexPrint(addr, 16);
      return s.str();
    });

    auto itr = breakpointMap.find(addr);
    assert(itr != breakpointMap.end());

    int ret = putChildLong(addr, itr->second);
    assert(ret == 0);
    breakpointMap.erase(addr);
  }

  bool Stepper::doWait()
  {
    int status;
    pid_t p = waitpid(childPid, &status, 0);
    if (p == -1 && errno == ECHILD)
    {
      // Can get ECHILD if child already exited
      logger->error(Logger::ERROR, "ECHILD");
      done = true;
      return false;
    }
    else if (p != childPid)
    {
      perror("waitpid");
      logger->error(Logger::ERROR, "Waiting for child process failed");
    }
    assert(p == childPid);

    done = WIFEXITED(status) || WIFSIGNALED(status);
    if (WIFEXITED(status))
    {
      int retCode = WEXITSTATUS(status);
      std::stringstream s;
      s << "Exited (" << retCode << ")";
      logger->log(retCode == 0 ? Logger::INFO : Logger::WARN, s.str());
      assert(done);
    }
    else if (WIFSIGNALED(status))
    {
      std::stringstream s;
      s << "Killed by signal: (" << WTERMSIG(status) << ") ";
      s << strsignal(WTERMSIG(status));
      logger->log(Logger::ERROR, s.str());
      assert(done);
    }
    return !done;
  }

  std::string Stepper::disasmAtAddr(uint64_t addr)
  {
    int bytesToRead = MAX_INSTR_BYTES;
    bool error = false;
    std::vector<uint8_t> instrBytes;

    while (bytesToRead > 0 && !error)
    {
      errno = 0;
      long data = getChildLong(addr);
      if (errno == 0)
      {
        for (unsigned i = 0; i < sizeof(data); ++i)
        {
          uint8_t byte = (data >> (i*8)) & 0xff;
          instrBytes.push_back(byte);
        }
      }
      else
      {
        error = true;
      }
      bytesToRead -= sizeof(data);
    }

    return disassembler.disassemble(addr, instrBytes, nullptr);
  }

  void Stepper::getMemoryRanges()
  {
	  // Get mem offset
    // Not used for PC offset now not compiled as PIE, but can use
    //  to determine extent of image
    std::stringstream s;
    s << "/proc/" << childPid << "/maps";
    std::ifstream memmap(s.str());

    std::string line;
    while ( std::getline(memmap, line) )
    {
      std::stringstream rangeStream(line);
      uint64_t start;
      uint64_t end;
      char c;
      std::string tmpName;
      std::string name;
      rangeStream >> std::hex >> start;
      rangeStream >> c;
      rangeStream >> std::hex >> end;
      std::getline(rangeStream, tmpName);
      auto tmpNames = split(tmpName, ' ');
      name = tmpNames.size() > 0 ? tmpNames[tmpNames.size()-1] : "";
      memoryRanges.push_back(Range(name, start, end));
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Map '" << name << "' @";
        s << HexPrint(start, 8) << "-";
        s << HexPrint(end, 8);
        return s.str();
      });
    }
  }

  void Stepper::readFromPipes()
  {
    // Won't get output until pipe buffer fills unless using pseudo-terminal
    std::stringstream outstream;

    if (Config::get(C_USE_PTY).Bool())
    {
      readFile(pMaster, &outstream);
    }
    else
    {
      readFile(pOutToParent[PIPE_RD], &outstream);
      readFile(pErrToParent[PIPE_RD], &outstream);
    }

    std::string line;
    while ( std::getline(outstream, line) )
    {
      // Remove all carriage returns
      // (Until web UI emulates a VT100)
      line = replaceAll(line, "\r", "");
      logger->log(Logger::TRACE, "stdout: "+line+">EOF");
      stdout.push(line);
    }
  }

  void Stepper::readFile(int fd, std::stringstream* stream)
  {
    int n;
    char buffer[256];
    int err;

    do
    {
      errno = 0;
      n = read(fd, buffer, sizeof(buffer));
      err = errno;
      if (n > 0)
      {
        stream->write(buffer, n);
      }
    }
    while (n > 0 && err == 0);

    if (err != 0 && !(errorTryAgain(err) || (err == EIO)))
    {
      std::stringstream s;
      s << "Error reading from traced process #" << fd;
      logger->error(Logger::ERROR, s.str());
    }
  }

  void Stepper::pushToPipe()
  {
    int fd;
    if (Config::get(C_USE_PTY).Bool())
    {
      fd = pMaster;
    }
    else
    {
      fd = pToChild[PIPE_WR];
    }

    while (!stdin.empty())
    {
      auto str = stdin.front()+"\n";

      if (!writeWrap(fd, str))
      {
        logger->error(Logger::ERROR, "Failed to write to STDIN");
      }

      stdin.pop();
    }
  }

  bool Stepper::singleStepDone(StepType step)
  {
    if (step == STEP_INSTR)
    {
      return true;
    }
    else if (step == STEP_CONT)
    {
      return done || hitBreakpoint;
    }
    else if (step == STEP_LINE)
    {
      auto loc = dwarfInfo.exactLocationByPC(cachedPC);
      // Sometimes get line 0 - don't want to stop here
      bool lineOk = loc.found() && (loc.line() != 0);
      return done || hitBreakpoint || lineOk;
    }
    else
    {
      throw Exception("Unknown step type", __EINFO__);
    }
  }

} /* namespace penguinTrace */
