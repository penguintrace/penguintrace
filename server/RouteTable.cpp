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

#include "RouteTable.h"

#include <utility>

#include "FileResponseBuilder.h"
#include "CompileResponseBuilder.h"
#include "StepResponseBuilder.h"
#include "BkptResponseBuilder.h"
#include "StateResponseBuilder.h"
#include "StopResponseBuilder.h"
#include "StdinResponseBuilder.h"
#include "UploadResponseBuilder.h"
#include "DownloadResponseBuilder.h"

#include "static_files.h"

namespace penguinTrace
{
  namespace server
  {

    RouteTable::RouteTable(std::unique_ptr<ComponentLogger> l)
    : ResponseBuilder(false, false), logger(std::move(l))
    {
    }

    RouteTable::RouteTable(std::shared_ptr<ComponentLogger> l)
    : ResponseBuilder(false, false), logger(l)
    {
    }

    void RouteTable::recursiveAddRoute(std::vector<std::string> path, files::Filename fname)
    {
      if (path.size() == 1)
      {
        // Last element, add ResponseBuilder
        routeTable[path[0]] = std::unique_ptr<ResponseBuilder>(new FileResponseBuilder(fname));
      }
      else
      {
        // Subdirectory, add a new RouteTable
        assert(path.size() > 1 && "Must have elements in path");
        std::vector<std::string> newPath;

        for (unsigned i = 1; i < path.size(); ++i)
        {
          newPath.push_back(path[i]);
        }

        if (routeTable.find(path[0]) == routeTable.end())
        {
          routeTable[path[0]] = std::unique_ptr<ResponseBuilder>(new RouteTable(logger));
        }

        auto subTable = dynamic_cast<RouteTable*>(routeTable[path[0]].get());
        if (subTable == nullptr)
        {
          std::stringstream s;
          s << "Failed to add path '/";
          for (auto p : path) s << p << "/";
          s << "' - incorrect routing configuration";
          logger->log(Logger::ERROR, s.str());
        }
        else
        {
          subTable->recursiveAddRoute(newPath, fname);
        }
      }
    }

    RouteTable* RouteTable::getRouteTable(SessionManager* sMgr, std::unique_ptr<ComponentLogger> l)
    {
      RouteTable* r = new RouteTable(l->subLogger("route"));

      for (auto it : files::filelist)
      {
        auto path = split(std::string(it.first), "___");
        auto fname = it.second;
        r->recursiveAddRoute(path, fname);
      }

      r->routeTable[""] = std::unique_ptr<ResponseBuilder> (
          new FileResponseBuilder("FL_INDEX_HTML"));
      r->routeTable["compile"] = std::unique_ptr<ResponseBuilder> (
          new CompileResponseBuilder(sMgr, l->subLogger("compile")));
      r->routeTable["stop"] = std::unique_ptr<ResponseBuilder>(
          new StopResponseBuilder(sMgr, l->subLogger("stop")));
      r->routeTable["step"] = std::unique_ptr<ResponseBuilder> (
          new StepResponseBuilder(STEP_INSTR, sMgr, l->subLogger("step")));
      r->routeTable["step-line"] = std::unique_ptr<ResponseBuilder> (
          new StepResponseBuilder(STEP_LINE, sMgr, l->subLogger("step")));
      r->routeTable["continue"] = std::unique_ptr<ResponseBuilder> (
          new StepResponseBuilder(STEP_CONT, sMgr, l->subLogger("step")));
      r->routeTable["breakpoint"] = std::unique_ptr<ResponseBuilder> (
          new BkptResponseBuilder (sMgr, l->subLogger ("bkpt")));
      r->routeTable["stdin"] = std::unique_ptr<ResponseBuilder>(
          new StdinResponseBuilder(sMgr, l->subLogger("stdin")));
      r->routeTable["session-state"] = std::unique_ptr<ResponseBuilder> (
          new StateResponseBuilder(StateResponseBuilder::ALL, sMgr, l->subLogger("state")));
      r->routeTable["step-state"] = std::unique_ptr<ResponseBuilder> (
          new StateResponseBuilder(StateResponseBuilder::DELTA, sMgr, l->subLogger("state")));
      r->routeTable["upload"] = std::unique_ptr<ResponseBuilder> (
          new UploadResponseBuilder(sMgr, l->subLogger("upload")));
      r->routeTable["download"] = std::unique_ptr<ResponseBuilder> (
          new DownloadResponseBuilder(sMgr, l->subLogger("download")));

      return r;
    }

    void RouteTable::printRouteTable(std::stringstream * s, int prefix)
    {
      for (auto it = routeTable.begin(); it != routeTable.end(); ++it)
      {
        for (int i = 0; i < prefix; i++)
        {
          (*s) << "  ";
        }
        (*s) << '\'' << it->first << '\'' << std::endl;
        auto r = dynamic_cast<RouteTable*>(it->second.get());
        if (r != nullptr)
        {
          r->printRouteTable(s, prefix+1);
        }
      }
    }

    RouteTable::~RouteTable()
    {
    }

    std::unique_ptr<Response> RouteTable::getResponse(Request& req)
    {
      std::string path_elem = (req.getPath().empty()) ? "" : req.getPath().front();
      req.popPath();
      auto it = routeTable.find(path_elem);
      if (it != routeTable.end())
      {
        // A request always goes through the RouteTable, so we can check
        //  here if the ResponseBuilder requires a POST request
        // Similarly, an endpoint that doesn't require POST needs to be
        //  a GET request (this allows feedback that HEAD requests are
        //  not supported as well)
        if (req.isPost() != it->second->needsPost())
        {
          FileResponseBuilder naBld("FL_E405_HTML", "", false, true);
          return naBld.getResponse(req);
        }

        auto resp = it->second->getResponse(req);
        if (resp)
        {
          return resp;
        }
        else
        {
          // Treat null response as 404
          FileResponseBuilder nfBld("FL_E404_HTML", "", true);
          return nfBld.getResponse(req);
        }
      }
      else
      {
        FileResponseBuilder nfBld("FL_E404_HTML", "", true);
        return nfBld.getResponse(req);
      }
    }

  } /* namespace server */
} /* namespace penguinTrace */
