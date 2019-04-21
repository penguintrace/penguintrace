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
// penguinTrace App Wrapper

#ifndef PENGUINTRACE_PENGUINTRACE_H_
#define PENGUINTRACE_PENGUINTRACE_H_

#include <memory>
#include <mutex>

#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "../common/Logger.h"
#include "../server/WebServer.h"

namespace penguinTrace
{
  class PenguinTrace
  {
    public:
      virtual ~PenguinTrace();
      void run();
      bool isRunning()
      {
        std::lock_guard<std::mutex> lock(runningMutex);
        return running;
      }
      void stop()
      {
        std::lock_guard<std::mutex> lock(runningMutex);
        running = false;
      }
      void shutdown();
      static PenguinTrace* construct(int argc, char **argv)
      {
        if (instance == NULL)
        {
          instance = new PenguinTrace(argc, argv);
        }
        else
        {
          instance->logger->log(Logger::ERROR, "Trying to construct a second penguinTrace instance");
        }
        return instance;
      }
      static void staticShutdown(int signal)
      {
        if (instance != nullptr)
        {
          instance->shutdown();
        }
      }
      static void staticAbort(int signum)
      {
        if (instance != nullptr)
        {
          if (instance->original != nullptr)
          {
            // Try to restore terminal attributes
            // No error checking as already in an error condition
            tcsetattr(STDIN_FILENO, TCSANOW, instance->original);
          }
        }
        if (signum == SIGABRT)
        {
          signal(signum, SIG_DFL);
          kill(getpid(), signum);
        }
      }
    private:
      PenguinTrace(int argc, char **argv);
      void runServer();

      static PenguinTrace* instance;

      termios* original;
      std::unique_ptr<Logger> logger;
      std::unique_ptr<server::WebServer> server;
      bool running;
      bool ok;
      std::mutex runningMutex;
  };

} /* namespace penguinTrace */

#endif /* PENGUINTRACE_PENGUINTRACE_H_ */
