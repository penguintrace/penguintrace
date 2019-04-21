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
// Static File Response Builder

#include "FileResponseBuilder.h"

#include "static_files.h"

namespace penguinTrace
{
  namespace server
  {

    FileResponseBuilder::FileResponseBuilder(files::Filename file, std::string mime,
        bool e404, bool e405)
    : ResponseBuilder(true, false), e404(e404), e405(e405)
    {
      auto f = files::files.find(file);
      auto type = files::fileTypes.find(file);
      if (f == files::files.end() || type == files::fileTypes.end())
      {
        found = false;
        f = files::files.find("FL_E500_HTML");
        type = files::fileTypes.find("FL_E500_HTML");
        assert(f != files::files.end() && type != files::fileTypes.end());
      }
      else
      {
        found = true;
      }
      contents.write(reinterpret_cast<const char *>(f->second.first), f->second.second);
      encoding = mime.length() > 0 ? mime : std::string(type->second);
    }

    FileResponseBuilder::~FileResponseBuilder()
    {
    }

    std::unique_ptr<Response> FileResponseBuilder::getResponse(Request& req)
    {
      ResponseType type = found ? (e405 ? HTTP405 : (e404 ? HTTP404 : HTTP200)) : HTTP500;
      std::string msg = found ? ((e404 || e405) ? "Oops" : "All Good") : "All gone horribly wrong...";
      std::unique_ptr<Response> resp(new Response(type, req, msg, encoding));
      resp->stream() << contents.str();
      return resp;
    }

  } /* namespace server */
} /* namespace penguinTrace */
