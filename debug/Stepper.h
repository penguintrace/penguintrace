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

#ifndef DEBUG_STEPPER_H_
#define DEBUG_STEPPER_H_

#include <queue>
#include <set>
#include <vector>

#include "../common/ComponentLogger.h"
#include "../object/Parser.h"
#include "../object/Section.h"
#include "../object/Symbol.h"

#include "../object/Disassembler.h"

#include "../dwarf/Info.h"

namespace penguinTrace
{
  extern const uint32_t BREAKPOINT_WORD;
  extern const uint32_t RETURN_WORD;
  extern const uint32_t RETURN_MASK;

  class Stepper
  {
    public:
      Stepper(std::string f, object::Parser* p,
          std::unique_ptr<std::queue<std::string> > args,
          std::unique_ptr<ComponentLogger> l);
      virtual ~Stepper();
      bool init();
      uint64_t step(StepType step);
      bool active()
      {
        return !done;
      }
      bool shouldStepAgain()
      {
        return stepAgain;
      }
      bool shouldContinueToEnd()
      {
        return continueToEnd;
      }
      bool singleStepDone(StepType step);
      bool hitBreak()
      {
        return hitBreakpoint;
      }
      int getStepCount()
      {
        return stepCount;
      }
      std::string getLastDisasm()
      {
        return lastDisasm;
      }
      uint64_t getLastPC()
      {
        return cachedPC;
      }
      std::map<std::string, uint64_t>& getRegValues()
      {
        return registerValues;
      }
      std::map<std::string, std::string>& getVarValues()
      {
        return variableValues;
      }
      std::list<std::string>& getStackTrace()
      {
        return stackTrace;
      }
      void sendStdin(std::string in)
      {
        stdin.push(in);
      }
      std::queue<std::string>& getStdout()
      {
        return stdout;
      }
      bool isDone()
      {
        return done;
      }
      bool queueBreakpoint(uint64_t addr, bool line);
      void removeBreakpoint(uint64_t addr, bool line);
      std::map<uint64_t, long>& getBreakpoints()
      {
        std::lock_guard<std::mutex> lock(breakpointMutex);
        return breakpointMap;
      }
      std::set<uint64_t>& getPendingBreakpoints()
      {
        std::lock_guard<std::mutex> lock(breakpointMutex);
        return breakpointsToAdd;
      }
    private:
      struct Range {
        Range() : name(""), start(0), end(0) { }
        Range(std::string name, uint64_t start, uint64_t end)
        : name(name), start(start), end(end) { }
        std::string name;
        uint64_t start;
        uint64_t end;
      };
      struct Syscall {
        Syscall() : num(0) { }
        Syscall(uint64_t n, std::vector<uint64_t> a) : num(n), args(a) { }
        uint64_t num;
        std::vector<uint64_t> args;
      };
      uint64_t firstStep();
      uint64_t allStep(StepType step);
      bool doWait();
      bool unwindRegs();
      uint64_t getRegValue(std::string reg);
      uint64_t getUnwindRegValue(std::string reg);
      uint64_t getMemValue(uint64_t ptr);
      void getVariables(uint64_t pc);
      void getStack(uint64_t pc);
      uint64_t getPC();
      void getRegisters();
      void setPC(uint64_t pc);
      uint64_t breakPC(uint64_t pc);
      void insertBreak(uint64_t addr);
      void removeBreak(uint64_t addr);
      std::string disasmAtAddr(uint64_t addr);
      bool isLibraryCall(uint64_t pc);
      bool isFunctionReturn(uint64_t pc);
      bool isSyscall(uint64_t pc);
      Syscall getSyscall();
      void errorSyscall(uint64_t pc);
      uint64_t callReturnAddr(uint64_t pc);
      void getMemoryRanges();
      void readFromPipes();
      void pushToPipe();
      void readFile(int fd, std::stringstream* stream);
      bool isolateSetup();
      bool isolateTracee();
      void tidyIsolation();
      bool writeToProc(std::string fname, std::string contents);

      // OS specific
      int traceMe();
      int traceContinue();
      int traceStep();
      uint32_t getChildWord(uint64_t addr);
      long getChildLong(uint64_t addr);
      int putChildLong(uint64_t addr, long data);

      std::unique_ptr<ComponentLogger> logger;

      // Executable Info
      std::string filename;
      object::Parser* parser;
      dwarf::Info dwarfInfo;
      object::Disassembler disassembler;
      std::unique_ptr<std::queue<std::string> > argv;

      // Child process information
      pid_t childPid;
      bool done;
      int stepCount;
      std::queue<std::string> firstSymbols;
      bool seenFirstSymbol;
      std::map<uint64_t, long> breakpointMap;
      std::set<uint64_t> breakpointsToAdd;
      std::set<uint64_t> breakpointsToRemove;

      // Communication pipes
      int pToChild[PIPE_NUM];
      int pOutToParent[PIPE_NUM];
      int pErrToParent[PIPE_NUM];
      int pMaster;
      int oldStderr;
      std::string tempDir;
      std::queue<std::string> stdin;
      std::queue<std::string> stdout;

      std::vector<std::pair<bool, std::string> > mapDirs;

      // Cached values
      uint64_t cachedPC;
      bool cachedPCvalid;
      std::string lastDisasm;
      std::vector<Range> memoryRanges;
      // TODO different size registers
      std::map<std::string, uint64_t> registerValues;
      std::map<std::string, uint64_t> unwindRegisterValues;
      std::map<std::string, std::string> variableValues;
      std::list<std::string> stackTrace;
      bool stepAgain;
      bool continueToEnd;
      bool hitBreakpoint;
      std::mutex breakpointMutex;

  };

} /* namespace penguinTrace */

#endif /* DEBUG_STEPPER_H_ */
