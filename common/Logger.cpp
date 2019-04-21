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

#include "Logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include <string.h>
#include <ctime>

#include "ComponentLogger.h"
#include "StreamOperations.h"

namespace penguinTrace
{

  Logger::Logger(Severity maxLevel) : baseLevel(maxLevel)
  {
    if (Config::get(C_LOG_COLOUR).Bool())
    {
      colour = isatty(Config::get(C_LOG_STDERR).Bool() ? FD_STDERR : FD_STDOUT) == 1;
    }
    showTime = Config::get(C_LOG_TIME).Bool();
    if (Config::get(C_LOG_FILE).String().length() > 0)
    {
      logFile.open(Config::get(C_LOG_FILE).String());
    }
    loadConfig();
  }

  Logger::~Logger()
  {
    if (logFile.is_open())
    {
      logFile.close();
    }
  }

  void Logger::loadConfig()
  {
    std::string homeDir = std::string(getenv("HOME"));
    std::string homeCfg = homeDir+"/penguintrace.log.cfg";

    std::ifstream homeStrm(homeCfg);
    if (homeStrm.good())
    {
      loadConfigFromFile(homeStrm, homeCfg);
    }

    std::string dirFname = "penguintrace.log.cfg";
    std::ifstream dirStrm(dirFname);
    if (dirStrm.good())
    {
      loadConfigFromFile(dirStrm, dirFname);
    }

    std::string fname = Config::get(C_LOG_CFG_FILE).String();
    std::ifstream ifs(fname);
    if (ifs.good())
    {
      loadConfigFromFile(ifs, fname);
    }
    else if (fname.size() > 0)
    {
      log(WARN, [&]() {
        std::stringstream s;
        s << "Could not open log config '";
        s << fname << "'";
        return s.str();
      });
    }

    log(DBG, [&]() {
      std::stringstream s;
      unsigned pad = 0;
      for (auto l : componentLevel)
      {
        pad = (l.first.size() > pad) ? l.first.size() : pad;
      }
      s << "Log levels:";
      for (auto l : componentLevel)
      {
        s << std::endl << "  ";
        s << StringPad(l.first, pad, true) << " = ";
        switch (l.second)
        {
          case TRACE: s << "TRACE"; break;
          case DBG:   s << "DBG";   break;
          case INFO:  s << "INFO";  break;
          case WARN:  s << "WARN";  break;
          case ERROR: s << "ERROR"; break;
        }
      }
      return s.str();
    });
  }

  void Logger::loadConfigFromFile(std::ifstream& ifs, std::string fname)
  {
    std::cout << "Parsing logger config file '" << fname << "'" << std::endl;

    while (!ifs.eof())
    {
      std::string whitespace = " \t";
      std::string line;
      std::getline(ifs, line);
      if (line.length() > 0)
      {
        if (line[0] != '#')
        {
          if (line.find('=') == std::string::npos) continue;
          const auto strBegin = line.find_first_not_of(whitespace);
          if (strBegin == std::string::npos) continue;
          const auto strEnd = line.find_last_not_of(whitespace);
          const auto strRange = strEnd - strBegin + 1;

          std::stringstream ss(line.substr(strBegin, strRange));
          std::string key;
          std::string val;
          std::getline(ss, key, '=');
          std::getline(ss, val, '\0');
          key = trimWhitespace(key);
          val = trimWhitespace(val);
          Severity s;

          if (val == "TRACE") s = TRACE;
          else if (val == "DBG") s = DBG;
          else if (val == "INFO") s = INFO;
          else if (val == "WARN") s = WARN;
          else if (val == "ERROR") s = ERROR;
          else
          {
            log(WARN, [&]() {
              std::stringstream s;
              s << "Could not parse log level '";
              s << val;
              s << "'";
              return s.str();
            });
            continue;
          }

          if (key == "*") baseLevel = s;
          else            addLevel(key, s);
        }
      }
    }
  }

  bool Logger::shouldLog(Severity s, std::string prefix)
  {
    if (prefix.length() > 0)
    {
      auto it = componentLevel.find(prefix);
      if (it != componentLevel.end())
      {
        return (int)s >= it->second;
      }
      log(DBG, [&]() {
        std::stringstream s;
        s << "No log level for component '";
        s << prefix;
        s << "'";
        return s.str();
      });
      addLevel(prefix, baseLevel);
    }
    return (int)s >= baseLevel;
  }

  void Logger::error(Severity s, std::string msg)
  {
    std::stringstream ss;
    ss << msg << " '";
    ss << strerror(errno);
    ss << "'";
    log(s, ss.str());
  }

  void Logger::errorPrefix(Severity s, std::string prefix, std::string msg)
  {
    std::stringstream ss;
    ss << prefix << ": ";
    ss << msg;
    error(s, ss.str());
  }

  void Logger::log(Severity s, std::string msg)
  {
    logImpl(s, "", msg);
  }

  void Logger::log(Severity s, std::function< std::string() > msg)
  {
    logImpl(s, "", msg());
  }

  void Logger::logPrefix(Severity s, std::string prefix, std::string msg)
  {
    std::stringstream ss;
    ss << prefix << ": ";
    ss << msg;

    logImpl(s, prefix, ss.str());
  }

  void Logger::logPrefix(Severity s, std::string prefix, std::function< std::string() > msg)
  {
    std::stringstream ss;
    ss << prefix << ": ";
    ss << msg();

    logImpl(s, prefix, ss.str());
  }

  void Logger::logImpl(Severity s, std::string prefix, std::string msg)
  {
    if (shouldLog(s, prefix))
    {
      std::lock_guard<std::mutex> lock(messageQueueMutex);
      messageQueue.push( {s, msg} );
    }
  }

  std::unique_ptr<ComponentLogger> Logger::subLogger(std::string p)
  {
    return std::unique_ptr<ComponentLogger>(new ComponentLogger(p, this));
  }

  void Logger::printMessages()
  {
    std::lock_guard<std::mutex> lock(messageQueueMutex);

    while (!messageQueue.empty())
    {
      auto msg = messageQueue.front();
      std::string str = formatMessage(msg, colour, showTime);

      if (logFile.is_open())
      {
        logFile << formatMessage(msg, false, true);
      }

      if (Config::get(C_LOG_STDERR).Bool())
      {
        std::cerr << str;
      }
      else
      {
        std::cout << str;
      }

      messageQueue.pop();
    }
  }

  std::string Logger::formatMessage(std::pair<Severity, std::string> msg, bool c, bool t)
  {
    std::stringstream ss;

    switch (msg.first)
    {
      case TRACE:
        if (c) ss << COLOUR_TRACE;
        break;
      case DBG:
        if (c) ss << COLOUR_DBG;
        break;
      case INFO:
        if (c) ss << COLOUR_INFO;
        break;
      case WARN:
        if (c) ss << COLOUR_WARN;
        break;
      case ERROR:
        if (c) ss << COLOUR_ERROR;
        break;
    }

    if (t)
    {
      time_t now = time(0);
      tm *ltm = localtime(&now);

      ss << "[" << (1900 + ltm->tm_year) << "-";
      ss << std::setw(2) << std::setfill('0') << (1 + ltm->tm_mon) << "-";
      ss << std::setw(2) << std::setfill('0') << ltm->tm_mday;
      ss << " ";
      ss << std::setw(2) << std::setfill('0') << ltm->tm_hour;
      ss << ":";
      ss << std::setw(2) << std::setfill('0') << ltm->tm_min;
      ss << ":";
      ss << std::setw(2) << std::setfill('0') << ltm->tm_sec;
      ss << "] ";
    }

    switch (msg.first)
    {
      case TRACE:
        ss << "[TRACE] ";
        break;
      case DBG:
        ss << "[DEBUG] ";
        break;
      case INFO:
        ss << "[INFO]  ";
        break;
      case WARN:
        ss << "[WARN]  ";
        break;
      case ERROR:
        ss << "[ERROR] ";
        break;
    }
    ss << msg.second;
    if (c)
    {
      ss << COLOUR_END;
    }
    ss << std::endl;

    return ss.str();
  }

} /* namespace penguinTrace */
