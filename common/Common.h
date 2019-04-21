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
// Common functions

#ifndef COMMON_COMMON_H_
#define COMMON_COMMON_H_

#include <assert.h>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <queue>
#include <vector>

#include <memory>

#include "Arch.h"
#include "Config.h"
#include "Exception.h"

namespace penguinTrace
{
  namespace files
  {
    typedef std::string Filename;
  }
  std::string rtrim(std::string& s);
  std::string trimWhitespace(std::string& s);
  std::vector<std::string> split(const std::string& s, char delim);
  std::vector<std::string> splitFirst(const std::string& s, char delim);
  std::vector<std::string> split(const std::string& s, const std::string& delim);
  std::string replaceAll(std::string s, const std::string& from, const std::string& to);

  inline std::string stdString(const char * c)
  {
    if (c == nullptr)
    {
      return std::string("");
    }
    else
    {
      return std::string(c);
    }
  }

  std::string jsonEscape(const std::string& s);

  std::string urlDecode(const std::string& s);

  std::string tryDemangle(std::string s);

  inline bool errorTryAgain(int err)
  {
    return (err == EAGAIN) || (err == EWOULDBLOCK);
  }

  inline bool errorOkNetError(int err)
  {
    return (err == ENETDOWN) ||
           (err == EPROTO) ||
           (err == ENOPROTOOPT) ||
           (err == EHOSTDOWN) ||
           (err == ENONET) ||
           (err == EHOSTUNREACH) ||
           (err == EOPNOTSUPP) ||
           (err == ENETUNREACH);
  }

  const int FD_STDIN = 0;
  const int FD_STDOUT = 1;
  const int FD_STDERR = 2;

  const int PIPE_RD = 0;
  const int PIPE_WR = 1;
  const int PIPE_NUM = 2;

  const unsigned ELF_MAGIC_LEN = 4;
  const unsigned char ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};

  const int ADDR_WIDTH_BYTES = 8;
  const int ADDR_WIDTH_CHARS = ADDR_WIDTH_BYTES*2;

  // Temporary 'session' before implementing actual sessions
  const std::string SINGLE_SESSION_NAME = "GLOBAL-SESSION";

  const std::string ELF_SOURCE_SECTION = "PENGUINTRACE.SRC";
  const std::string ELF_CONFIG_SECTION = "PENGUINTRACE.CFG";

  const int TEMP_FILE_NAME_BUF_LEN = 256;

  std::pair<int, std::string> getTempFile(std::string name, bool tempDir=true);
  std::pair<bool, std::string> getTempDir(std::string name);
  std::string getTempDir();

  bool fileExists(std::string name);

  enum StepType
  {
    STEP_INSTR,
    STEP_LINE,
    STEP_CONT
  };

  struct CompileFailureReason
  {
    public:
      CompileFailureReason(std::string c, std::string d, unsigned ln,
                         unsigned col)
          : category(c), description(d), locationValid(true), line(ln), column(
              col)
      {

      }
      CompileFailureReason(std::string c, std::string d)
          : category(c), description(d), locationValid(false), line(0), column(
              0)
      {

      }
      const std::string& getCategory() { return category; }
      const std::string& getDescription() { return description; }
      bool hasLocation() { return locationValid; }
      unsigned getLine() { return line; }
      unsigned getColumn() { return column; }
    private:
      std::string category;
      std::string description;
      bool locationValid;
      unsigned line;
      unsigned column;
  };

  inline void dualLoop(int max, int limit, std::function<void(int)> first, std::function<void()> second)
  {
    for (int i = 0; i < max; ++i)
    {
      if (i < limit)
      {
        first(i);
      }
      else
      {
        second();
      }
    }
  }

  bool writeWrap(int fd, std::string s);
  bool readWrap(int fd, std::stringstream* s);

  // Convert non-printing characters
  inline char makePrintable(char c, char alt)
  {
    return (c >= 0x20 && c <= 0x7e) ? c : alt;
  }

} /* namespace penguinTrace */

#endif /* COMMON_COMMON_H_ */
