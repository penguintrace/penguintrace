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
// penguinTrace Session

#ifndef PENGUINTRACE_SESSION_H_
#define PENGUINTRACE_SESSION_H_

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <functional>

#include "../object/Parser.h"
#include "../object/LineDisassembly.h"
#include "../debug/Stepper.h"
#include "../dwarf/Info.h"

#include "SessionCmd.h"

namespace penguinTrace
{

  struct Session
  {
    public:
      Session(std::string f, std::string name, std::function<void()> sc);
      std::queue<CompileFailureReason>* getCompileFailures();
      std::string executable();
      void setParser(std::unique_ptr<object::Parser> p, std::unique_ptr<ComponentLogger> l);
      object::Parser* getParser();
      void setStepper(std::unique_ptr<Stepper> s);
      Stepper* getStepper();
      dwarf::Info* getDwarfInfo();
      std::map<uint64_t, object::LineDisassembly>* getDisasmMap();
      bool pendingCommands();
      void enqueueCommand(std::unique_ptr<SessionCmd> c);
      void cleanup();
      void enqueueStop()
      {
        std::lock_guard<std::mutex> log(stopMutex);
        pendingStop = true;
      }
      bool toRemove();
      void setRemove();
      std::string timeString();
      std::string getName();
      void setSource(std::string s);
      std::string getSource();
      void setLang(std::string s);
      std::string getLang();
      void setBuffer(std::unique_ptr<std::vector<char> > ob);
      std::vector<char>* getBuffer();
    private:
      std::string execFilename;
      std::string name;
      std::queue<CompileFailureReason> compileFailures;
      std::unique_ptr<object::Parser> parser;
      std::unique_ptr<Stepper> stepper;
      std::map<uint64_t, object::LineDisassembly> disasmMap;
      std::mutex threadMutex;
      std::unique_ptr<std::thread> thread;
      std::queue<std::unique_ptr<SessionCmd> > taskQueue;
      std::unique_ptr<dwarf::Info> dwarfInfo;
      bool taskQueueRunning;
      std::mutex stopMutex;
      bool pendingStop;
      bool pendingRemove;
      std::function<void()> shutdownCallback;
      std::time_t timeCreated;
      std::time_t timeModified;
      std::string source;
      std::string lang;
      std::unique_ptr<std::vector<char> > objBuffer;

      void processTaskQueue();
      void processTaskQueueImpl();
  };
} /* namespace penguinTrace */

#endif /* PENGUINTRACE_SESSION_H_ */
