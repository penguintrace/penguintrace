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

#include "Common.h"

#include <paths.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>

#include <cxxabi.h>

namespace penguinTrace
{
  std::string rtrim(std::string& s)
  {
    std::string r(s);
    if (!r.empty() && r[r.length()-1] == '\r')
    {
      r.erase(r.length()-1, 1);
    }
    return r;
  }

  std::string trimWhitespace(std::string& s)
  {
    std::string r(s);
    r.erase(r.find_last_not_of(" \t")+1, std::string::npos);
    return r.substr(r.find_first_not_of(" \t"), std::string::npos);
  }

  std::vector<std::string> split(const std::string& s, char delim)
  {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string str;
    while (std::getline(ss, str, delim))
    {
      res.push_back(str);
    }
    return res;
  }

  std::vector<std::string> splitFirst(const std::string& s, char delim)
  {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string str;
    std::getline(ss, str, delim);
    res.push_back(str);
    if (!ss.eof())
    {
      std::getline(ss, str, '\0');
      res.push_back(str);
    }
    else
    {
      res.push_back("");
    }
    return res;
  }

  std::vector<std::string> split(const std::string& s, const std::string& delim)
  {
    std::string scopy(s);
    std::vector<std::string> res;
    std::string str;
    size_t p = 0;
    while ((p = scopy.find(delim)) != std::string::npos)
    {
      str = scopy.substr(0, p);
      res.push_back(str);
      scopy.erase(0, p + delim.length());
    }
    res.push_back(scopy);
    return res;
  }

  std::string replaceAll(std::string s, const std::string& from, const std::string& to)
  {
    auto pos = s.find(from, 0);
    while (pos != std::string::npos)
    {
      s.replace(pos, from.length(), to);
      pos += to.length();
      pos = s.find(from, pos);
    }
    return s;
  }

  std::string jsonEscape(const std::string& s)
  {
    std::string res = replaceAll(s, R"(\)", R"(\\)");
    res = replaceAll(res, "\n", R"(\n)");
    res = replaceAll(res, R"(")", R"(\")");
    res = replaceAll(res, "\t", R"(\t)");
    res = replaceAll(res, "\r", R"(\r)");
    res = replaceAll(res, "\f", R"(\f)");
    res = replaceAll(res, "\b", R"(\b)");
    return res;
  }

  std::string urlDecode(const std::string& s)
  {
    std::stringstream in(s);
    std::stringstream out;

    while (!in.eof())
    {
      char c;
      in >> c;
      if (c == '%')
      {
        std::string hexchars;
        int i;
        in >> std::setw(2) >> hexchars;
        std::stringstream strm(hexchars);
        strm >> std::hex >> i;
        out << (char)i;
      }
      else
      {
        out << c;
      }
      in.peek();
    }

    return out.str();
  }

  std::string tryDemangle(std::string s)
  {
    // Version is unlikely to be useful so discard everything after '@@'
    auto ss = split(s, "@@");
    char* realname;
    int status = 1;
    size_t size = 0;
    realname = abi::__cxa_demangle(ss[0].c_str(), (char*)nullptr, &size, &status);
    if (status == 0 && realname != nullptr)
    {
      std::string n = std::string(realname);
      free(realname);
      return n;
    }
    else
    {
      return s;
    }
  }

  std::string getTempDir()
  {
    std::string dir = _PATH_TMP;

#ifdef P_tmpdir
    dir = P_tmpdir;
#endif

    char * envTmp = getenv("TMPDIR");
    if (envTmp != nullptr)
    {
      dir = envTmp;
    }

    char * iEnvTmp = getenv("PENGUINTRACE_TMPDIR");
    if (iEnvTmp != nullptr)
    {
      dir = iEnvTmp;
    }

    if (dir.length() > 0 && dir[dir.length()-1] != '/')
    {
      dir += '/';
    }

    return dir;
  }

  std::pair<bool, std::string> getTempDir(std::string name)
  {
    bool ok = false;
    std::string dname = "";

    std::string nameTpl = getTempDir() + name + "-XXXXXX";

    char nameTemplate[nameTpl.length()+1];
    nameTpl.copy(nameTemplate,nameTpl.length(),0);
    nameTemplate[nameTpl.length()] = '\0';

    char* dirName = mkdtemp(nameTemplate);

    if (dirName != NULL)
    {
      ok = true;
      dname = std::string(dirName);
    }

    return {ok, dname};
  }

  bool fileExists(std::string name)
  {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
  }

  bool writeWrap(int fd, std::string s)
  {
    bool ok = true;
    int offset = 0;
    int toWrite = s.length();
    while (toWrite > 0)
    {
      auto n = write(fd, s.c_str()+offset, toWrite);
      if (n < 0)
      {
        ok = false;
        toWrite = 0;
      }
      else
      {
        offset += n;
        toWrite -= n;
      }
    }
    return ok;
  }

  bool readWrap(int fd, std::stringstream* stream)
  {
    int n;
    char buffer[256];
    int err;

    do
    {
      errno = 0;
      n = read(fd, buffer, sizeof(buffer));
      err = errno;
      if (n > 0)
      {
        stream->write(buffer, n);
      }
    }
    while (n > 0 && err == 0);

    if (err != 0 && !(errorTryAgain(err) || (err == EIO)))
    {
      *stream << "Error reading from traced process #" << fd << "\n";
      return false;
    }
    return true;
  }

} /* namespace penguinTrace */
