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
// DWARF Line Program

#include "LineProgram.h"

namespace penguinTrace
{
  namespace dwarf
  {

    std::string LineProgramHeader::Filename(int index)
    {
      unsigned fIdx = index-1;
      std::stringstream err;
      err << "FILE_NOT_FOUND(" << index << ")";
      std::string dir;
      if (fIdx >= file_names.size()) return err.str();
      auto fname = file_names[fIdx];
      if (!fname.Valid()) return err.str();

      unsigned dirIdx = fname.DirIndex();
      if (dirIdx == 0)
      {
        dir = "";
      }
      else if ((dirIdx-1) >= include_directories.size())
      {
        std::stringstream err;
        err << "DIR_NOT_FOUND(" << fname.DirIndex() << ")/";
        dir = err.str();
      }
      else
      {
        dir = include_directories[dirIdx-1] + "/";
      }
      return dir + fname.Name();
    }

    void LineProgramHeader::addFilename(std::istream& is)
    {
      LineProgramFileEntry file(is);
      if (!file.Valid()) throw Exception("Invalid filename for DW_LNE_define_file", __EINFO__);
      file_names.push_back(file);
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
