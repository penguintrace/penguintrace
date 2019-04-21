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

#include "ComponentLogger.h"

namespace penguinTrace
{

  ComponentLogger::~ComponentLogger()
  {
  }

  void ComponentLogger::log(Logger::Severity s, std::string msg)
  {
    parentLogger->logPrefix(s, prefix, msg);
  }

  void ComponentLogger::log(Logger::Severity s, std::function<std::string()> msg)
  {
    parentLogger->logPrefix(s, prefix, msg);
  }

  void ComponentLogger::error(Logger::Severity s, std::string msg)
  {
    parentLogger->errorPrefix(s, prefix, msg);
  }

  std::unique_ptr<ComponentLogger> ComponentLogger::subLogger(std::string p)
  {
    std::string newPrefix = prefix+"."+p;
    parentLogger->logPrefix(Logger::DBG, "LOG", [&]() {
      std::stringstream s;
      s << "Added new logger '" << newPrefix << "'";
      return s.str();
    });
    return std::unique_ptr<ComponentLogger>(new ComponentLogger(newPrefix, parentLogger));
  }

} /* namespace penguinTrace */
