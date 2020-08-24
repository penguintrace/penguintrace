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

#ifndef COMMON_CONFIG_H_
#define COMMON_CONFIG_H_

#include <cassert>
#include <map>
#include <string>
#include <sstream>
#include <functional>

namespace penguinTrace
{

  enum config_value_t
  {
    CFG_VAL_NULL,
    CFG_VAL_BOOL, // Boolean (bool)
    CFG_VAL_INT,  // Integer (int64_t)
    CFG_VAL_STR  // String  (std::string)
  };

  class CfgValue
  {
    public:
      CfgValue() :
          valueClass(CFG_VAL_NULL), boolValue(false), intValue(0),strValue("")
      {
      }
      CfgValue(bool value) :
          valueClass(CFG_VAL_BOOL), boolValue(value), intValue(0), strValue("")
      {
      }
      CfgValue(int64_t value) :
          valueClass(CFG_VAL_INT), boolValue(false), intValue(value), strValue("")
      {
      }
      CfgValue(std::string value) :
          valueClass(CFG_VAL_STR), boolValue(false), intValue(0), strValue(value)
      {
      }
      bool Bool()
      {
        assert((valueClass == CFG_VAL_BOOL) && "Value is not a boolean");
        return boolValue;
      }
      int64_t Int()
      {
        assert((valueClass == CFG_VAL_INT) && "Value is not an integer");
        return intValue;
      }
      std::string String()
      {
        assert((valueClass == CFG_VAL_STR) && "Value is not a string");
        return strValue;
      }
      config_value_t type()
      {
        return valueClass;
      }
      std::string typeStr()
      {
        switch (valueClass)
        {
          case CFG_VAL_BOOL: return "Boolean";
          case CFG_VAL_INT:  return "Integer";
          case CFG_VAL_STR:  return "String";
          default:           return "NULL";
        }
      }
      std::string toString();
      static CfgValue fromString(std::string s, bool forceString);
    private:
      config_value_t valueClass;
      bool          boolValue;
      int64_t      intValue;
      std::string   strValue;
  };

  typedef std::function<bool(CfgValue)> validator_t;

  // Create a validator using a regex
  validator_t RegexVal(std::string s);
  validator_t MaxVal(int i);

  class ConfigDefault
  {
    public:
      ConfigDefault(bool adv, CfgValue def, std::string desc) :
        advanced(adv), def(def), desc(desc),
        validator([](CfgValue c) {return true;})
      {
      }
      ConfigDefault(bool adv, CfgValue def, std::string desc, validator_t validator) :
        advanced(adv), def(def), desc(desc), validator(validator)
      {
      }
      bool getAdvanced()
      {
        return advanced;
      }
      CfgValue getDefault()
      {
        return def;
      }
      std::string getDesc()
      {
        return desc;
      }
      bool valid(CfgValue c)
      {
        return validator(c);
      }
      void showHelp() { advanced = false; }
    private:
      bool advanced;
      CfgValue def;
      std::string desc;
      validator_t validator;
  };

  class Config
  {
    public:
      static CfgValue get(std::string key);
      static void set(std::string key, int64_t i);
      static void set(std::string key, bool b);
      static void set(std::string key, std::string s);
      static bool parse(int argc, char **argv, bool needsFile, std::string desc);
      static std::string print();
      static void showHelp(std::string key);
    private:
      static void getPath(std::string key, std::string exe);
      static bool getPaths();
      static bool parseFile(std::map<std::string, CfgValue>* map, std::string fname);
      static bool attachToMap(std::map<std::string, CfgValue>* map,
          std::string key, std::string val);
      Config();
      static Config inst;
      std::map<std::string, CfgValue> cfg;
      static std::map<std::string, ConfigDefault > defaults;
      static void defaultsCapabilities(std::map<std::string, ConfigDefault >* m);
  };

  extern std::string C_C_COMPILER_BIN;
  extern std::string C_CXX_COMPILER_BIN;
  extern std::string C_RUSTC_COMPILER_BIN;
  extern std::string C_CLANG_BIN;
  extern std::string C_OBJCOPY_BIN;
  extern std::string C_LIB_DIRS;
  extern std::string C_SERVER_PORT;
  extern std::string C_DELETE_TEMP_FILES;
  extern std::string C_SERVER_GLOBAL;
  extern std::string C_SERVER_IPV6;
  extern std::string C_SINGLE_SESSION;
  extern std::string C_USE_PTY;
  extern std::string C_TEMP_FILE_TPL;
  extern std::string C_TEMP_DIR_TPL;
  extern std::string C_FILE_ARGUMENT;
  extern std::string C_AUTO_STEP;
  extern std::string C_EXEC_NAME;
  extern std::string C_ARGC_MAX;
  extern std::string C_CSTR_MAX_CHAR;
  extern std::string C_LOG_COLOUR;
  extern std::string C_LOG_STDERR;
  extern std::string C_LOG_FILE;
  extern std::string C_LOG_CFG_FILE;
  extern std::string C_LOG_TIME;
  extern std::string C_STEP_AFTER_MAIN;
  extern std::string C_PTR_DEPTH;
  extern std::string C_PRETTY_PRINT;
  extern std::string C_HIDE_NON_PRETTY_PRINT;
  extern std::string C_ISOLATE_TRACEE;
  extern std::string C_STRICT_MODE;

  //----------------------
  // Static configuration
  //----------------------

  // Web server definitions
  const int SERVER_QUEUE_SIZE = 4;
  const int SERVER_NUM_TRIES = 20;

  // Stepper configuration
  const bool STEPPER_STEP_OVER_LIBRARY_CALLS = true;
  const bool STEPPER_ALWAYS_SINGLE_STEP = true;

} /* namespace penguinTrace */

#endif /* COMMON_CONFIG_H_ */
