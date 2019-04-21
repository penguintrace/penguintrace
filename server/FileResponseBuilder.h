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

#ifndef SERVER_FILERESPONSEBUILDER_H_
#define SERVER_FILERESPONSEBUILDER_H_

#include "ResponseBuilder.h"

namespace penguinTrace
{
  namespace server
  {

    class FileResponseBuilder : public ResponseBuilder
    {
      public:
        FileResponseBuilder(files::Filename file, std::string mime="",
            bool e404=false, bool e405=false);
        virtual ~FileResponseBuilder();
        std::unique_ptr<Response> getResponse(Request& req);
      private:
        std::stringstream contents;
        bool found;
        bool e404;
        bool e405;
        std::string encoding;
    };

  } /* namespace server */
} /* namespace penguinTrace */

#endif /* SERVER_FILERESPONSEBUILDER_H_ */
