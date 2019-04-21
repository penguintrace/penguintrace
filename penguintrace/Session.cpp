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

#include "Session.h"

#include "../common/Exception.h"

namespace penguinTrace
{
    Session::Session(std::string f, std::string name, std::function<void()> sc) :
        execFilename(f), name(name), parser(nullptr), stepper(nullptr),
        thread(nullptr), taskQueueRunning(false), pendingStop(false),
        pendingRemove(false), shutdownCallback(sc), source(""), lang(""),
        objBuffer(nullptr)
    {
      std::time_t t = std::time(nullptr);
      timeCreated = t;
      timeModified = t;
    }

    std::queue<CompileFailureReason>* Session::getCompileFailures()
    {
      return &compileFailures;
    }

    std::string Session::executable()
    {
      return execFilename;
    }

    std::string Session::getName()
    {
      return name;
    }

    void Session::setSource(std::string s)
    {
      source = s;
    }

    std::string Session::getSource()
    {
      return source;
    }

    void Session::setLang(std::string s)
    {
      lang = s;
    }

    std::string Session::getLang()
    {
      return lang;
    }

    void Session::setBuffer(std::unique_ptr<std::vector<char> > ob)
    {
      objBuffer = std::move(ob);
    }

    std::vector<char>* Session::getBuffer()
    {
      return objBuffer.get();
    }

    void Session::setParser(std::unique_ptr<object::Parser> p, std::unique_ptr<ComponentLogger> l)
    {
      parser = std::move(p);
      dwarfInfo = std::unique_ptr<dwarf::Info>(new dwarf::Info(parser.get(), std::move(l)));
    }

    object::Parser* Session::getParser()
    {
      return parser.get();
    }

    void Session::setStepper(std::unique_ptr<Stepper> s)
    {
      stepper = std::move(s);
    }

    Stepper* Session::getStepper()
    {
      return stepper.get();
    }

    dwarf::Info* Session::getDwarfInfo()
    {
      return dwarfInfo.get();
    }

    std::map<uint64_t, object::LineDisassembly>* Session::getDisasmMap()
    {
      return &disasmMap;
    }

    void Session::cleanup()
    {
      {
        std::lock_guard<std::mutex> lock(threadMutex);
        taskQueueRunning = false;
      }

      if (thread)
      {
        thread->join();
      }

      thread.reset();

      // Remove any leftover tasks
      while (!taskQueue.empty())
      {
        taskQueue.pop();
      }
      pendingRemove = true;
    }

    bool Session::toRemove()
    {
      std::lock_guard<std::mutex> lock(threadMutex);
      return pendingRemove;
    }

    void Session::setRemove()
    {
      std::lock_guard<std::mutex> lock(threadMutex);
      pendingRemove = true;
    }

    std::string Session::timeString()
    {
      std::lock_guard<std::mutex> lock(threadMutex);
      std::stringstream s;
      std::tm tmC = *std::localtime(&timeCreated);
      s << "Created: " << std::put_time(&tmC, "%T %F") << ", ";
      std::tm tmM = *std::localtime(&timeModified);
      s << "Modified: " << std::put_time(&tmM, "%T %F");
      return s.str();
    }

    bool Session::pendingCommands()
    {
      std::lock_guard<std::mutex> lock(threadMutex);

      return !taskQueue.empty();
    }

    void Session::enqueueCommand(std::unique_ptr<SessionCmd> c)
    {
      // Start a task on a separate thread for this session
      std::lock_guard<std::mutex> lock(threadMutex);

      timeModified = std::time(nullptr);

      if (!taskQueueRunning && thread)
      {
        thread.reset();
      }

      if (!thread)
      {
        taskQueueRunning = true;
        thread = std::unique_ptr<std::thread>(new std::thread(&Session::processTaskQueue, this));
      }

      taskQueue.push(std::move(c));
    }

    void Session::processTaskQueue()
    {
      try
      {
        processTaskQueueImpl();
      }
      catch (Exception& e)
      {
        shutdownCallback();
        std::this_thread::yield();
        e.printInfo();
      }
    }

    void Session::processTaskQueueImpl()
    {
      // All ptrace calls must be made in this thread as all calls must come
      //  from the same thread
      bool run = true;

      while (run)
      {
        std::unique_ptr<SessionCmd> task = nullptr;
        {
          std::lock_guard<std::mutex> lock(threadMutex);

          // Pop task and run
          if (!taskQueue.empty())
          {
            task = std::move(taskQueue.front());
          }
        }
        // Clear lock during long running task
        std::this_thread::yield();
        if (task)
        {
          task->run();
        }
        {
          std::lock_guard<std::mutex> lock(threadMutex);
          if (task)
          {
            taskQueue.pop();
          }

          run = taskQueueRunning;
        }
        if (taskQueue.empty())
        {
          std::lock_guard<std::mutex> log(stopMutex);
          if (pendingStop)
          {
            std::lock_guard<std::mutex> lock(threadMutex);
            run = false;
            // Should be empty but MT
            while (!taskQueue.empty())
            {
              taskQueue.pop();
            }
            pendingRemove = true;
          }
        }
        // Yield thread when mutex is out of scope
        std::this_thread::yield();
      }
    }

} /* namespace penguinTrace */
