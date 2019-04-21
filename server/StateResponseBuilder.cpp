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

#include "StateResponseBuilder.h"

#include "Serialize.h"

namespace penguinTrace
{
  namespace server
  {

    StateResponseBuilder::StateResponseBuilder(ResponseType type, SessionManager* sMgr,
                                             std::unique_ptr<ComponentLogger> l)
        : ResponseBuilder(true, false), logger(std::move(l)), sessionMgr(sMgr), type(type)
    {
    }

    StateResponseBuilder::~StateResponseBuilder()
    {
    }

    std::unique_ptr<Response> StateResponseBuilder::getResponse(Request& req)
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Request Contents:" << std::endl;
        s << req.getBody();
        return s.str();
      });

      std::string msg = "State";
      std::unique_ptr<Response> resp(new Response(HTTP200, req, msg, "application/json; charset=utf-8"));

      auto session = sessionMgr->lockSession(sessionId(req));
      if (session.valid())
      {
        if (session->pendingCommands() || ((type != ALL) && (session->getStepper() == nullptr)))
        {
          if (session->pendingCommands())
          {
            resp->stream() << "{\"state\": false, \"retry\": true, \"arch\": \"" MACHINE_ARCH "\"}";
          }
          else
          {
            resp->stream() << "{\"state\": false, \"retry\": false, \"arch\": \"" MACHINE_ARCH "\"}";
          }
          logger->log(Logger::TRACE, "No state - pending commands");
        }
        else
        {
          if (type == ALL)
          {
            resp->stream() << *Serialize::sessionState(*session);
          }
          else
          {
            resp->stream() << *Serialize::stepState(*session);
          }
        }
        logger->log(Logger::TRACE, "Returning state");
      }
      else
      {
        resp->stream() << "{\"state\": false, \"retry\": false, \"arch\": \"" MACHINE_ARCH "\"}";
        logger->log(Logger::TRACE, "No session");
      }

      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Response Contents:" << std::endl;
        s << resp->stream().str();
        return s.str();
      });
      return resp;
    }

  } /* namespace server */
} /* namespace penguinTrace */
