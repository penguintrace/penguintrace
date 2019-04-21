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

#include "Types.h"

namespace penguinTrace
{
  namespace server
  {
    Request Request::create(std::stringstream& req_stream, std::string client,
                     int connection)
    {
      Request r;
      r.connection = connection;
      r.client = client;
      // Request from stream
      std::string first_line;
      std::getline(req_stream, first_line);
      auto req = split(first_line, ' ');
      if (req.size() == 3)
      {
        if (req[0] == "GET")
        {
          r.type = GET;
        }
        else if (req[0] == "POST")
        {
          r.type = POST;
        }
        else
        {
          r.type = UNHANDLED;
          r.unknown_msg = req[0];
        }
        const std::vector<std::string>& splitUrl = splitFirst(req[1], '?');
        r.fullPath = splitUrl[0];
        r.queryString = splitUrl[1];
        const std::vector<std::string>& splitPath = split(r.fullPath, '/');
        for (auto it = splitPath.begin(); it != splitPath.end(); ++it)
        {
          if (it->length() > 0)
          {
            r.path.push(*it);
          }
        }
        r.protocol = rtrim(req[2]);

        const std::vector<std::string>& splitQuery = split(r.queryString, '&');
        for (auto it = splitQuery.begin(); it != splitQuery.end(); ++it)
        {
          const std::vector<std::string>& splitQueryInner = splitFirst(*it, '=');
          r.queryMap[splitQueryInner[0]] = splitQueryInner[1];
        }
      }
      else
      {
        r.type = UNHANDLED;
        r.unknown_msg = req_stream.str();
      }
      std::string sLine;
      while (std::getline(req_stream, sLine))
      {
        std::string s = rtrim(sLine);
        if (s.length() == 0)
        {
          break;
        }
        std::vector<std::string> hdr = split(s, ": ");
        if (hdr.size() == 2)
        {
          r.headers[hdr[0]] = hdr[1];
        }
      }

      std::stringstream bodyStream;
      std::string bLine;
      while (std::getline(req_stream, bLine))
      {
        bodyStream << bLine;
        if (!req_stream.eof())
        {
          bodyStream << std::endl;
        }
      }
      r.body = bodyStream.str();

      return r;
    }

    std::string Request::toShortString()
    {
      std::stringstream s;
      switch (type)
      {
        case GET:
          s << "GET ";
          break;
        case POST:
          s << "POST";
          break;
        default:
          s << "UNKNOWN(" << unknown_msg << ")";
          return s.str();
      }
      s << " " << fullPath;
      s << " <" << queryString << ">";
      s << " " << client;
      return s.str();
    }

    std::string Request::toString()
    {
      if (type == UNHANDLED)
      {
        return "UNHANDLED/UNKNOWN";
      }
      std::stringstream s;
      s << "Request:" << std::endl;
      s << "  ";
      switch (type)
      {
        case GET:
          s << "GET";
          break;
        case POST:
          s << "POST";
          break;
        default:
          s << "UNKNOWN";
          break;
      }
      s << " (" << protocol << ")" << std::endl;
      s << "  Path = " << fullPath;
      s << std::endl;
      s << "  Headers:" << std::endl;
      for (auto n : headers)
      {
        s << "    " << n.first;
        s << " = " << n.second;
        s << std::endl;
      }
      return s.str();
    }

    bool Response::addHeader(std::string header, std::string value)
    {
      auto it = headers.find(header);
      if (it != headers.end())
      {
        // No overriding of headers
        return false;
      }
      headers[header] = value;
      return true;
    }

    std::string Response::toString()
    {
      std::stringstream s;

      s << "Response:" << std::endl;
      s << "  " << protocol << " ";
      switch (type)
      {
        case HTTP200:
          s << "200";
          break;
        case HTTP404:
          s << "404";
          break;
        case HTTP405:
          s << "405";
          break;
        case HTTP500:
          s << "500";
          break;
        default:
          s << "UNKNOWN";
          break;
      }
      s << " " << type_msg << std::endl;
      for (auto it : headers)
      {
        s << "  " << it.first << " = " << it.second << std::endl;
      }
      s << "  Length = " << body.str().size() << std::endl;

      return s.str();
    }

    std::string Response::toRespStr()
    {
      std::stringstream s;
      s << protocol << " ";
      switch (type)
      {
        case HTTP200:
          s << "200";
          break;
        case HTTP404:
          s << "404";
          break;
        case HTTP405:
          s << "405";
          break;
        case HTTP500:
        default:
          s << "500";
          break;
      }
      s << " " << type_msg << "\r\n";
      for (auto it : headers)
      {
        s << it.first << ": " << it.second << "\r\n";
      }

      std::string b = body.str();

      s << "Content-Length: " << b.length() << "\r\n\r\n";
      s << b;
      return s.str();
    }
  } /* namespace server */
} /* namespace penguinTrace */
