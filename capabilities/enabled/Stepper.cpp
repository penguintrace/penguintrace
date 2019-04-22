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
// Tracee isolation setup (when enabled)

#include "../../debug/Stepper.h"

#include <ios>

#include <sched.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <ftw.h>

#include <string.h>
#include <unistd.h>

namespace penguinTrace
{

  const char* binaryName = "/exe";

  bool Stepper::isolateSetup()
  {
    auto tmpDir = getTempDir(Config::get(C_TEMP_DIR_TPL).String());
    if (!tmpDir.first)
    {
      logger->error(Logger::ERROR, "Failed to create temporary directory");
      return false;
    }
    else
    {
      tempDir = tmpDir.second;
      std::stringstream s;
      s << "Temporary directory for process: '" << tmpDir.second;
      s << "'" << std::endl;
      logger->log(Logger::INFO, s.str());
    }
    return true;
  }

  bool Stepper::writeToProc(std::string fname, std::string contents)
  {
    int fd, ret;
    fd = open(fname.c_str(), O_WRONLY);
    if (fd == -1)
    {
      std::stringstream s;
      s << "Failed to open " << fname << ". " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    ret = write(fd, contents.c_str(), contents.length());
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to write to " << fname << ". " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    ret = close(fd);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to close " << fname << ". " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    return true;
  }

  bool Stepper::isolateTracee()
  {
    if (!Config::get(C_ISOLATE_TRACEE).Bool()) return true;

    std::stringstream uidmap;
    std::stringstream gidmap;

    uidmap << "0 " << getuid() << " 1";
    gidmap << "0 " << getgid() << " 1";

    int ret;

    ret = unshare(CLONE_NEWUSER | CLONE_NEWNS);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to disassociate context. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }

    bool ok;
    ok = writeToProc("/proc/self/setgroups", "deny");
    if (!ok) return false;
    ok = writeToProc("/proc/self/uid_map", uidmap.str());
    if (!ok) return false;
    ok = writeToProc("/proc/self/gid_map", gidmap.str());
    if (!ok) return false;

    std::string newBinary = tempDir+binaryName;

    std::ifstream ifs = std::ifstream(filename, std::ios::binary);
    std::ofstream ofs = std::ofstream(newBinary, std::ios::binary);

    ofs << ifs.rdbuf();

    ifs.close();
    ofs.close();

    ret = chmod(newBinary.c_str(), S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret == -1)
    {
      writeWrap(oldStderr, "Failed to make temporary binary executable\n");
      return false;
    }

    for (auto d : mapDirs)
    {
      std::string libPath = tempDir+d.second;
      ret = mkdir(libPath.c_str(), 0775);
      if (ret == -1)
      {
        writeWrap(oldStderr, "Failed to make library directories\n");
        return false;
      }
    }

    for (auto dir : mapDirs)
    {
      if (dir.first && fileExists(dir.second))
      {
        std::string from = dir.second;
        std::string to   = tempDir+dir.second;
        // Mark RO in case underlying filesystem is - but may not take effect
        //  until remounted
        ret = mount(from.c_str(), to.c_str(), nullptr, MS_BIND|MS_RDONLY, nullptr);
        if (ret == -1)
        {
          std::stringstream s;
          s << "Failed to mount '" << to << "' " << strerror(errno) << std::endl;
          writeWrap(oldStderr, s.str());
          return false;
        }
        ret = mount(nullptr, to.c_str(), nullptr, MS_BIND|MS_REMOUNT|MS_RDONLY, nullptr);
        if (ret == -1)
        {
          std::stringstream s;
          s << "Failed to remount '" << to << "' readonly " << strerror(errno) << std::endl;
          writeWrap(oldStderr, s.str());
          return false;
        }
      }
    }

    // Map back to unprivileged
    // No need to remap user as capability to remap will be removed
    //  and defaults to nobody
    ret = unshare(CLONE_NEWUSER);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to disassociate user context. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }

    ret = chroot(tempDir.c_str());
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to chroot. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    ret = chdir("/");
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to chdir to new root. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }

    cap_t caps;
    caps = cap_get_proc();
    if (caps == nullptr)
    {
      std::stringstream s;
      s << "Failed to get capabilities. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    ret = cap_clear(caps);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to clear capabilities. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    ret = cap_set_proc(caps);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to set capabilities. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }
    ret = cap_free(caps);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to free capabilities pointer. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }

    ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (ret == -1)
    {
      std::stringstream s;
      s << "Failed to set 'no_new_privs' bit. " << strerror(errno) << std::endl;
      writeWrap(oldStderr, s.str());
      return false;
    }

    // Filename for this thread set to exe in chroot
    filename = binaryName;
    return true;
  }

  static int rmPath(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb)
  {
    if(remove(pathname) < 0)
    {
      std::stringstream s;
      s << "Failed to remove '" << pathname << "'";
      perror(s.str().c_str());
      return -1;
    }
    return 0;
  }

  void Stepper::tidyIsolation()
  {
    if (tempDir.length() > 0 && Config::get(C_DELETE_TEMP_FILES).Bool())
    {
      std::string binName = tempDir+binaryName;
      if (fileExists(binName))
      {
        int ret = unlink(binName.c_str());
        if (ret == -1)
        {
          logger->error(Logger::ERROR, "Failed to delete temporary binary");
        }
      }
      for (auto dIt = mapDirs.rbegin(); dIt != mapDirs.rend(); ++dIt)
      {
        std::string libPath = tempDir+dIt->second;
        int ret = rmdir(libPath.c_str());
        if (ret == -1 && errno != ENOENT)
        {
          std::stringstream s;
          s << "Failed to remove temporary dir: '" << libPath << "'";
          logger->error(Logger::ERROR, s.str());
        }
      }

      // Try and clean up any files/directories created by tracee
      int ret = nftw(tempDir.c_str(), rmPath, 8, FTW_DEPTH|FTW_MOUNT|FTW_PHYS);
      if (ret == -1)
      {
        std::stringstream s;
        s << "Failed to remove temporary dir: '" << tempDir << "'";
        logger->error(Logger::ERROR, s.str());
      }
      tempDir = "";
    }
    logger->log(Logger::INFO, "Destructing stepper");
  }

} /* namespace penguinTrace */
