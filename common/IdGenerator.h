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
// (Usually) Readable ID Generator

#ifndef COMMON_IDGENERATOR_H_
#define COMMON_IDGENERATOR_H_

#include <stdint.h>

#include <mutex>
#include <string>
#include <vector>

namespace penguinTrace
{

  class IdGenerator
  {
    public:
      IdGenerator();
      std::string getNextId();
    private:
      uint64_t nextId;
      std::vector<std::tuple<std::string, std::string, std::string> > names;
      std::mutex idMutex;
  };

} /* namespace penguinTrace */

#endif /* COMMON_IDGENERATOR_H_ */
