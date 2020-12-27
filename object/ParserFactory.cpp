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
// Parser Factory

#include "ParserFactory.h"

#include <memory>

#include "../common/Common.h"

#ifdef USE_ELF
#include "ElfParser.h"
#endif

#ifdef USE_LIBLLVM
#include "LLVMParser.h"
#endif

#include <fstream>

namespace penguinTrace
{
  namespace object
  {

    std::unique_ptr<Parser> ParserFactory::getParser(std::string filename, std::unique_ptr<ComponentLogger> logger)
    {
      Parser* parser = nullptr;
#ifdef USE_ELF
#ifdef USE_LIBLLVM
      // Have ELF and LIBLLVM, prioritise ELF if magic number matches
      std::ifstream ifs(filename);
      unsigned char elf_magic[ELF_MAGIC_LEN];
      bool isElf = false;

      ifs.seekg(0);
      ifs.read(reinterpret_cast<char*>(&elf_magic), sizeof(elf_magic));

      if (ifs.gcount() == sizeof(elf_magic))
      {
        isElf = true;
        for (unsigned i = 0; i < ELF_MAGIC_LEN; ++i)
        {
          isElf &= (elf_magic[i] == ELF_MAGIC[i]);
        }
      }

      ifs.close();

      if (isElf)
      {
        parser = new ElfParser(filename,std::move(logger));
      }
      else
      {
        parser = new LLVMParser(filename,std::move(logger));
      }
#else // !USE_LIBLLVM
      // Have ELF only
      parser = new ElfParser(filename,std::move(logger));
#endif // USE_LIBLLVM
#else // !USE_ELF
#ifdef USE_LIBLLVM
      // Have LIBLLVM only
      parser = new LLVMParser(filename,std::move(logger));
#endif // USE_LIBLLVM
#endif // USE_ELF
      return std::unique_ptr<Parser>(parser);
    }

  } /* namespace object */
} /* namespace penguinTrace */
