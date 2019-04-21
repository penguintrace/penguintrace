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

#include "WebServer.h"

#include <algorithm>
#include <iterator>
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>

namespace penguinTrace
{
  namespace server
  {

    WebServer::WebServer(std::unique_ptr<ComponentLogger> log)
        : logger(std::move(log)), running(true)
    {
      auto shutdownCallback = std::bind(&WebServer::stop, this);
      sessionMgr = std::unique_ptr<SessionManager>(
          new SessionManager(shutdownCallback, logger->subLogger("SESSION")));
      routes = std::unique_ptr<RouteTable>(
          RouteTable::getRouteTable(sessionMgr.get(), logger->subLogger("API")));

      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Route Table:" << std::endl;
        routes->printRouteTable(&s, 1);
        return s.str();
      });
    }

    WebServer::~WebServer()
    {
    }

    void WebServer::printSessions()
    {
      sessionMgr->printSessions();
    }

    void WebServer::killSessions()
    {
      sessionMgr->endAllSessions();
    }

    void WebServer::getServAddr(sockaddr_storage* addr)
    {
      if (Config::get(C_SERVER_IPV6).Bool())
      {
        sockaddr_in6* a = reinterpret_cast<sockaddr_in6*>(addr);
        a->sin6_family = AF_INET6;
        if (Config::get(C_SERVER_GLOBAL).Bool())
        {
          a->sin6_addr = in6addr_any;
        }
        else
        {
          a->sin6_addr = in6addr_loopback;
        }
        a->sin6_port = htons(Config::get(C_SERVER_PORT).Int());
        a->sin6_flowinfo = 0;
        a->sin6_scope_id = 0;
      }
      else
      {
        sockaddr_in* a = reinterpret_cast<sockaddr_in*>(addr);
        a->sin_family = AF_INET;
        if (Config::get(C_SERVER_GLOBAL).Bool())
        {
          a->sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
          a->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }
        a->sin_port = htons(Config::get(C_SERVER_PORT).Int());
      }
    }

    bool WebServer::doBind(int socketDescriptor)
    {
      sockaddr_storage servAddr;
      int ret = 0;

      getServAddr(&servAddr);

      ret = bind(socketDescriptor, (sockaddr*)&servAddr, sizeof(servAddr));
      if (ret != 0) return false;

      ret = listen(socketDescriptor, SERVER_QUEUE_SIZE);
      if (ret != 0) return false;

      return true;
    }

    void WebServer::run()
    {
      logger->log(Logger::INFO, "Starting Server...");

      ifaddrs* if_addrs;
      getifaddrs(&if_addrs);

      for (ifaddrs* i = if_addrs; i != NULL; i = i->ifa_next)
      {
        if (i->ifa_addr != NULL)
        {
          bool global = Config::get(C_SERVER_GLOBAL).Bool();
          bool v6     = Config::get(C_SERVER_IPV6).Bool();

          if (i->ifa_addr->sa_family == AF_INET)
          {
            if (!(global || !v6)) continue;
          }
          else if (i->ifa_addr->sa_family == AF_INET6)
          {
            if (!v6) continue;
          }
          else
          {
            continue;
          }

          if (((i->ifa_flags & IFF_LOOPBACK) == IFF_LOOPBACK) || global)
          {
            std::stringstream ss;
            ss << "  IF: " << i->ifa_name << " ";

            ss << addrStr(i->ifa_addr);

            ss << ":" << Config::get(C_SERVER_PORT).Int();

            logger->log(Logger::INFO, ss.str());
          }
        }
      }

      freeifaddrs(if_addrs);

      int socketDescriptor, clientAddrLength;
      sockaddr_in6 clientAddr;

      bool error = false;

      int sNamespace = Config::get(C_SERVER_IPV6).Bool() ? AF_INET6 : AF_INET;
      socketDescriptor = socket(sNamespace, SOCK_STREAM | SOCK_NONBLOCK, 0);
      if (socketDescriptor < 0)
      {
        logger->error(Logger::ERROR, "Opening socket failed");
        error = true;
      }

      if (!doBind(socketDescriptor))
      {
        logger->error(Logger::ERROR, "Binding socket failed");
        error = true;
      }

      clientAddrLength = sizeof(clientAddr);

      while (!error && isRunning())
      {
        sessionMgr->cleanFinishedSessions();

        int connection = accept(socketDescriptor, (sockaddr*)&clientAddr,
                                (socklen_t*)&clientAddrLength);
        if (connection < 0)
        {
          if (errorTryAgain(errno) || errorOkNetError(errno))
          {
            // Waiting for connection or a resumable error
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
          }
          else
          {
            // Likely configuration error so stop server
            error = true;
            logger->error(Logger::ERROR, "Stopping server due to error");
          }
        }
        else
        {
          timeval tout;
          tout.tv_sec = 5;
          tout.tv_usec = 0;
          if (setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(tout)) < 0)
          {
            logger->error(Logger::WARN, "Failed to change timeout");
          }

          // Start a thread to read the request
          // Necessary because some browsers (ahem, Cr) start a speculative
          //  request and send no data on it, so although there is no benefit
          //  to responding to multiple requests at once (a debug session must
          //  be serialised) it still has to be handled to prevent deadlocks
          std::string clientAddrStr = addrStr(&clientAddr);
          std::thread request_thread(&WebServer::readRequest, this, connection, clientAddrStr);

          request_thread.detach();
        }

        // Go through the request queue and respond
        // To support multiple sessions, would need a request queue per
        //   session (identification of sessions not considered)
        while (pendingRequest())
        {
          if (requestQueueMutex.try_lock())
          {
            Request r = requestQueue.front();
            requestQueue.pop();

            requestQueueMutex.unlock();

            auto resp = routes->getResponse(r);

            std::string respStr = resp->toRespStr();
            logger->log(Logger::TRACE, [&]() { return r.toString(); });
            logger->log(Logger::TRACE, respStr);

            int n = write(r.getConnection(), respStr.c_str(), respStr.length());
            if (n < 0)
            {
              std::stringstream s;
              s << " Writing to socket failed (";
              s << r.clientAddr() << ") #" << r.getConnection();
              logger->error(Logger::ERROR, s.str());
            }
          }

        }

        std::this_thread::yield();
      }

      sessionMgr->endAllSessions();

      close(socketDescriptor);
      logger->log(Logger::INFO, "Server stopped");
    }

    std::string WebServer::addrStr(sockaddr_in6* addr)
    {
      return addrStr(reinterpret_cast<sockaddr*>(addr));
    }

    std::string WebServer::addrStr(sockaddr* addr)
    {
      if (addr->sa_family == AF_INET6)
      {
        char c[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &((sockaddr_in6*)addr)->sin6_addr, c,
        INET6_ADDRSTRLEN) != NULL)
        {
          return std::string(c);
        }
      }
      else if (addr->sa_family == AF_INET)
      {
        char c[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &((sockaddr_in*)addr)->sin_addr, c,
        INET_ADDRSTRLEN) != NULL)
        {
          return std::string(c);
        }
      }

      return "?";
    }

    void WebServer::readRequest(int connection, std::string clientAddr)
    {
      int tries = 0;
      auto tid = std::this_thread::get_id();
      std::stringstream s;
      s << "#" << tid << " Starting (" << clientAddr << ")";
      logger->log(Logger::DBG, s.str());

      int n;
      char buffer[512];
      std::stringstream req_buf;

      n = read(connection, buffer, sizeof(buffer));

      std::stringstream s2;
      s2 << "#" << tid << " Read ";
      s2 << n << "byte(s) /" << sizeof(buffer);
      logger->log(Logger::DBG, s2.str());

      bool isGet = false;
      bool isPost = false;
      bool seenHeaders = false;
      size_t headerOffset = std::string::npos;
      bool lengthValid = false;
      size_t contLength = 0;
      size_t curLength = 0;
      bool gotAll = n == 0;
      while ((n == sizeof(buffer) && n > 0) || !gotAll)
      {
        if (!gotAll && n == 0)
        {
          tries++;
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // If not got all of request, n may be negative...
        if (n > 0)
        {
          curLength += n;
          req_buf.write(buffer, n);
        }

        std::string tmpStr = req_buf.str();
        if (!seenHeaders)
        {
          isGet = tmpStr.substr(0,3) == "GET";
          isPost = tmpStr.substr(0,4) == "POST";
          headerOffset = tmpStr.find("\r\n\r\n");
          seenHeaders = headerOffset != std::string::npos;
          headerOffset += 4;
        }
        if (seenHeaders)
        {
          if (isGet)
          {
            gotAll = true;
          }
          else if (isPost)
          {
            std::string searchStr = "Content-Length: ";
            size_t cLen = tmpStr.find(searchStr);
            if (cLen != std::string::npos)
            {
              req_buf.seekg(cLen+searchStr.length());
              req_buf >> contLength;
              lengthValid = true;
              req_buf.seekg(0);
            }
          }
        }
        if (lengthValid && isPost)
        {
          gotAll = curLength == (headerOffset+contLength);
        }

        if (!gotAll)
        {
          n = read(connection, buffer, sizeof(buffer));
          std::stringstream s;
          s << "#" << tid << " Read ";
          s << n << "byte(s) /" << sizeof(buffer);
          logger->log(Logger::DBG, s.str());

          if (tries > SERVER_NUM_TRIES)
          {
            gotAll = true;
            n = 0;
          }
        }
      }
      if (n < 0)
      {
        std::stringstream s;
        s << "#" << tid << " Reading from socket failed (";
        s << clientAddr << ")";
        logger->error(Logger::ERROR, s.str());

        // Reduce lingering timeout when a connection has already broken
        linger l;
        l.l_onoff = 1;
        l.l_linger = 1;
        if (setsockopt(connection, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) < 0)
        {
          logger->error(Logger::WARN, "Failed to reduce lingering time");
        }
        close(connection);
      }
      else if (n > 0)
      {
        // Write req of request
        req_buf.seekg(0);

        Request r = Request::create(req_buf, clientAddr, connection);
        logger->log(Logger::INFO, r.toShortString());

        std::lock_guard<std::mutex> lock(requestQueueMutex);
        // Push responsibility back to main thread
        requestQueue.push(r);

        std::stringstream s;
        s << "#" << tid << " Done (" << clientAddr << ")";
        logger->log(Logger::DBG, s.str());
      }
      else if (n == 0)
      {
        std::stringstream s;
        // Connection was closed by other end
        s << "#" << tid << " Closed (" << clientAddr << ")";
        logger->log(Logger::DBG, s.str());
      }
    }

  } /* namespace server */
} /* namespace penguinTrace */
