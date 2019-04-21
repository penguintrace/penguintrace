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
// Overall Logger

#ifndef COMMON_LOGGER_H_
#define COMMON_LOGGER_H_

#include <string>
#include <queue>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>

#include <fstream>

#include <unistd.h>

#include "../common/Common.h"

namespace penguinTrace
{
  class ComponentLogger;

  class Logger
  {
    public:
      enum Severity
      {
        TRACE = 0,
        DBG   = 1,
        INFO  = 2,
        WARN  = 3,
        ERROR = 4
      };
      const char * COLOUR_TRACE = "\033[2m";
      const char * COLOUR_DBG = "\033[37m";
      const char * COLOUR_INFO = "\033[0m";
      const char * COLOUR_WARN = "\033[1;31m";
      const char * COLOUR_ERROR = "\033[1;37;41m";
      const char * COLOUR_END = "\033[0m";
      Logger(Severity maxLevel);
      virtual ~Logger();
      void log(Severity s, std::string msg);
      void log(Severity s, std::function< std::string() > msg);
      void error(Severity s, std::string msg);
      void printMessages();
      std::unique_ptr<ComponentLogger> subLogger(std::string p);
      void addLevel(std::string prefix, Severity s)
      {
        componentLevel[prefix] = s;
      }
    private:
      void loadConfig();
      void loadConfigFromFile(std::ifstream& ifs, std::string fname);
      void errorPrefix(Severity s, std::string prefix, std::string msg);
      void logPrefix(Severity s, std::string prefix, std::string msg);
      void logPrefix(Severity s, std::string prefix, std::function< std::string() > msg);
      void logImpl(Severity s, std::string prefix, std::string msg);
      std::string formatMessage(std::pair<Severity, std::string> msg, bool c, bool t);
      bool shouldLog(Severity s, std::string prefix);
      Severity baseLevel;
      std::unordered_map<std::string, Severity> componentLevel;
      std::queue<std::pair<Severity, std::string> > messageQueue;
      std::mutex messageQueueMutex;
      bool colour;
      bool showTime;
      std::ofstream logFile;

      friend class ComponentLogger;
  };

} /* namespace penguinTrace */

#endif /* COMMON_LOGGER_H_ */
