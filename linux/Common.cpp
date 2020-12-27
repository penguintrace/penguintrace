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
// Common functions

#include "../common/Common.h"

#include <paths.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>

#include <cxxabi.h>

namespace penguinTrace
{

  std::pair<int, std::string> getTempFile(std::string name, bool tempDir)
  {
    int fd = -1;
    std::string fname = "";

    std::string nameTpl = "";
    if (tempDir)
    {
      nameTpl = getTempDir();
    }
    nameTpl += name + "-XXXXXX";

    char nameTemplate[nameTpl.length()+1];
    nameTpl.copy(nameTemplate,nameTpl.length(),0);
    nameTemplate[nameTpl.length()] = '\0';

    int tmpFD = mkstemp(nameTemplate);
    if (tmpFD == -1)
    {
      return {-1, nameTpl};
    }
    char fnamebuf[TEMP_FILE_NAME_BUF_LEN];
    std::stringstream procS;
    procS << "/proc/self/fd/" << tmpFD;
    int fLen = 0;
    if (tmpFD > 0)
    {
      fLen = readlink(procS.str().c_str(), fnamebuf, sizeof(fnamebuf));
    }
    std::stringstream tempFileNameS;

    if (fLen > 0 && fLen != TEMP_FILE_NAME_BUF_LEN && tmpFD >= 0)
    {
      tempFileNameS.write(fnamebuf, fLen);
      fname = tempFileNameS.str();
      fd = tmpFD;
    }
    else
    {
      // Overflow or error from readlink
      fd = -1;
      fname = nameTpl;
    }

    return {fd, fname};
  }

} /* namespace penguinTrace */
