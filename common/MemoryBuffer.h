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
// Generic Memory Buffer

#ifndef COMMON_MEMORYBUFFER_H_
#define COMMON_MEMORYBUFFER_H_

#include <istream>
#include <streambuf>

namespace penguinTrace
{

  class MemoryBuffer : public std::streambuf
  {
    public:
      MemoryBuffer(uint8_t* data, size_t size)
      {
        setg((char*)data, (char*)data, (char*)data + size);
        setp((char*)data, (char*)data + size);
      }
      virtual ~MemoryBuffer();
      pos_type seekpos(pos_type sp, std::ios_base::openmode which) override {
        return seekoff(sp - pos_type(off_type(0)), std::ios_base::beg, which);
      }
      pos_type seekoff(off_type off,
                       std::ios_base::seekdir dir,
                       std::ios_base::openmode which = std::ios_base::in) override {
        if (dir == std::ios_base::cur)
          gbump(off);
        else if (dir == std::ios_base::end)
          setg(eback(), egptr() + off, egptr());
        else if (dir == std::ios_base::beg)
          setg(eback(), eback() + off, egptr());
        return gptr() - eback();
      }
  };

} /* namespace penguinTrace */

#endif /* COMMON_MEMORYBUFFER_H_ */
