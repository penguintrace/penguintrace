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
// Global Configuration

#include "Config.h"

#include "Common.h"

#include <list>
#include <queue>
#include <iostream>
#include <fstream>
#include <stdlib.h>

#include <sys/types.h>
#include <regex.h>
#include <unistd.h>

#include "StreamOperations.h"

namespace penguinTrace
{
  // Names for config file/command line
  std::string C_C_COMPILER_BIN        = "C_COMPILER_BIN";
  std::string C_CXX_COMPILER_BIN      = "CXX_COMPILER_BIN";
  std::string C_RUSTC_COMPILER_BIN    = "RUSTC_COMPILER_BIN";
  std::string C_CLANG_BIN             = "CLANG_BIN";
  std::string C_OBJCOPY_BIN           = "OBJCOPY_BIN";
  std::string C_LIB_DIRS              = "LIB_DIRS";
  std::string C_SERVER_PORT           = "SERVER_PORT";
  std::string C_DELETE_TEMP_FILES     = "DELETE_TEMP_FILES";
  std::string C_SERVER_GLOBAL         = "SERVER_GLOBAL";
  std::string C_SERVER_IPV6           = "SERVER_IPV6";
  std::string C_SINGLE_SESSION        = "SINGLE_SESSION";
  std::string C_USE_PTY               = "USE_PTY";
  std::string C_TEMP_FILE_TPL         = "TEMP_FILE_TPL_BINARIES";
  std::string C_TEMP_DIR_TPL          = "TEMP_DIR_TPL";
  std::string C_FILE_ARGUMENT         = "FILE_ARG";
  std::string C_AUTO_STEP             = "AUTO_STEP";
  std::string C_EXEC_NAME             = "EXEC_NAME";
  std::string C_ARGC_MAX              = "ARGC_MAX";
  std::string C_CSTR_MAX_CHAR         = "CSTR_MAX_CHAR";
  std::string C_LOG_COLOUR            = "LOG_COLOUR";
  std::string C_LOG_STDERR            = "LOG_STDERR";
  std::string C_LOG_FILE              = "LOG_FILE";
  std::string C_LOG_CFG_FILE          = "LOG_CFG_FILE";
  std::string C_LOG_TIME              = "LOG_TIME";
  std::string C_STEP_AFTER_MAIN       = "STEP_AFTER_MAIN";
  std::string C_PTR_DEPTH             = "PTR_DEPTH";
  std::string C_PRETTY_PRINT          = "PRETTY_PRINT";
  std::string C_HIDE_NON_PRETTY_PRINT = "HIDE_NON_PRETTY_PRINT";
  std::string C_ISOLATE_TRACEE        = "ISOLATE_TRACEE";
  std::string C_STRICT_MODE           = "STRICT_MODE";

  void regexError(int error, regex_t* r)
  {
    char* cBuf;
    size_t needed = regerror(error, r, 0, 0);
    cBuf = new char[needed];
    regerror(error, r, cBuf, needed);
    std::cout << cBuf << std::endl;

    delete[] cBuf;
  }

  std::function<bool(CfgValue)> RegexVal(std::string s)
  {
    return [=](CfgValue c) {
      bool match = false;
      int ret;
      regex_t re;
      std::stringstream newR;
      newR << "^" << s << "$";
      ret = regcomp(&re, newR.str().c_str(), REG_EXTENDED | REG_NOSUB);
      if (ret != 0)
      {
        std::cout << "Failed to compile validator regex: " << std::endl;
        regexError(ret, &re);
        return false;
      }
      regmatch_t m;
      ret = regexec(&re, c.String().c_str(), 1, &m, 0);

      if (ret == 0)
      {
        match = true;
      }

      regfree(&re);
      return match;
    };
  }

  std::function<bool(CfgValue)> MaxVal(int i)
  {
    return [=](CfgValue c) {
      return c.Int() <= i;
    };
  }

  std::map<std::string, ConfigDefault> Config::defaults = {
      {C_C_COMPILER_BIN,
        ConfigDefault(true,
                      CfgValue(std::string("")),
                      "Path to C compiler") },
      {C_CXX_COMPILER_BIN,
        ConfigDefault(true,
                      CfgValue(std::string("")),
                      "Path to C++ compiler") },
      {C_RUSTC_COMPILER_BIN,
        ConfigDefault(true,
                      CfgValue(std::string("")),
                      "Path to Rust compiler") },
      {C_CLANG_BIN,
        ConfigDefault(true,
                      CfgValue(std::string("")),
                      "Path to clang (for parsing)") },
      {C_OBJCOPY_BIN,
        ConfigDefault(true,
                      CfgValue(std::string("")),
                      "Path to objcopy") },
      {C_LIB_DIRS,
        ConfigDefault(true,
                      CfgValue(std::string("/lib:/lib64:/usr/lib")),
                      "Path to objcopy",
                      RegexVal("[.a-zA-Z0-9\\/]+(:[.a-zA-Z0-9\\/]+)*")) },
      {C_SERVER_PORT,
        ConfigDefault(true,
                      CfgValue((int64_t)8080),
                      "Port to listen on for server", MaxVal(65535)) },
      {C_SERVER_GLOBAL,
        ConfigDefault(true,
                      CfgValue(false),
                      "Bind to all network interfaces rather than just local") },
      {C_SERVER_IPV6,
        ConfigDefault(true,
                      CfgValue(false),
                      "Listen as IPv6") },
      {C_SINGLE_SESSION,
        ConfigDefault(true,
                      CfgValue(false),
                      "Only support a single (global) session") },
      {C_TEMP_FILE_TPL,
        ConfigDefault(true,
                      CfgValue(std::string("PENGUINTRACE-ELF")),
                      "Template for filenames for compiled executables",
                      RegexVal("[a-zA-Z0-9_-]+")) },
      {C_TEMP_DIR_TPL,
        ConfigDefault(true,
                      CfgValue(std::string("penguintrace-dir")),
                      "Template for directories for isolated processes",
                      RegexVal("[a-zA-Z0-9_-]+")) },
      {C_EXEC_NAME,
        ConfigDefault(true,
                      CfgValue(std::string("penguintrace")),
                      "Executable name passed to child process as argv[0]",
                      RegexVal("[a-zA-Z0-9_-]+")) },
      {C_LOG_COLOUR,
        ConfigDefault(false,
                      CfgValue(true),
                      "Try to print log messages in colour") },
      {C_LOG_STDERR,
        ConfigDefault(false,
                      CfgValue(false),
                      "Print log messages to STDERR") },
      {C_LOG_TIME,
        ConfigDefault(false,
                      CfgValue(false),
                      "Include time in log messages") },
      {C_LOG_CFG_FILE,
        ConfigDefault(false,
                      CfgValue(std::string("")),
                      "Configuration file for logging level (also searches ~/penguintrace.cfg.log and ./penguintrace.cfg.log)") },
      {C_LOG_FILE,
        ConfigDefault(false,
                      CfgValue(std::string("")),
                      "Print all log messages to specified file") },
      {C_STRICT_MODE,
        ConfigDefault(false,
                      CfgValue(false),
                      "Use strict compilation options (-Wall -Werror)") },
      // 'Advanced' configuration
      {C_DELETE_TEMP_FILES,
        ConfigDefault(true,
                      CfgValue(true),
                      "Delete temporary files on close") },
      {C_USE_PTY,
        ConfigDefault(true,
                      CfgValue(true),
                      "Use a pseudo-terminal for communication with child process") },
      {C_AUTO_STEP,
        ConfigDefault(true,
                      CfgValue(true),
                      "Automatically step (for command line tools)") },
      {C_STEP_AFTER_MAIN,
        ConfigDefault(true,
                      CfgValue(false),
                      "Continue single-stepping after returning from main") },
      {C_ARGC_MAX,
        ConfigDefault(true,
                      CfgValue((int64_t)64),
                      "Maximum number of strings to show for argc/argv") },
      {C_CSTR_MAX_CHAR,
        ConfigDefault(true,
                      CfgValue((int64_t)256),
                      "Maximum number of characters to try to show for a C string") },
      {C_PTR_DEPTH,
        ConfigDefault(true,
                      CfgValue((int64_t)8),
                      "Maximum number of pointers to dereference to get value") },
      {C_PRETTY_PRINT,
        ConfigDefault(false,
                      CfgValue(true),
                      "Try to print contents of standard C++ classes") },
      {C_HIDE_NON_PRETTY_PRINT,
        ConfigDefault(true,
                      CfgValue(false),
                      "If there is a pretty printer, hide default representation") }
  };

  std::string CfgValue::toString()
  {
    std::stringstream s;
    if (valueClass == CFG_VAL_BOOL)
    {
      s << (boolValue ? "true" : "false");
    }
    else if (valueClass == CFG_VAL_INT)
    {
      s << (int64_t)intValue;
    }
    else if (valueClass == CFG_VAL_STR)
    {
      s << '"' << strValue << '"';
    }
    else
    {
      s << "NULL";
    }
    return s.str();
  }

  CfgValue CfgValue::fromString(std::string s, bool forceString)
  {
    if (forceString)
    {
      return CfgValue(s);
    }
    else if (s == "true")
    {
      return CfgValue(true);
    }
    else if (s == "false")
    {
      return CfgValue(false);
    }

    try
    {
      int64_t i = std::stoi(s, nullptr, 10);
      return CfgValue(i);
    }
    catch (...)
    {
      return CfgValue(s);
    }
  }

  Config::Config()
  {
    defaultsCapabilities(&defaults);
    for (auto it = defaults.begin(); it != defaults.end(); ++it)
    {
      cfg[it->first] = it->second.getDefault();
    }
  }

  CfgValue Config::get(std::string key)
  {
    return Config::inst.cfg[key];
  }

  void Config::set(std::string key, int64_t i)
  {
    auto v = Config::inst.cfg[key];
    assert(v.type() == config_value_t::CFG_VAL_INT);
    Config::inst.cfg[key] = CfgValue(i);
  }

  void Config::set(std::string key, bool b)
  {
    auto v = Config::inst.cfg[key];
    assert(v.type() == config_value_t::CFG_VAL_BOOL);
    Config::inst.cfg[key] = CfgValue(b);
  }

  void Config::set(std::string key, std::string s)
  {
    auto v = Config::inst.cfg[key];
    assert(v.type() == config_value_t::CFG_VAL_STR);
    Config::inst.cfg[key] = CfgValue(s);
  }

  void Config::showHelp(std::string key)
  {
    defaults.find(key)->second.showHelp();
  }

  void Config::getPath(std::string key, std::string exe)
  {
    char * envPath = getenv("PATH");
    if (envPath != nullptr)
    {
      auto pathList = split(envPath, ':');
      for (auto path : pathList)
      {
        std::string filenameToTry = path + "/" + exe;
        if (fileExists(filenameToTry))
        {
          if (access(filenameToTry.c_str(), X_OK) == 0)
          {
            set(key, filenameToTry);
            break;
          }
        }
      }
    }
  }

  bool Config::getPaths()
  {
    bool ok = true;

    getPath(C_C_COMPILER_BIN,   "gcc");
    getPath(C_CXX_COMPILER_BIN, "g++");
    getPath(C_RUSTC_COMPILER_BIN, "rustc");
    getPath(C_C_COMPILER_BIN,   "clang");
    getPath(C_CXX_COMPILER_BIN, "clang++");
    getPath(C_CLANG_BIN,        "clang");
    getPath(C_OBJCOPY_BIN,      "objcopy");

    if (get(C_C_COMPILER_BIN).String().size() == 0)
    {
      std::cout << "Cannot find path to C compiler (clang/gcc)" << std::endl;
      ok = false;
    }
    if (get(C_CXX_COMPILER_BIN).String().size() == 0)
    {
      std::cout << "Cannot find path to C++ compiler (clang++/g++)" << std::endl;
      ok = false;
    }
    if (get(C_OBJCOPY_BIN).String().size() == 0)
    {
      std::cout << "Cannot find path to objcopy" << std::endl;
      ok = false;
    }
#ifdef USE_LIBCLANG
    if (get(C_CLANG_BIN).String().size() == 0)
    {
      std::cout << "Cannot find path to clang for libclang" << std::endl;
      ok = false;
    }
#endif

    return ok;
  }

  bool Config::parse(int argc, char **argv, bool needsFile, std::string desc)
  {
    if (!getPaths())
    {
      return false;
    }

    if (needsFile)
    {
      defaults.find(C_EXEC_NAME)->second.showHelp();
    }

    bool advHelp = false;
    bool parseOk = true;
    std::string filename(argc > 0 ? argv[0] : "?");
    std::queue<std::string> args;
    std::string fileArg = "";

    std::map<std::string, CfgValue> cfgFile;
    std::map<std::string, CfgValue> cfgCli;

    for (int i = 1; i < argc; ++i)
    {
      args.push(argv[i]);
    }

    std::string homeDir = std::string(getenv("HOME"));
    std::string homeCfg = homeDir+"/penguintrace.cfg";
    parseFile(&cfgFile, homeCfg);
    parseFile(&cfgFile, "penguintrace.cfg");

    while (!args.empty())
    {
      std::string arg = args.front();
      args.pop();

      if (arg == "-h" || arg == "--h" ||
          arg == "-help" || arg == "--help")
      {
        parseOk = false;
      }
      else if (arg == "-o" || arg == "--o" ||
          arg == "-options" || arg == "--options")
      {
        parseOk = false;
        advHelp = true;
      }
      else if (arg == "-f")
      {
        if (args.size() < 1)
        {
          parseOk = false;
          std::cout << "'-f' requires an argument" << std::endl;
          std::cout << std::endl;
          break;
        }
        std::string fname = args.front();
        bool ok = parseFile(&cfgFile, fname);
        args.pop();
        if (!ok)
        {
          std::cout << "Couldn't parse config from '";
          std::cout << fname << "'" << std::endl;
          std::cout << std::endl;
          parseOk = false;
        }
      }
      else if (arg == "-c")
      {
        if (args.size() < 2)
        {
          parseOk = false;
          std::cout << "'-c' requires two arguments" << std::endl;
          std::cout << std::endl;
          break;
        }
        else
        {
          std::string cfgKey = args.front();
          args.pop();
          std::string cfgValStr = args.front();
          args.pop();

          if (!attachToMap(&cfgCli, cfgKey, cfgValStr))
          {
            parseOk = false;
            continue;
          }
        }
      }
      else if (needsFile && fileArg == "")
      {
        fileArg = arg;
        cfgCli[C_FILE_ARGUMENT] = CfgValue(arg);
      }
      else
      {
        std::cout << "Unknown argument: '";
        std::cout << arg << "'";
        std::cout << std::endl;
        parseOk = false;
        break;
      }
    }

    if (parseOk && needsFile && fileArg == "")
    {
      parseOk = false;
      std::cout << "Expected filename" << std::endl;
    }

    if (parseOk)
    {
      for (auto it = cfgFile.begin(); it != cfgFile.end(); ++it)
      {
        inst.cfg[it->first] = it->second;
      }
      for (auto it = cfgCli.begin(); it != cfgCli.end(); ++it)
      {
        inst.cfg[it->first] = it->second;
      }
    }
    else
    {
      auto bin = split(filename, '/');
      std::cout << bin[bin.size()-1] << ": " << desc;
      std::cout << std::endl;
      std::cout << std::endl;
      std::cout << "Usage: " << filename;
      std::cout << " [-f FILE] [-c OPTION VALUE]... [-h]";
      if (needsFile)
      {
        std::cout << " FILE";
      }
      std::cout << std::endl;
      std::cout << std::endl;
      std::cout << "Command Line Options:" << std::endl;
      std::cout << "  -f FILE          ";
      std::cout << "Set a configuration file" << std::endl;
      std::cout << "  -c OPTION VALUE  ";
      std::cout << "Set a configuration option" << std::endl;
      std::cout << "  -h, --help       ";
      std::cout << "Show help" << std::endl;
      std::cout << "  -o, --options    ";
      std::cout << "Show all options" << std::endl;
      std::cout << std::endl;
      std::cout << "Configuration Options:" << std::endl;
      std::cout << "  Configuration will be also read from ~/penguintrace.cfg";
      std::cout << " and ./penguintrace.cfg if they exist";
      std::cout << std::endl;
      int maxLen = 0;
      for (auto it = defaults.begin(); it != defaults.end(); ++it)
      {
        if (it->second.getAdvanced() & !advHelp) continue;
        int curLen = it->first.length();
        if (curLen > maxLen) maxLen = curLen;
      }
      for (auto it = defaults.begin(); it != defaults.end(); ++it)
      {
        if (it->second.getAdvanced() & !advHelp) continue;
        std::cout << "  " << StringPad(it->first, maxLen);
        std::cout << ": " << it->second.getDesc();
        std::cout << " (default=" << it->second.getDefault().toString() << ")";
        std::cout << std::endl;
      }
      std::cout << std::endl;
      std::cout << std::endl;
      std::cout << "Copyright (C) 2019 Alex Beharrell" << std::endl;
      std::cout << "This program comes with ABSOLUTELY NO WARRANTY." << std::endl;
      std::cout << "This is free software, and you are welcome to ";
      std::cout << "redistribute it under certain conditions." << std::endl;
      std::cout << "Full details available at: ";
      std::cout << "https://github.com/penguintrace/penguintrace";
      std::cout << std::endl;
    }

    return parseOk;
  }

  bool Config::parseFile(std::map<std::string, CfgValue>* map, std::string fname)
  {
    std::ifstream ifs(fname);
    if (ifs.good())
    {
      std::cout << "Parsing config file '" << fname << "'" << std::endl;

      while (!ifs.eof())
      {
        std::string whitespace = " \t";
        std::string line;
        std::getline(ifs, line);
        if (line.length() > 0)
        {
          if (line[0] != '#')
          {
            if (line.find('=') == std::string::npos) return false;
            const auto strBegin = line.find_first_not_of(whitespace);
            if (strBegin == std::string::npos) continue;
            const auto strEnd = line.find_last_not_of(whitespace);
            const auto strRange = strEnd - strBegin + 1;

            std::stringstream ss(line.substr(strBegin, strRange));
            std::string key;
            std::string val;
            std::getline(ss, key, '=');
            std::getline(ss, val, '\0');

            bool ok = attachToMap(map, trimWhitespace(key),
                                       trimWhitespace(val));

            if (!ok)
            {
              return false;
            }
          }

        }
      }

      return true;
    }

    return false;
  }

  bool Config::attachToMap(std::map<std::string, CfgValue>* map,
      std::string key, std::string val)
  {
    bool isStr = false;
    auto oldValIt = inst.cfg.find(key);

    if (oldValIt != inst.cfg.end())
    {
      isStr = oldValIt->second.type() == CFG_VAL_STR;
    }

    CfgValue cfgVal = CfgValue::fromString(val, isStr);

    if (oldValIt != inst.cfg.end())
    {
      if (oldValIt->second.type() != cfgVal.type())
      {
        std::cout << '"' << key << '"';
        std::cout << " requires a value of type '";
        std::cout << oldValIt->second.typeStr();
        std::cout << "' but got a value of type '";
        std::cout << cfgVal.typeStr() << "'" << std::endl;
        std::cout << std::endl;
        return false;
      }
    }

    auto def = defaults.find(key);
    if (def != defaults.end())
    {
      if (!def->second.valid(cfgVal))
      {
        std::cout << '"' << val << '"';
        std::cout << " is not a value value for ";
        std::cout << '"' << key << '"' << std::endl;
        return false;
      }
    }

    (*map)[key] = cfgVal;
    return true;
  }

  std::string Config::print()
  {
    std::stringstream s;
    int maxLen = 0;
    for (auto it = inst.cfg.begin(); it != inst.cfg.end(); ++it)
    {
      int curLen = it->first.length();
      if (curLen > maxLen) maxLen = curLen;
    }
    s << "Configuration:" << std::endl;
    for (auto it = inst.cfg.begin(); it != inst.cfg.end(); ++it)
    {
      s << "  " << StringPad(it->first, maxLen);
      s << " = " << it->second.toString();
      s << std::endl;
    }
    return s.str();
  }

  Config Config::inst = Config();

} /* namespace penguinTrace */
