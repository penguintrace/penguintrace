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
// Parser Base Class

#ifndef OBJECT_PARSER_H_
#define OBJECT_PARSER_H_

#include <map>
#include <memory>
#include <string>

#include "Section.h"
#include "Symbol.h"

namespace penguinTrace
{
  namespace object
  {
    class Parser
    {
      public:
        typedef std::shared_ptr<object::Section> SectionPtr;
        typedef std::shared_ptr<object::Symbol> SymbolPtr;
        typedef std::map<std::string, SectionPtr> SectionNameMap;
        typedef std::map<uint64_t, SectionPtr> SectionAddrMap;
        typedef std::map<std::string, SymbolPtr> SymbolNameMap;
        typedef std::map<uint64_t, SymbolPtr> SymbolAddrMap;

        Parser(std::string filename)
            : filename(filename)
        {

        }
        virtual ~Parser();
        virtual bool parse() = 0;
        virtual void close() = 0;
        // or return iterators/info struct?
        // iterators over sections (sorted by addr)
        // iterators over symbols
        // maintain series of offsets into file
        //
        //   return stream reference (positioned)
        // OR better create a substream class that can only access that section
        //
        // Combine two - return an info struct that contains:
        //   - if code
        //   - if loaded
        //   - if debug? etc...
        //   - section name
        //   - base virtual addr
        //   - substream accessor reference
        //   - symbol iterators
        //
        //virtual std::ifstream& getSection(std::string name);
        // map[section][addr] -> (symbol, offset)
        //virtual std::map<uint64_t, std::string> getSectionSymbols(std::string name);
        //virtual uint64_t getSectionAddr(std::string name);
        std::string getFilename()
        {
          return filename;
        }
        SectionNameMap& getSectionNameMap()
        {
          return sectionByName;
        }
        SectionAddrMap& getSectionAddrMap()
        {
          return sectionByAddr;
        }
        SymbolNameMap& getSymbolNameMap()
        {
          return symbolByName;
        }
        SymbolAddrMap& getSymbolAddrMap()
        {
          return symbolByAddr;
        }
      protected:
        std::string filename;
        SectionNameMap sectionByName;
        SectionAddrMap sectionByAddr;
        SymbolNameMap symbolByName;
        SymbolAddrMap symbolByAddr;
    };

  } /* namespace object */
} /* namespace penguinTrace */

#endif /* OBJECT_PARSER_H_ */
