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
// Breakpoint Response Builder

#include "BkptResponseBuilder.h"

#include "Serialize.h"

namespace penguinTrace
{
  namespace server
  {

    BkptResponseBuilder::BkptResponseBuilder(SessionManager* sMgr,
                                             std::unique_ptr<ComponentLogger> l)
        : ResponseBuilder(true, true), logger(std::move(l)), sessionMgr(sMgr)
    {
    }

    BkptResponseBuilder::~BkptResponseBuilder()
    {
    }

    std::unique_ptr<Response> BkptResponseBuilder::getResponse(Request& req)
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Request Contents:" << std::endl;
        s << req.getBody();
        return s.str();
      });

      std::string msg = "Breakpoint";
      std::unique_ptr<Response> resp(new Response(HTTP200, req, msg, "application/json; charset=utf-8"));

      auto session = sessionMgr->lockSession(sessionId(req));

      if (session.valid() && session->getStepper() != nullptr)
      {
        bool setBkptOk = true;

        logger->log(Logger::DBG, req.getBody());
        std::map<std::string, std::string> action;
        for (auto it : split(req.getBody(), '&'))
        {
          auto pair = split(it, '=');
          action[pair[0]] = pair[1];
        }

        auto setIt = action.find("set");
        auto addrIt = action.find("addr");
        auto lineIt = action.find("line");
        bool set = (setIt != action.end()) && (setIt->second == "true");
        setBkptOk &= setIt != action.end();
        if (addrIt != action.end())
        {
          std::stringstream s(addrIt->second);
          uint64_t addr = 0;
          s >> std::hex >> addr;
          if (addr != 0)
          {
            if (set)
            {
              session->getStepper()->queueBreakpoint(addr, false);
            }
            else
            {
              session->getStepper()->removeBreakpoint(addr, false);
            }
          }
          else
          {
            // Failed to get address
            setBkptOk = false;
          }
        }
        else if (lineIt != action.end())
        {
          std::stringstream s(lineIt->second);
          uint64_t addr = 0;
          s >> addr;
          if (addr != 0)
          {
            if (set)
            {
              session->getStepper()->queueBreakpoint(addr, true);
            }
            else
            {
              session->getStepper()->removeBreakpoint(addr, true);
            }
          }
          else
          {
            // Failed to get address
            setBkptOk = false;
          }
        }
        else
        {
          logger->log(Logger::ERROR, "No address or line for breakpoint");
          setBkptOk = false;
        }

        resp->stream() << *Serialize::bkptState(*session, setBkptOk);
      }
      else
      {
        // Returning error to reset state of web interface
        resp->stream() << "{\"bkpt\": false, \"error\": true}";
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
