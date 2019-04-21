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

#include "IdGenerator.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>

namespace penguinTrace
{

  IdGenerator::IdGenerator()
  : nextId(0)
  {
    std::vector<std::string> adjectives = {
        "beeping", "blinking", "buzzing", "clicking",
        "executing", "flickering", "running",
        "sleeping", "stopping", "whirring"
    };
    std::vector<std::string> colours = {
        "red", "green", "blue", "cyan", "magenta", "yellow",
        "black", "white"
    };
    std::vector<std::string> nouns = {
        "console", "keyboard", "laptop", "mouse",
        "monitor", "motherboard", "network", "printer",
        "terminal", "wire", "router", "modem", "screen",
        "speaker", "penguin"
    };

    for (auto aIt = adjectives.begin(); aIt != adjectives.end(); ++aIt)
    {
      for (auto cIt = colours.begin(); cIt != colours.end(); ++cIt)
      {
        for (auto nIt = nouns.begin(); nIt != nouns.end(); ++nIt)
        {
          names.push_back({*aIt, *cIt, *nIt});
        }
      }
    }

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

    std::shuffle(names.begin(), names.end(), std::default_random_engine(seed));
  }

  std::string IdGenerator::getNextId()
  {
    std::lock_guard<std::mutex> lock(idMutex);
    std::stringstream s;

    if (nextId >= names.size())
    {
      // Fallback to numeric id to save handling
      //  overflow etc.
      s << "session" << nextId;
    }
    else
    {
      auto n = names[nextId];
      s << std::get<0>(n) << '_' << std::get<1>(n) << '_' << std::get<2>(n);
    }

    nextId++;
    return s.str();
  }

} /* namespace penguinTrace */
