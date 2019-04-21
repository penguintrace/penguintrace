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
// Server specific Types

#ifndef SERVER_TYPES_H_
#define SERVER_TYPES_H_

#include <assert.h>
#include <map>
#include <string>
#include <sstream>
#include <queue>

#include "../common/Common.h"

namespace penguinTrace
{
  namespace server
  {
    enum RequestType
    {
      GET,
      POST,
      UNHANDLED
    };

    enum ResponseType
    {
      HTTP200,
      HTTP404,
      HTTP405,
      HTTP500
    };

    struct Request
    {
      public:
        static Request create(std::stringstream& req_stream, std::string client,
                int connection);
        const bool ok() const
        {
          return type != UNHANDLED;
        }
        const bool isPost() const
        {
          return type == POST;
        }
        const std::queue<std::string>& getPath() const
        {
          return path;
        }
        std::string getQueryString()
        {
          return queryString;
        }
        std::map<std::string, std::string>& getQuery()
        {
          return queryMap;
        }
        std::string getFullPath()
        {
          return fullPath;
        }
        std::string getProtocol() const
        {
          return protocol;
        }
        std::string clientAddr()
        {
          return client;
        }
        int getConnection()
        {
          return connection;
        }
        void popPath()
        {
          if (!path.empty())
          {
            path.pop();
          }
        }
        std::string getBody()
        {
          return body;
        }
        std::string toString();
        std::string toShortString();
      private:
        int connection;
        std::string client;
        std::string unknown_msg;
        RequestType type;
        std::queue<std::string> path;
        std::string queryString;
        std::map<std::string, std::string> queryMap;
        std::string fullPath;
        std::string protocol;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    struct Response
    {
      public:
        Response(ResponseType type, const Request& req, std::string type_msg,
            std::string encoding)
            : type(type), request(req), protocol(req.getProtocol()), type_msg(type_msg)
        {
          headers["Connection"] = "close";
          headers["Content-Type"] = encoding;
          if (type == HTTP405)
          {
            // HTTP405 implies the opposite as the request type is needed
            //  as only GET/POST are used
            if (req.isPost())
            {
              headers["Allow"] = "GET";
            }
            else
            {
              headers["Allow"] = "POST";
            }
          }
          // Content-Length will be added at the end
        }
        bool addHeader(std::string header, std::string value);
        std::string toString();
        std::string toRespStr();
        std::stringstream& stream() { return body; }
        const Request& getRequest() { return request; }
      private:
        ResponseType type;
        const Request& request;
        const std::string protocol;
        std::string type_msg;
        std::map<std::string, std::string> headers;
        std::stringstream body;
    };

  } /* namespace server */
} /* namespace penguinTrace */

#endif /* SERVER_TYPES_H_ */
