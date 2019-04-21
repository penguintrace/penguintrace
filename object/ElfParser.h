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

#ifndef OBJECT_ELFPARSER_H_
#define OBJECT_ELFPARSER_H_
#ifdef USE_ELF
#include <elf.h>

#include <fstream>
#include <list>

#include "Parser.h"

#include "../common/ComponentLogger.h"
#include "../common/StreamOperations.h"

namespace penguinTrace
{
  namespace object
  {

    class ElfParser : public Parser
    {
      public:
        ElfParser(std::string filename, std::unique_ptr<ComponentLogger> l);
        virtual ~ElfParser();
        bool parse();
        void close();
      private:
        template<int N> class Types;
        template<typename T>
        T readStruct(std::ifstream &strm, uint64_t offset);
        template<int N> bool parseElf(std::ifstream& strm);

        std::map<std::string, std::unique_ptr<std::vector<uint8_t> > > sectionBuffers;

        std::unique_ptr<ComponentLogger> logger;
    };

    template <> class ElfParser::Types<ELFCLASS64>
    {
      public:
        typedef Elf64_Ehdr ElfHeader;
        typedef Elf64_Shdr SectionHeader;
        typedef Elf64_Phdr ProgHeader;
        typedef Elf64_Sym  Symbol;
    };

    template <> class ElfParser::Types<ELFCLASS32>
    {
      public:
        typedef Elf32_Ehdr ElfHeader;
        typedef Elf32_Shdr SectionHeader;
        typedef Elf32_Phdr ProgHeader;
        typedef Elf32_Sym  Symbol;
    };

    template<typename T>
    T ElfParser::readStruct(std::ifstream &strm, uint64_t offset)
    {
      T data;
      strm.seekg(offset);
      strm.read(reinterpret_cast<char*>(&data), sizeof(T));
      return data;
    }

    template<int N>
    bool ElfParser::parseElf(std::ifstream& strm)
    {
      typedef typename Types<N>::ElfHeader EHdr;
      typedef typename Types<N>::SectionHeader SHdr;
      typedef typename Types<N>::ProgHeader PHdr;
      typedef typename Types<N>::Symbol Sym;

      auto header = readStruct<EHdr>(strm, 0);

      std::stringstream s, s2;
      int nbits = N == ELFCLASS64 ? 64 : 32;

      s << "ELF: " << nbits << "-bit";
      s << " (";
      switch (header.e_ident[EI_DATA])
      {
        case ELFDATA2LSB:
          s << "little-endian";
          break;
        case ELFDATA2MSB:
          s << "big-endian";
          break;
        default:
          s << "INVALID";
          break;
      }
      s << ")" << std::endl;
      s << "  ";
      switch (header.e_ident[EI_OSABI])
      {
        case ELFOSABI_NONE:
          s << "Linux/UNIX System V";
          break;
        case ELFOSABI_LINUX:
          s << "Linux GNU ELF Extensions";
          break;
        case ELFOSABI_ARM:
          s << "ARM";
          break;
        case ELFOSABI_ARM_AEABI:
          s << "ARM EABI";
          break;
        default:
          s << "Other";
          break;
      }
      s << " ABI" << std::endl;
      s << "  ";
      switch (header.e_machine)
      {
        case EM_ARM:
          s << "ARM";
          break;
        case EM_AARCH64:
          s << "ARM AArch64";
          break;
        case EM_X86_64:
          s << "x86_64";
          break;
        default:
          s << "Other";
          break;
      }
      s << " Architecture" << std::endl;
      s << "  Entry Point = " << HexPrint(header.e_entry, 1);

      assert(sizeof(SHdr) == header.e_shentsize);
      assert(header.e_shstrndx < header.e_shentsize);

      uint64_t nameSHdrPos = header.e_shoff + (header.e_shstrndx * sizeof(SHdr));
      auto nameSHdr = readStruct<SHdr>(strm, nameSHdrPos);
      uint64_t nameSHdrOffset = nameSHdr.sh_offset;

      std::map<std::string, SHdr> sHeaders;
      std::list<PHdr> pHeaders;
      std::string symTable = ".symtab";
      std::string strTable = ".strtab";

      for (int i = 0; i < header.e_shnum; ++i)
      {
        uint64_t offset = header.e_shoff + (i * sizeof(SHdr));
        auto data = readStruct<SHdr>(strm, offset);
        strm.seekg(nameSHdrOffset + data.sh_name);
        std::string name = "";
        std::getline(strm, name, '\0');

        assert(sHeaders.find(name) == sHeaders.end());

        if (data.sh_type == SHT_SYMTAB)
        {
          symTable = name;
        }
        // Exclude Section Header string table and any allocated
        //  sections to exclude dynamic string section
        if ((data.sh_type == SHT_STRTAB) &
            (data.sh_offset != nameSHdr.sh_offset) &
            ((data.sh_flags & SHF_ALLOC) == 0))
        {
          strTable = name;
        }

        auto buffer = std::unique_ptr<std::vector<uint8_t> >(new std::vector<uint8_t>(data.sh_size));

        strm.seekg(data.sh_offset);
        strm.read(reinterpret_cast<char*>(buffer->data()), data.sh_size);

        sHeaders[name] = data;
        sectionBuffers.insert(std::make_pair(name, std::move(buffer)));
      }

      for (int i = 0; i < header.e_phnum; ++i)
      {
        uint64_t offset = header.e_phoff + (i * sizeof(PHdr));
        auto data = readStruct<PHdr>(strm, offset);
        pHeaders.push_back(data);
      }

      for (auto it = sHeaders.begin(); it != sHeaders.end(); ++it)
      {
        // Section pointer
        std::string name = it->first;
        SHdr        hdr  = it->second;
        uint64_t    addr = hdr.sh_addr;
        uint8_t* dataPtr = sectionBuffers.find(name)->second->data();
        bool isCode = false;
        bool threadLocal = (hdr.sh_flags & SHF_TLS) == SHF_TLS;

        // Get matching program header
        if (addr != 0)
        {
          for (auto it = pHeaders.begin(); it != pHeaders.end(); ++it)
          {
            uint64_t startAddr = it->p_vaddr;
            uint64_t endAddr = startAddr+it->p_memsz;

            if (addr >= startAddr && addr < endAddr)
            {
              isCode = (it->p_flags & PF_X) == PF_X;
              break;
            }
          }
        }

        SectionPtr section(new object::Section(name, addr, hdr.sh_size, isCode, dataPtr));

        if (name != "")
        {
          assert(sectionByName.find(name) == sectionByName.end());
          sectionByName[name] = section;
        }
        if (addr != 0 && !threadLocal)
        {
          if (sectionByAddr.find(addr) != sectionByAddr.end())
          {
            std::stringstream s;
            s << "Duplicate section: '";
            s << name << "' and '";
            s << sectionByAddr.find(addr)->second->getName() << "'";
            s << " @" << HexPrint(addr, 1);
            logger->log(Logger::WARN, s.str());
          }
          sectionByAddr[addr] = section;
        }
        s2 << "section = " << name << (isCode ? " (code)" : "") << std::endl;
      }

      assert((sHeaders[symTable].sh_size % sizeof(Sym)) == 0);
      uint64_t numSymbs = sHeaders[symTable].sh_size / sizeof(Sym);

      auto strBuf = sectionByName[strTable]->getContents();
      std::istream strStrm(&strBuf);

      for (unsigned i = 0; i < numSymbs; ++i)
      {
        uint64_t offset = sHeaders[symTable].sh_offset + (i * sizeof(Sym));
        auto data = readStruct<Sym>(strm, offset);

        uint64_t addr = data.st_value;

        strStrm.seekg(data.st_name);
        std::string name = "";
        std::getline(strStrm, name, '\0');

        SymbolPtr symbol(new penguinTrace::object::Symbol(name, addr, data.st_size));

        s2 << "symbol = " << HexPrint(addr, 1) << " " << name << std::endl;

        if (name != "")
        {
          symbolByName[name] = symbol;
        }
        if (addr != 0)
        {
          auto symbIt = symbolByAddr.find(addr);
          if (symbIt != symbolByAddr.end())
          {
            std::string oldName = symbIt->second->getName();
            std::string demangle = tryDemangle(name);
            std::string oldDemangle = tryDemangle(oldName);


            if ((oldName.length() == 0) ||
                (name == "main") ||
                (name == "_start"))
            {
              // Want to keep if main/_start or unnamed
              symbolByAddr[addr] = symbol;
            }
            else if ((name.length() == 0) ||
                     (name == oldName) ||
                     (oldName == "main") ||
                     (oldName == "_start"))
            {
              // Ignore new if empty or same symbol
              // Or was 'main' or '_start' to preserve entry point
            }
            else if (demangle == oldDemangle)
            {
              // Different constructors/deconstructors may be mapped to the
              //  same symbol if the 'base object con/destructor' and 'complete
              //  object con/destructor' are the same (no virtual base classes)
              bool ok = false;
              if ((name.length() > 4) && (oldName.length() > 4))
              {
                std::vector<std::vector<std::string> > toCut;
                toCut.push_back({"D1E", "D2E"});
                toCut.push_back({"C1E", "C2E"});

                for (auto it = toCut.begin(); it != toCut.end(); ++it)
                {
                  std::string sold = oldName;
                  std::string snew = name;
                  for (auto iit = it->begin(); iit != it->end(); ++iit)
                  {
                    std::string s = *iit;
                    if (sold.find(s) != std::string::npos) sold.replace(sold.find(s), s.length(), "");
                    if (snew.find(s) != std::string::npos) snew.replace(snew.find(s), s.length(), "");
                  }
                  if (sold == snew) ok = true;
                }
              }

              if (!ok)
              {
                std::stringstream s;
                s << "Duplicate symbol: '";
                s << name << "' and '";
                s << oldName << "'";
                s << " @" << HexPrint(addr, 1);
                logger->log(Logger::TRACE, s.str());
              }
            }
            else
            {
              std::stringstream s;
              s << "Duplicate symbol: '";
              s << demangle << "' and '";
              s << oldDemangle << "'";
              s << " @" << HexPrint(addr, 1);
              logger->log(Logger::TRACE, s.str());
            }
          }
          else
          {
            symbolByAddr[addr] = symbol;
          }
        }
      }

      strStrm.seekg(0);

      logger->log(Logger::DBG, s.str());
      logger->log(Logger::TRACE, s2.str());

      return true;
    }

  } /* namespace object */
} /* namespace penguinTrace */

#endif /* USE_ELF */
#endif /* OBJECT_ELFPARSER_H_ */
