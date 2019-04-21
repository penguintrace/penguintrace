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

#include "Compiler.h"

#include <thread>
#include <fstream>

#ifdef USE_LIBCLANG
#include <clang-c/Index.h>
#endif

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace penguinTrace
{

  Compiler::Compiler(std::string src, std::map<std::string, std::string> args,
      std::string argsStr, std::string file, ComponentLogger* l)
      :
      source(src), requestArgsString(argsStr), requestArgs(args), logger(l),
      reqLang(""), langExt(".c")
  {
    if (file.length() > 0)
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Temporary file created '";
        s << file;
        s << "'";
        return s.str();
      });

      execFilename = file;
    }
    else
    {
      std::stringstream s;
      s << "Failed to create a temporary file for compilation";
      compileFailures.push(CompileFailureReason("Internal Error", s.str()));
      logger->error(Logger::ERROR, "Temporary file creation");
    }
  }

  Compiler::~Compiler()
  {
  }

  uint64_t Compiler::execute()
  {
    bool ok = compileFailures.empty();
    auto langIt = requestArgs.find("lang");

    if (langIt != requestArgs.end())
    {
      reqLang = langIt->second;
      if (reqLang.compare("c") == 0)
      {
        compileName = Config::get(C_C_COMPILER_BIN).String();
        langExt = ".c";
        compilerCmds.push_back(const_cast<char*>(compileName.c_str()));
        compilerCmds.push_back(const_cast<char*>("-xc"));
      }
      else if (reqLang.compare("cxx") == 0)
      {
        compileName = Config::get(C_CXX_COMPILER_BIN).String();
        langExt = ".cpp";
        compilerCmds.push_back(const_cast<char*>(compileName.c_str()));
        compilerCmds.push_back(const_cast<char*>("-xc++"));
      }
      else if (reqLang.compare("asm") == 0)
      {
        compileName = Config::get(C_C_COMPILER_BIN).String();
        langExt = ".s";
        compilerCmds.push_back(const_cast<char*>(compileName.c_str()));
        compilerCmds.push_back(const_cast<char*>("-nostdlib"));
        compilerCmds.push_back(const_cast<char*>("-xassembler"));
      }
      else
      {
        ok = false;
        compileFailures.push(
            CompileFailureReason("Request Error",
                                 "Unknown language requested '"+reqLang+"'"));
      }
    }
    else
    {
      ok = false;
      compileFailures.push(
          CompileFailureReason("Request Error",
                               "No language specified"));
    }

    if (ok)
    {
      clangParse();
      // Compile with GCC/Clang
      return compile();
    }
    else
    {
      return 0;
    }
  }

  // First pass syntax check through libclang if available
  // Gives more detail about errors without having to parse GCC
  //  NOP if using not libclang
  void Compiler::clangParse()
  {
#ifdef USE_LIBCLANG
    std::string clang = Config::get(C_CLANG_BIN).String();
    const char* cClang = clang.c_str();

    CXIndex index = clang_createIndex(0, 0);
    std::vector<const char *> clangArgs;
    clangArgs.push_back(const_cast<char*>(cClang));
    if (Config::get(C_STRICT_MODE).Bool())
    {
      clangArgs.push_back("-Wall");
      clangArgs.push_back("-Werror");
    }

    std::string source_filename = "input"+langExt;
    std::string source_contents(source);

    CXUnsavedFile source;
    source.Filename = source_filename.c_str();
    source.Contents = source_contents.c_str();
    source.Length = source_contents.length();

    CXTranslationUnit tUnit;
    CXErrorCode parseError = clang_parseTranslationUnit2FullArgv(
        index, source_filename.c_str(), clangArgs.data(), clangArgs.size(), &source, 1, 0,
        &tUnit);
    if (clang_getNumDiagnostics(tUnit) > 0)
    {
      logger->log(Logger::INFO, "Unable to parse source");
      CXDiagnosticSet diags = clang_getDiagnosticSetFromTU(tUnit);
      for (unsigned i = 0; i < clang_getNumDiagnosticsInSet(diags); i++)
      {
        CXDiagnostic diag = clang_getDiagnosticInSet(diags, i);
        std::stringstream ss;
        CXSourceLocation diagLoc = clang_getDiagnosticLocation(diag);
        CXFile file;
        unsigned line,column,offset;
        clang_getFileLocation(diagLoc, &file, &line, &column, &offset);
        CXString desc = clang_getDiagnosticSpelling(diag);
        CXString cat = clang_getDiagnosticCategoryText(diag);

        CompileFailureReason err(clang_getCString(cat), clang_getCString(desc),
                               line, column);
        compileFailures.push(err);

        ss << line << ";" << column << " ";
        ss << clang_getCString(cat) << ": " << clang_getCString(desc);
        logger->log(Logger::INFO, ss.str());
        clang_disposeString(desc);
        clang_disposeString(cat);
        clang_disposeDiagnostic(diag);
      }
      clang_disposeDiagnosticSet(diags);
    }
    else if (parseError != CXError_Success)
    {
      std::string errReason = "Unknown";
      switch (parseError)
      {
        case CXError_Success: errReason = "How?"; break;
        case CXError_Failure: errReason = "Unknown Failure"; break;
        case CXError_Crashed: errReason = "libclang Crashed"; break;
        case CXError_InvalidArguments: errReason = "Invalid Args"; break;
        case CXError_ASTReadError: errReason = "AST Read Error"; break;
      }
      // No AST to read for assembly
      if (!((reqLang.compare("asm") == 0) && (parseError == CXError_ASTReadError)))
      {
        CompileFailureReason err("Compile Failed", errReason);
        compileFailures.push(err);
      }
    }

    clang_disposeTranslationUnit(tUnit);
    clang_disposeIndex(index);
#endif // USE_LIBCLANG
  }

  uint64_t Compiler::compile()
  {
    if (compileFailures.size() == 0)
    {
      return execCompile();
    }
    return 0;
  }

  bool Compiler::cmdWrap(std::vector<char *> args, std::string key)
  {
    int pOutToParent[PIPE_NUM];
    int pErrToParent[PIPE_NUM];

    if (pipe(pOutToParent) == -1)
    {
      std::string err = "Failed to open pipe";
      logger->error(Logger::ERROR, err);
      compileFailures.push(CompileFailureReason(key+" Failed", err));
      return false;
    }
    if (pipe(pErrToParent) == -1)
    {
      std::string err = "Failed to open pipe";
      logger->error(Logger::ERROR, err);
      compileFailures.push(CompileFailureReason(key+" Failed", err));
      return false;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
      std::string err = "Failed to fork child process";
      logger->error(Logger::ERROR, err);
      compileFailures.push(CompileFailureReason(key+" Failed", err));
      return false;
    }
    else if (pid == 0)
    {
      // In Child, connect to pipes
      dup2(pOutToParent[PIPE_WR], FD_STDOUT);
      dup2(pErrToParent[PIPE_WR], FD_STDERR);

      // Close FDs that the subprocess shouldn't see
      // E.g. the pipes themselves
      close(pOutToParent[PIPE_RD]);
      close(pOutToParent[PIPE_WR]);
      close(pErrToParent[PIPE_RD]);
      close(pErrToParent[PIPE_WR]);

      execv(args[0], args.data());
      logger->error(Logger::ERROR, "Failed to start child process");
      exit(-1);
    }

    close(pOutToParent[PIPE_WR]);
    close(pErrToParent[PIPE_WR]);

    std::thread stdoutThread(&Compiler::readFile, this, pOutToParent[PIPE_RD], &compileStdoutStream);
    std::thread stderrThread(&Compiler::readFile, this, pErrToParent[PIPE_RD], &compileStderrStream);

    bool done = false;
    int status;

    do
    {
      waitpid(pid, &status, 0);
      done = WIFEXITED(status) || WIFSIGNALED(status);
    }
    while (!done);

    stdoutThread.join();
    stderrThread.join();
    close(pOutToParent[PIPE_RD]);
    close(pErrToParent[PIPE_RD]);

    compileStdout = compileStdoutStream.str();
    compileStderr = compileStderrStream.str();

    bool ok = false;
    if (WIFEXITED(status))
    {
      // TODO may want to format better for UI
      std::stringstream s;
      s << key << " returncode=" << WEXITSTATUS(status);
      logger->log(Logger::DBG, s.str());
      if (WEXITSTATUS(status) != 0)
      {
        std::stringstream ss;
        ss << "RETURN: " << WEXITSTATUS(status) << std::endl;
        ss << "STDOUT:" << std::endl << compileStdout << std::endl;
        ss << "STDERR:" << std::endl << compileStderr << std::endl;
        for (auto cIt = args.begin(); cIt != args.end(); ++cIt)
        {
          if (*cIt != nullptr)
          {
            ss << *cIt << " ";
          }
        }
        compileFailures.push(CompileFailureReason(key+" Failed", ss.str()));
      }
      else
      {
        ok = true;
      }
    }
    else if (WIFSIGNALED(status))
    {
      std::stringstream s;
      s << key << " killed by signal: " << WTERMSIG(status);
      logger->log(Logger::DBG, s.str());
      std::stringstream ss;
      ss << "SIGNAL: " << strsignal(WTERMSIG(status)) << std::endl;
      ss << "STDOUT:" << std::endl << compileStdout << std::endl;
      ss << "STDERR:" << std::endl << compileStderr << std::endl;
      compileFailures.push(CompileFailureReason(key+" Killed", ss.str()));
    }
    return ok;
  }

  uint64_t Compiler::execCompile()
  {
    uint64_t pc = 0;
    bool ok = true;

    auto tempSrcFile = getTempFile(Config::get(C_TEMP_FILE_TPL).String()+"-src");
    auto tempCfgFile = getTempFile(Config::get(C_TEMP_FILE_TPL).String()+"-cfg");

    std::ofstream srcFile(tempSrcFile.second);
    srcFile << source;
    srcFile.close();

    std::ofstream cfgFile(tempCfgFile.second);
    cfgFile << requestArgsString;
    cfgFile.close();

    std::stringstream srcStr;
    srcStr << ELF_SOURCE_SECTION << "=" << tempSrcFile.second;
    std::string copySrcStr = srcStr.str();

    std::stringstream cfgStr;
    cfgStr << ELF_CONFIG_SECTION << "=" << tempCfgFile.second;
    std::string copyCfgStr = cfgStr.str();

    const char* cTmpSrcFile    = tempSrcFile.second.c_str();
    const char* tmpOutputFile  = execFilename.c_str();

    if (ok)
    {
      compilerCmds.push_back(const_cast<char*>(cTmpSrcFile));
      // Add debug information
      compilerCmds.push_back(const_cast<char*>("-g"));
      // Disable position independent executables
      // (so PC matches image)
      compilerCmds.push_back(const_cast<char*>("-fno-pie"));
      compilerCmds.push_back(const_cast<char*>("-no-pie"));
      if (Config::get(C_STRICT_MODE).Bool())
      {
        compilerCmds.push_back(const_cast<char*>("-Wall"));
        compilerCmds.push_back(const_cast<char*>("-Werror"));
      }
      else
      {
        // If using clang -w suppresses warnings
        //  GCC ignores this argument
        compilerCmds.push_back(const_cast<char*>("-w"));
      }
      //
      compilerCmds.push_back(const_cast<char*>("-o"));
      compilerCmds.push_back(const_cast<char*>(tmpOutputFile));
      compilerCmds.push_back(nullptr);

      ok &= cmdWrap(compilerCmds, "Compile");
    }

    if (ok)
    {
      std::string objcopy = Config::get(C_OBJCOPY_BIN).String();
      std::vector<char*> args;

      const char* cCopySrcArg = copySrcStr.c_str();
      const char* cCopyCfgArg = copyCfgStr.c_str();
      const char* cObjcopy    = objcopy.c_str();

      args.push_back(const_cast<char*>(cObjcopy));
      args.push_back(const_cast<char*>("--add-section"));
      args.push_back(const_cast<char*>(cCopySrcArg));
      args.push_back(const_cast<char*>("--add-section"));
      args.push_back(const_cast<char*>(cCopyCfgArg));
      args.push_back(const_cast<char*>(tmpOutputFile));
      args.push_back(nullptr);

      ok &= cmdWrap(args, "Embed Source");
    }

    tryUnlink(tempSrcFile.second);
    tryUnlink(tempCfgFile.second);

    return pc;
  }

  void Compiler::tryUnlink(std::string filename)
  {
    int ret = unlink(filename.c_str());

    if (ret == -1)
    {
      std::stringstream err;

      err << "Failed to delete temporary file '";
      err << filename;
      err << "'";

      logger->error(Logger::WARN, err.str());
    }
  }

  void Compiler::readFile(int fd, std::stringstream* stream)
  {
    int n;
    char buffer[512];
    int err;

    do
    {
      n = read(fd, buffer, sizeof(buffer));
      err = errno;
      if (n > 0)
      {
        stream->write(buffer, n);
      }
    }
    while (n > 0 && err == 0);

    if (err != 0)
    {
      logger->error(Logger::ERROR, "Error reading from compiler");
    }
  }

} /* namespace penguinTrace */
