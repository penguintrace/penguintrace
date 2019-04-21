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
// Logger for components

#ifndef COMMON_COMPONENTLOGGER_H_
#define COMMON_COMPONENTLOGGER_H_

#include <memory>
#include <string>

#include "Logger.h"

namespace penguinTrace
{

  class ComponentLogger
  {
    public:
      ComponentLogger(std::string p, Logger* l)
          : prefix(p), parentLogger(l)
      {

      }
      virtual ~ComponentLogger();
      void log(Logger::Severity s, std::string msg);
      void log(Logger::Severity s, std::function< std::string() > msg);
      void error(Logger::Severity s, std::string msg);
      std::unique_ptr<ComponentLogger> subLogger(std::string p);
    private:
      std::string prefix;
      Logger* parentLogger;
  };

} /* namespace penguinTrace */

#endif /* COMMON_COMPONENTLOGGER_H_ */
