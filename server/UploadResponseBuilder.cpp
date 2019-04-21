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
// Upload Response Builder

#include "UploadResponseBuilder.h"

#include "../debug/Compiler.h"

namespace penguinTrace
{
  namespace server
  {

    UploadResponseBuilder::UploadResponseBuilder(SessionManager* sMgr, std::unique_ptr<ComponentLogger> l)
        : ResponseBuilder(true, true), logger(std::move(l)), sessionMgr(sMgr)
    {
    }

    UploadResponseBuilder::~UploadResponseBuilder()
    {
    }

    std::unique_ptr<Response> UploadResponseBuilder::getResponse(Request& req)
    {
      auto tempFile = getTempFile(Config::get(C_TEMP_FILE_TPL).String());
      std::string fname = "";

      if (tempFile.first > 0)
      {
        fname = tempFile.second;
        close(tempFile.first);
      }

      // If got a temporary file, create the session - otherwise create an
      //  invalid wrapper which will error later.
      auto session = fname != "" ? sessionMgr->createSession(fname) : SessionWrapper();

      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Request Contents:" << std::endl;
        s << req.getBody();
        return s.str();
      });

      std::string msg = "Upload";
      std::unique_ptr<Response> resp(new Response(HTTP200, req, msg, "application/json; charset=utf-8"));

      if (session.valid())
      {
        session->enqueueCommand(
            std::unique_ptr<UploadCmd>(
                new UploadCmd(fname, req.getBody(), session->getCompileFailures())));
        session->enqueueCommand(
            std::unique_ptr<ParseCmd>(
                new ParseCmd(sessionMgr, session->getName(), logger->subLogger("PARSE"))));

        resp->stream() << "{\"compile\": true, \"session\": \"";
        if (!Config::get(C_SINGLE_SESSION).Bool())
        {
          resp->stream() << session->getName();
        }
        resp->stream() << "\"}";
      }
      else
      {
        resp->stream() << "{\"compile\": false, \"session\": \"\"}";
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
