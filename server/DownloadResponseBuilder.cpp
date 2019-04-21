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
// Download Response Builder

#include "DownloadResponseBuilder.h"

namespace penguinTrace
{
  namespace server
  {

    DownloadResponseBuilder::DownloadResponseBuilder(SessionManager* sMgr, std::unique_ptr<ComponentLogger> l)
        : ResponseBuilder(true, false), logger(std::move(l)), sessionMgr(sMgr)
    {
    }

    DownloadResponseBuilder::~DownloadResponseBuilder()
    {
    }

    std::unique_ptr<Response> DownloadResponseBuilder::getResponse(Request& req)
    {
      auto sid = sessionId(req);
      auto session = sessionMgr->lockSession(sid);

      if (session.valid())
      {
        auto buf = session->getBuffer();

        if (buf == nullptr)
        {
          return nullptr;
        }
        else
        {
          std::string msg = "Download";
          std::unique_ptr<Response> resp(new Response(HTTP200, req, msg, "application/x-executable; charset=binary"));

          resp->addHeader("Content-Disposition", "attachment; filename=\"PENGUINTRACE_" + sid +"\"");
          resp->stream().write(buf->data(), buf->size());
          return resp;
        }
      }
      else
      {
        return nullptr;
      }
    }

  } /* namespace server */
} /* namespace penguinTrace */
