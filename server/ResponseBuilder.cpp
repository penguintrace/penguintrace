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
// Base Response Builder

#include "ResponseBuilder.h"

namespace penguinTrace
{
  namespace server
  {

    ResponseBuilder::ResponseBuilder(bool endpoint, bool postOnly)
    : isEndpoint(endpoint), requiresPost(postOnly)
    {
    }

    ResponseBuilder::~ResponseBuilder()
    {
    }

    std::string ResponseBuilder::sessionId(Request& req)
    {
      if (Config::get(C_SINGLE_SESSION).Bool())
      {
        return SINGLE_SESSION_NAME;
      }
      else
      {
        auto qMap = req.getQuery();
        auto it = qMap.find("sid");
        if (it != qMap.end())
        {
          return it->second;
        }
        else
        {
          return "";
        }
      }
    }

  } /* namespace server */
} /* namespace penguinTrace */
