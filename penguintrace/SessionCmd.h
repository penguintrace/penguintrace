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
// penguinTrace Session Command Wrappers

#ifndef PENGUINTRACE_SESSIONCMD_H_
#define PENGUINTRACE_SESSIONCMD_H_

#include "../debug/Compiler.h"
#include "../debug/Stepper.h"

namespace penguinTrace
{
  class SessionManager;

  class SessionCmd
  {
    public:
      SessionCmd();
      virtual ~SessionCmd();
      virtual void run() = 0;
      virtual std::string repr() = 0;
  };

  class StepCmd : public SessionCmd
  {
    public:
      StepCmd(Stepper* s, StepType step) : stepper(s), step(step) {}
      virtual void run();
      virtual std::string repr();
    private:
      Stepper* stepper;
      StepType step;
  };

  class StdinCmd : public SessionCmd
  {
    public:
      StdinCmd(Stepper* s, std::string str) : stepper(s), string(str) {}
      virtual void run();
      virtual std::string repr();
    private:
      Stepper* stepper;
      std::string string;
  };

  class CompileCmd : public SessionCmd
  {
    public:
      CompileCmd(std::unique_ptr<Compiler> c,
          std::queue<CompileFailureReason>* f) :
          compiler(std::move(c)), failures(f)
      {
      }
      virtual void run();
      virtual std::string repr();
    private:
      std::unique_ptr<Compiler> compiler;
      std::queue<CompileFailureReason>* failures;
  };

  class UploadCmd : public SessionCmd
  {
    public:
      UploadCmd(std::string fname, std::string obj,
          std::queue<CompileFailureReason>* f) :
          filename(fname), obj(obj), failures(f)
      {
      }
      virtual void run();
      virtual std::string repr();
    private:
      std::string filename;
      std::string obj;
      std::queue<CompileFailureReason>* failures;
  };

  class ParseCmd : public SessionCmd
  {
    public:
      ParseCmd(SessionManager* sMgr, std::string sName, std::unique_ptr<ComponentLogger> log) :
        sMgr(sMgr), sessionName(sName), log(std::move(log))
      {
      }
      virtual void run();
      virtual std::string repr();
    private:
      SessionManager* sMgr;
      std::string sessionName;
      std::unique_ptr<ComponentLogger> log;
  };

} /* namespace penguinTrace */

#endif /* PENGUINTRACE_SESSIONCMD_H_ */
