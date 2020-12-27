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
// Common functions - OS specific

#include <string>
#include <sstream>

namespace penguinTrace
{

  const std::string ELF_SOURCE_SEGMENT = "";

  inline bool canEmbedSource()
  {
    return true;
  }

  inline bool errorOkNetError(int err)
  {
    return (err == ENETDOWN) ||
           (err == EPROTO) ||
           (err == ENOPROTOOPT) ||
           (err == EHOSTDOWN) ||
           (err == ENONET) ||
           (err == EHOSTUNREACH) ||
           (err == EOPNOTSUPP) ||
           (err == ENETUNREACH);
  }

  inline std::string debugFilename(std::string abs_executable, std::string executable)
  {
    return abs_executable;
  }

}
