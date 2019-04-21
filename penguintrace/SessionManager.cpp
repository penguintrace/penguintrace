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
// Server Session Manager

#include "SessionManager.h"

#include <unistd.h>
#include <sstream>

namespace penguinTrace
{
  SessionManager::~SessionManager()
  {
  }

  void SessionManager::printSessions()
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::stringstream s;
    if (sessions.size() == 0)
    {
      s << "No running sessions";
    }
    else
    {
      s << "Sessions:" << std::endl;
    }
    for (auto it = sessions.begin(); it != sessions.end(); ++it)
    {
      if (it != sessions.begin()) s << std::endl;
      s << "  " << it->first << " (";
      s << it->second->timeString() << ")";
    }
    logger->log(Logger::INFO, s.str());
  }

  SessionWrapper SessionManager::lockSession(std::string session)
  {
    return getSession(session);
  }

  SessionWrapper SessionManager::getSession(std::string session)
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(session);
    auto sit = sessionLocks.find(session);
    return ((it != sessions.end()) && (sit != sessionLocks.end())) ?
              SessionWrapper(it->second.get(), std::unique_lock<std::mutex>(sit->second)) :
              SessionWrapper();
  }

  bool SessionManager::removeSession(std::string session)
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return removeSessionImpl(session);
  }

  bool SessionManager::removeSessionImpl(std::string session)
  {
    auto sIt = sessions.find(session);
    if (sIt != sessions.end())
    {
      bool success = cleanSession(*sIt->second);
      sessions.erase(session);
      if (sessionLocks.find(session) != sessionLocks.end())
      {
        sessionLocks.erase(session);
      }
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Cleaning session '";
        s << session;
        s << "'";
        return s.str();
      });
      return success;
    }
    return true;
  }

  void SessionManager::endAllSessions()
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (auto it = sessions.begin(); it != sessions.end(); ++it)
    {
      cleanSession(*it->second);
    }
    sessions.clear();
    sessionLocks.clear();
  }

  void SessionManager::cleanFinishedSessions()
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::queue<std::string> toRemove;

    for (auto it = sessions.begin(); it != sessions.end(); ++it)
    {
      if (it->second->toRemove())
      {
        toRemove.push(it->first);
      }
    }

    while (!toRemove.empty())
    {
      std::string s = toRemove.front();
      toRemove.pop();
      removeSessionImpl(s);
    }
  }

  bool SessionManager::cleanSession(Session& s)
  {
    s.cleanup();

    if (s.getParser() != nullptr)
    {
      s.getParser()->close();
    }

    auto fname = s.executable();

    if (Config::get(C_DELETE_TEMP_FILES).Bool())
    {
      logger->log(Logger::DBG, [&]()
      {
        std::stringstream ss;
        ss << "Deleting '";
        ss << fname;
        ss << "'";
        return ss.str();
      });
      return unlink(fname.c_str()) == 0;
    }
    else
    {
      return true;
    }
  }

  SessionWrapper SessionManager::createSession(std::string filename)
  {
    std::lock_guard<std::mutex> lock(sessionMutex);
    bool singleSession = Config::get(C_SINGLE_SESSION).Bool();
    std::string session = singleSession ? SINGLE_SESSION_NAME : idGen.getNextId();
    auto sIt = sessions.find(session);
    bool alreadyExists = sIt != sessions.end();

    if (alreadyExists && !singleSession)
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Session '" << session << "' already exists";
        return s.str();
      });
      return SessionWrapper();
    }
    if (alreadyExists)
    {
      logger->log(Logger::DBG, [&]()
      {
        std::stringstream s;
        s << "Replacing session '" << session << "'";
        return s.str();
      });
      cleanSession(*sIt->second);
      sessions.erase(session);
      sessionLocks.erase(session);
    }

    sessions.insert(
        std::pair<std::string, std::unique_ptr<Session> >(
            session, std::unique_ptr<Session>(new Session(filename, session, shutdownCallback))));
    // Will call default std::mutex constructor if not already in map
    std::unique_lock<std::mutex> slock(sessionLocks[session]);
    return SessionWrapper(sessions.at(session).get(), std::move(slock));
  }

} /* namespace penguinTrace */
