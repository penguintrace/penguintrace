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

#include "PenguinTrace.h"

#include <thread>
#include <chrono>

namespace penguinTrace
{
  PenguinTrace* PenguinTrace::instance = nullptr;

  PenguinTrace::PenguinTrace(int argc, char **argv)
      : original(nullptr), running(true)
  {
    Config::showHelp(C_C_COMPILER_BIN);
    Config::showHelp(C_CXX_COMPILER_BIN);
    Config::showHelp(C_SERVER_PORT);
    Config::showHelp(C_SERVER_GLOBAL);
    Config::showHelp(C_SERVER_IPV6);
    Config::showHelp(C_SINGLE_SESSION);
    Config::showHelp(C_TEMP_FILE_TPL);

    ok = Config::parse(argc, argv, false, "penguinTrace web server");

    if (ok)
    {
      logger = std::unique_ptr<Logger>(new Logger(Logger::INFO));

      server = std::unique_ptr<server::WebServer>(
          new server::WebServer(logger->subLogger("SERVER")));

      logger->log(Logger::INFO, Config::print());
    }
  }

  PenguinTrace::~PenguinTrace()
  {
    if (original != nullptr) delete original;
    instance = nullptr;
  }

  void PenguinTrace::run()
  {
    if (!ok) return;
    // Set Terminal Attributes
    termios t;
    original = new termios;
    if (tcgetattr(STDIN_FILENO, &t) == 0)
    {
      *original = t;
      t.c_lflag &= ~ICANON;
      t.c_lflag &= ~ECHO;
      t.c_cc[VMIN] = 0;
      t.c_cc[VTIME] = 0;

      if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0)
      {
        logger->error(Logger::WARN, "Failed to set terminal attributes");
      }
    }
    else if (errno == ENOTTY)
    {
      logger->log(Logger::INFO, "Running without a terminal");
    }
    else
    {
      logger->error(Logger::WARN, "Failed to get terminal attributes");
    }

    // Handle Ctrl-C gracefully
    signal(SIGINT, &PenguinTrace::staticShutdown);
    // Try to restore the terminal on segfault/assertion failure
    signal(SIGABRT, &PenguinTrace::staticAbort);
    signal(SIGSEGV, &PenguinTrace::staticAbort);

    // Start Server thread
    std::thread server_thread(&PenguinTrace::runServer, this);

    if (running)
    {
      std::stringstream s;
      s << "Server Running" << std::endl;
      s << "  Commands:" << std::endl;
      s << "   q - Shutdown server" << std::endl;
      s << "   l - List active sessions" << std::endl;
      s << "   k - Kill all active sessions" << std::endl;
      logger->log(Logger::INFO, s.str());
    }

    while (running)
    {
      char c;
      if (read(STDIN_FILENO, &c, 1) > 0)
      {
        // Mask input to lowercase
        c = c | 0x20;
        switch (c)
        {
          case 'q':
            shutdown();
            break;
          case 'l':
            server->printSessions();
            break;
          case 'k':
            server->killSessions();
            break;
          default:
            break;
        }
      }

      logger->printMessages();

      // Sleep to return control to server thread
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Ensure everything is shutdown if closed by signal
    shutdown();

    // Server has stopped, just need to wait for thread
    server_thread.join();

    // Print any remaining messages
    logger->printMessages();

    if (original != nullptr)
    {
      if (tcsetattr(STDIN_FILENO, TCSANOW, original) != 0)
      {
        logger->error(Logger::WARN, "Failed to restore terminal attrs");
      }
    }
  }

  void PenguinTrace::shutdown()
  {
    if (server->isRunning())
    {
      logger->log(Logger::INFO, "Stopping server...");
      server->stop();
    }
  }

  void PenguinTrace::runServer()
  {
    server->run();
    // Signal stop back to main thread
    stop();
  }

} /* namespace penguinTrace */
