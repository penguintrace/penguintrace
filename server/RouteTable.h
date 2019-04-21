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
// Server Route Table

#ifndef SERVER_ROUTETABLE_H_
#define SERVER_ROUTETABLE_H_

#include <unordered_map>
#include <memory>
#include <sstream>

#include "ResponseBuilder.h"

#include "../common/ComponentLogger.h"

namespace penguinTrace
{
  class SessionManager;

  namespace server
  {
    class RouteTable : public ResponseBuilder
    {
      public:
        static RouteTable* getRouteTable(SessionManager* sMgr, std::unique_ptr<ComponentLogger> l);
        virtual ~RouteTable();
        std::unique_ptr<Response> getResponse(Request& req);
        void printRouteTable(std::stringstream* s, int prefix=0);
      private:
        void recursiveAddRoute(std::vector<std::string> path, files::Filename fname);
        typedef std::unordered_map<std::string, std::unique_ptr<ResponseBuilder> > RouteMap;
        RouteTable(std::unique_ptr<ComponentLogger> l);
        RouteTable(std::shared_ptr<ComponentLogger> l);
        std::shared_ptr<ComponentLogger> logger;
        RouteMap routeTable;
    };

  } /* namespace server */
} /* namespace penguinTrace */

#endif /* SERVER_ROUTETABLE_H_ */
