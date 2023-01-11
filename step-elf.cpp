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
// Run a binary in penguinTrace

#include <memory>
#include <thread>

#include "debug/Stepper.h"
#include "object/ParserFactory.h"

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
  std::string desc = "Step through a binary";
  penguinTrace::Config::showHelp(penguinTrace::C_AUTO_STEP);
  if (penguinTrace::Config::parse(argc, argv, true, desc))
  {
    bool running = true;
    std::string filename(
        penguinTrace::Config::get(penguinTrace::C_FILE_ARGUMENT).String());

    penguinTrace::Logger logger(penguinTrace::Logger::DBG);
    std::thread lThread(&logThread, &logger, &running);

    logger.log(penguinTrace::Logger::INFO, penguinTrace::Config::print());

    auto parser = penguinTrace::object::ParserFactory::getParser(filename, logger.subLogger("PARSE"));
    bool parseOk = parser->parse();

    auto argsQueue = std::unique_ptr<std::queue<std::string> >(new std::queue<std::string>());

    if (parseOk)
    {
      penguinTrace::Stepper stepper(filename, parser.get(), std::move(argsQueue), logger.subLogger("STEP"));

      if (stepper.init())
      {
        bool first = true;
        do
        {
          if (!first)
          {
            if (!penguinTrace::Config::get(penguinTrace::C_AUTO_STEP).Bool())
            {
              // Step on enter press
              getchar();
            }
          }
          first = false;
          while (!stepper.active())
          {
          }
          stepper.step(penguinTrace::STEP_INSTR);

          if (stepper.shouldContinueToEnd() && stepper.active())
          {
            stepper.step(penguinTrace::STEP_CONT);
          }

          while (!stepper.getStdout().empty())
          {
            auto s = stepper.getStdout().front();

            logger.log(penguinTrace::Logger::WARN, s);

            stepper.getStdout().pop();
          }
        }
        while (stepper.active());

        std::stringstream s;
        s << "final steps = " << stepper.getStepCount() << std::endl;
        logger.log(penguinTrace::Logger::DBG, s.str());

        parser->close();
      }
      else
      {
        logger.log(penguinTrace::Logger::ERROR, "Failed to initialise stepper");
      }
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
