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
// Step Response Builder

#include "StepResponseBuilder.h"

#include "../penguintrace/SessionCmd.h"

namespace penguinTrace
{
  namespace server
  {

    StepResponseBuilder::StepResponseBuilder(StepType step, SessionManager* sMgr,
                                             std::unique_ptr<ComponentLogger> l)
        : ResponseBuilder(true, true), logger(std::move(l)), sessionMgr(sMgr), step(step)
    {
    }

    StepResponseBuilder::~StepResponseBuilder()
    {
    }

    std::unique_ptr<Response> StepResponseBuilder::getResponse(Request& req)
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Request Contents:" << std::endl;
        s << req.getBody();
        return s.str();
      });

      std::string msg = "Step";
      std::unique_ptr<Response> resp(new Response(HTTP200, req, msg, "application/json; charset=utf-8"));

      auto session = sessionMgr->lockSession(sessionId(req));
      if (session.valid())
      {
        if (session->getStepper() != nullptr)
        {
          session->enqueueCommand(
              std::unique_ptr<SessionCmd>(
                  new StepCmd(session->getStepper(), step)));

          resp->stream() << "{\"step\": true}";
        }
        else
        {
          // Returning false will reset web interface
          //  as we have no valid stepper
          resp->stream() << "{\"step\": false}";
        }

      }
      else
      {
        // Returning false will reset web interface
        //  as we have no valid session/stepper
        resp->stream() << "{\"step\": false}";
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
