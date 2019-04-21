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
// Generate penguinTrace session Ids

#include <iostream>

#include "common/IdGenerator.h"

void usage(std::string exe)
{
  std::cout << "Usage: " << exe << " N" << std::endl;
}

int main(int argc, char **argv)
{
  std::string desc = "Generate penguinTrace session IDs";

  if (argc < 2)
  {
    usage(argc > 0 ? argv[0] : "session-id-gen");
    return 1;
  }

  try
  {
    penguinTrace::IdGenerator idgen;

    int64_t num = std::stoi(argv[1], nullptr, 10);

    for (int i = 0; i < num; ++i)
    {
      std::cout << idgen.getNextId() << std::endl;
    }

    return 0;
  }
  catch (...)
  {
    usage(argv[0]);
    return 1;
  }
}
