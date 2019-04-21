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
// State Response Builder

#ifndef SERVER_STATERESPONSEBUILDER_H_
#define SERVER_STATERESPONSEBUILDER_H_

#include "../common/ComponentLogger.h"

#include "ResponseBuilder.h"

#include "../penguintrace/SessionManager.h"


namespace penguinTrace
{
  namespace server
  {

    class StateResponseBuilder : public ResponseBuilder
    {
      public:
        enum ResponseType
        {
          ALL,
          DELTA
        };
        StateResponseBuilder(ResponseType type, SessionManager* sMgr, std::unique_ptr<ComponentLogger> l);
        virtual ~StateResponseBuilder();
        std::unique_ptr<Response> getResponse(Request& req);
      private:
        std::unique_ptr<ComponentLogger> logger;
        SessionManager* sessionMgr;
        ResponseType type;
    };

  } /* namespace server */
} /* namespace penguinTrace */

#endif /* SERVER_STATERESPONSEBUILDER_H_ */
