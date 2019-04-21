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
// Show debugging information from a binary

#include <memory>
#include <thread>

#include "debug/Stepper.h"
#include "object/ParserFactory.h"
#include "dwarf/Info.h"

#include "common/StreamOperations.h"

void logThread(penguinTrace::Logger* log, bool* run)
{
  while (*run)
  {
    log->printMessages();
    std::this_thread::yield();
  }
  log->printMessages();
}

int main(int argc, char **argv)
{
  std::string desc = "Display debug information in binary";
  if (penguinTrace::Config::parse(argc, argv, true, desc))
  {
    bool running = true;
    std::string filename(
        penguinTrace::Config::get(penguinTrace::C_FILE_ARGUMENT).String());

    penguinTrace::Logger logger(penguinTrace::Logger::TRACE);
    std::thread lThread(&logThread, &logger, &running);

    auto parser = penguinTrace::object::ParserFactory::getParser(filename, logger.subLogger("PARSE"));
    bool parseOk = parser->parse();
    if (parseOk)
    {
      penguinTrace::dwarf::Info info(parser.get(), logger.subLogger("DWARF"));

      info.printInfo();

      parser->close();
    }
    else
    {
      logger.log(penguinTrace::Logger::ERROR, "Failed to parse");
    }

    running = false;
    lThread.join();
  }

  return 0;
}
