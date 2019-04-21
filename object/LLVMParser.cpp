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
// LLVM Object File Parser wrapper

#ifdef USE_LIBLLVM
#include "LLVMParser.h"

#include "../common/StreamOperations.h"

namespace penguinTrace
{
  namespace object
  {

    LLVMParser::LLVMParser(std::string filename, std::unique_ptr<ComponentLogger> l)
      : Parser(filename), logger(std::move(l))
    {
      logger->log(Logger::DBG, [&]() {
        std::stringstream s;
        s << "Creating Parser for '" << filename << "'";
        return s.str();
      });
      objFile = nullptr;
    }

    LLVMParser::~LLVMParser()
    {
      close();
    }

    bool LLVMParser::parse()
    {
      LLVMMemoryBufferRef buffer;
      char *msg;

      if (LLVMCreateMemoryBufferWithContentsOfFile(filename.c_str(), &buffer, &msg) == 0)
      {
        objFile = LLVMCreateObjectFile(buffer);

        if (objFile == nullptr)
        {
          std::stringstream s;
          s << "Failed to parse object file";
          logger->log(Logger::ERROR, s.str());
          return false;
        }

        // Sections
        LLVMSectionIteratorRef sectionIt = LLVMGetSections(objFile);
        while (!LLVMIsSectionIteratorAtEnd(objFile, sectionIt))
        {
          uint64_t addr = LLVMGetSectionAddress(sectionIt);
          uint64_t size = LLVMGetSectionSize(sectionIt);
          const char * namePtr = LLVMGetSectionName(sectionIt);
          std::string name = penguinTrace::stdString(namePtr);

          auto dp = LLVMGetSectionContents(sectionIt);
          uint8_t* dataPtr = reinterpret_cast<uint8_t*>(const_cast<char*>(dp));

          // Don't know if a section is code from LLVM API
          SectionPtr section(new penguinTrace::object::Section(name, addr, size, true, dataPtr));

          if (name.length() > 0)
          {
            assert(sectionByName.find(name) == sectionByName.end());
            sectionByName[name] = section;
          }
          if (addr != 0)
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
          LLVMMoveToNextSection(sectionIt);
        }

        LLVMDisposeSectionIterator(sectionIt);

        LLVMSymbolIteratorRef symbolIt = LLVMGetSymbols(objFile);
         while (!LLVMIsSymbolIteratorAtEnd(objFile, symbolIt))
         {
           uint64_t addr = LLVMGetSymbolAddress(symbolIt);
           uint64_t size = LLVMGetSymbolSize(symbolIt);
           const char * namePtr = LLVMGetSymbolName(symbolIt);
           std::string name = penguinTrace::stdString(namePtr);

           SymbolPtr symbol(new penguinTrace::object::Symbol(name, addr, size));

           if (name != "")
           {
             symbolByName[name] = symbol;
           }
           if (addr != 0)
           {
             assert(symbolByAddr.find(addr) == symbolByAddr.end());
             symbolByAddr[addr] = symbol;
           }

           LLVMMoveToNextSymbol(symbolIt);
         }

         LLVMDisposeSymbolIterator(symbolIt);

        return true;
      }
      else
      {
        std::stringstream s;
        s << "Failed to get MemoryBuffer '" << msg << "'";
        logger->log(Logger::ERROR, s.str());
        LLVMDisposeMessage(msg);
        return false;
      }
    }

    void LLVMParser::close()
    {
      if (objFile != nullptr)
      {
        LLVMDisposeObjectFile(objFile);
      }
      objFile = nullptr;
    }

  } /* namespace object */
} /* namespace penguinTrace */
#endif
