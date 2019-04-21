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
// Web Server

#ifndef SERVER_WEBSERVER_H_
#define SERVER_WEBSERVER_H_

#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../common/Common.h"
#include "../common/ComponentLogger.h"

#include "RouteTable.h"
#include "../penguintrace/SessionManager.h"

namespace penguinTrace
{
  namespace server
  {

    class WebServer
    {
      public:
        WebServer(std::unique_ptr<ComponentLogger> log);
        virtual ~WebServer();
        void run();
        void readRequest(int connection, std::string clientAddr);
        void printSessions();
        void killSessions();
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
        bool pendingRequest()
        {
          std::lock_guard<std::mutex> lock(requestQueueMutex);
          return !requestQueue.empty();
        }
        std::string addrStr(sockaddr* addr);
        std::string addrStr(sockaddr_in6* addr);
      private:
        void getServAddr(sockaddr_storage* addr);
        bool doBind(int socketDescriptor);
        std::unique_ptr<SessionManager> sessionMgr;
        std::unique_ptr<RouteTable> routes;
        std::unique_ptr<ComponentLogger> logger;
        bool running;
        std::mutex runningMutex;
        std::queue<Request> requestQueue;
        std::mutex requestQueueMutex;
    };

  } /* namespace server */
} /* namespace penguinTrace */

#endif /* SERVER_WEBSERVER_H_ */
