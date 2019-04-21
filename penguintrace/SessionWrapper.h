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
// Server Session Wrapper
//
// Locks session for duration of wrapper existing

#ifndef PENGUINTRACE_SESSIONWRAPPER_H_
#define PENGUINTRACE_SESSIONWRAPPER_H_

#include <mutex>

#include "Session.h"

namespace penguinTrace
{
  class SessionWrapper
  {
    public:
      SessionWrapper();
      SessionWrapper(Session* s, std::unique_lock<std::mutex> m);
      SessionWrapper(SessionWrapper &&) = default;
      bool valid();
      Session& operator*();
      Session* operator->();
    private:
      Session* session;
      std::unique_lock<std::mutex> mutex;
  };

} /* namespace penguinTrace */

#endif /* PENGUINTRACE_SESSIONMANAGER_H_ */
