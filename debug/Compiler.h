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
// penguinTrace Compiler

#ifndef DEBUG_COMPILER_H_
#define DEBUG_COMPILER_H_

#include "../common/ComponentLogger.h"

#include <queue>
#include <map>

namespace penguinTrace
{
  class Compiler
  {
    public:
      Compiler(std::string src, std::map<std::string, std::string> args,
          std::string argsStr, std::string file, ComponentLogger* l);
      virtual ~Compiler();
      uint64_t execute();
      std::queue<CompileFailureReason>& failures()
      {
        return compileFailures;
      }
    private:
      void clangParse();
      uint64_t compile();
      uint64_t execCompile();
      bool cmdWrap(std::vector<char *> args, std::string key);
      void readFile(int fd, std::stringstream* stream);
      void tryUnlink(std::string filename);
      void disass();
      std::string source;
      std::string requestArgsString;
      std::map<std::string, std::string> requestArgs;
      ComponentLogger* logger;
      std::queue<CompileFailureReason> compileFailures;
      std::stringstream compileStdoutStream;
      std::stringstream compileStderrStream;
      std::string compileStdout;
      std::string compileStderr;
      std::string execFilename;
      std::string reqLang;
      std::string langExt;
      std::string compileName;
      std::vector<char*> compilerCmds;
  };

} /* namespace penguinTrace */

#endif /* DEBUG_COMPILER_H_ */
