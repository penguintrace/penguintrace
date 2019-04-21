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
// Custom Exceptions

#ifndef COMMON_EXCEPTION_H_
#define COMMON_EXCEPTION_H_

#include <string>
#include <exception>
#include <vector>

#define __EINFO__ __FILE__, __LINE__

namespace penguinTrace
{

  class Exception : public std::exception
  {
    public:
      Exception(std::string reason, std::string file, int line);
      void printInfo() const;
      const char * what () const throw();
    private:
      std::string reason;
      std::string file;
      int         line;
      std::vector<uint64_t> bt;
      const int BTRACE_DEPTH = 64;
  };

} /* namespace penguinTrace */

#endif /* COMMON_EXCEPTION_H_ */
