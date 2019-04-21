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
// Parser Factory

#ifndef OBJECT_PARSERFACTORY_H_
#define OBJECT_PARSERFACTORY_H_

#include <memory>

#include "Parser.h"
#include "../common/ComponentLogger.h"

namespace penguinTrace
{
  namespace object
  {

    class ParserFactory
    {
      public:
        static std::unique_ptr<Parser> getParser(std::string filename, std::unique_ptr<ComponentLogger> logger);
      private:
        ParserFactory();
        virtual ~ParserFactory();
    };

  } /* namespace object */
} /* namespace penguinTrace */

#endif /* OBJECT_PARSERFACTORY_H_ */
