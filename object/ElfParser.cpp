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
// ELF File parser

#ifdef USE_ELF
#include "ElfParser.h"

#include <fstream>

namespace penguinTrace
{
  namespace object
  {

    ElfParser::ElfParser(std::string filename, std::unique_ptr<ComponentLogger> l)
      : Parser(filename), logger(std::move(l))
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Creating Parser for '" << filename << "'";
        return s.str();
      });
    }

    ElfParser::~ElfParser()
    {
    }

    bool ElfParser::parse()
    {
      std::ifstream elfStream;
      elfStream.open(filename, std::ios::in | std::ios::binary);

      if (!elfStream)
      {
        return false;
      }

      unsigned char eIdent[EI_NIDENT];

      elfStream.seekg(0);
      elfStream.read(reinterpret_cast<char*>(&eIdent), sizeof(eIdent));

      if (elfStream.gcount() != sizeof(eIdent) ||
          eIdent[EI_MAG0] != ELFMAG0 ||
          eIdent[EI_MAG1] != ELFMAG1 ||
          eIdent[EI_MAG2] != ELFMAG2 ||
          eIdent[EI_MAG3] != ELFMAG3)
      {
        return false;
      }

      if (eIdent[EI_CLASS] == ELFCLASS64)
      {
        return parseElf<ELFCLASS64>(elfStream);
      }
      else if (eIdent[EI_CLASS] == ELFCLASS32)
      {
        return parseElf<ELFCLASS32>(elfStream);
      }
      return false;
    }

    void ElfParser::close()
    {
    }

  } /* namespace object */
} /* namespace penguinTrace */

#endif /*USE_ELF*/
