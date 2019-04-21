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

#ifndef PENGUINTRACE_SESSIONMANAGER_H_
#define PENGUINTRACE_SESSIONMANAGER_H_

#include "../common/ComponentLogger.h"

#include <unordered_map>
#include <mutex>

#include "Session.h"
#include "SessionWrapper.h"
#include "../common/IdGenerator.h"

namespace penguinTrace
{
  class SessionManager
  {
    public:
      SessionManager(std::function<void()> sc, std::unique_ptr<ComponentLogger> l)
          : logger(std::move(l)), shutdownCallback(sc)
      {

      }
      virtual ~SessionManager();
      void printSessions();
      SessionWrapper createSession(std::string filename);
      bool removeSession(std::string session);
      void endAllSessions();
      void cleanFinishedSessions();
      SessionWrapper lockSession(std::string session);
    private:
      bool removeSessionImpl(std::string session);
      std::unique_lock<std::mutex> getSessionLock(std::string session);
      SessionWrapper getSession(std::string session);
      bool cleanSession(Session& s);
      std::unique_ptr<ComponentLogger> logger;
      std::unordered_map<std::string, std::unique_ptr<Session> > sessions;
      std::unordered_map<std::string, std::mutex > sessionLocks;
      std::mutex sessionMutex;
      std::function<void()> shutdownCallback;
      IdGenerator idGen;
  };

} /* namespace penguinTrace */

#endif /* PENGUINTRACE_SESSIONMANAGER_H_ */
