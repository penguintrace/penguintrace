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
// DWARF Information Wrapper

#ifndef DWARF_INFO_H_
#define DWARF_INFO_H_

#include <list>

#include "definitions.h"
#include "Abbrev.h"
#include "LineProgram.h"
#include "Frame.h"
#include "DIE.h"

#include "../object/Parser.h"
#include "../common/ComponentLogger.h"

namespace penguinTrace
{
  namespace dwarf
  {

    class Info
    {
      public:
        struct CodeLocation
        {
          public:
            CodeLocation()
              : fnd(false), p(0), ln(0), col(0), fname(""), pEnd(false), eBegin(false) {}
            CodeLocation(uint64_t p, std::string f, uint64_t l, uint64_t c, bool end, bool ep)
              : fnd(true), p(p), ln(l), col(c), fname(f), pEnd(end), eBegin(ep) {}
            bool found() { return fnd; }
            uint64_t pc() { return p; }
            uint64_t line() { return ln; }
            uint64_t column() { return col; }
            std::string filename() { return fname; }
            bool prologueEnd() { return pEnd; }
            bool epilogueBegin() { return eBegin; }
          private:
            bool fnd;
            uint64_t p;
            uint64_t ln;
            uint64_t col;
            std::string fname;
            bool pEnd;
            bool eBegin;
        };

        Info(penguinTrace::object::Parser* p, std::unique_ptr<ComponentLogger> logger);
        virtual ~Info();

        void printInfo();
        CodeLocation locationByPC(uint64_t addr, bool matchSrc);
        CodeLocation exactLocationByPC(uint64_t addr);
        CodeLocation exactLocationByLine(uint64_t line);
        DIE* functionByPC(uint64_t addr)
        {
          if (!parsed) parse();
          return info->functionContaining(addr);
        }
        void dataObjects(uint64_t pc, std::list<DIE*>& list)
        {
          if (!parsed) parse();
          DIE* func = info->functionContaining(pc);
          if (func != nullptr) func->dataObjects(pc, list);
        }
        void formalParams(uint64_t pc, std::list<DIE*>& list)
        {
          if (!parsed) parse();
          DIE* func = info->functionContaining(pc);
          if (func != nullptr) func->formalParams(pc, list);
        }
        bool inFunctionPrologue(uint64_t pc);
        uint64_t nextLinePC(uint64_t pc)
        {
          if (!parsed) parse();

          auto it = locationByPc.upper_bound(pc);
          if (it != locationByPc.end())
          {
            return it->second.pc();
          }

          return 0;
        }
        FDE* getFDE(uint64_t pc)
        {
          if (!parsed) parse();

          return frames->getFDEByPc(pc);
        }
      private:
        typedef std::map<dwarf_t::section_t, penguinTrace::object::Parser::SectionPtr> SectionMap;
        void parseSections();
        void parse();
        void parseAbbrev();
        void parseInfo();
        void recurseInfoAttrs(DIE* d);
        void parseLineSection();
        void parseFrame();
        void addLineStateMachine(LineStateMachine lsm, LineProgramHeader* hdr);
        std::string ExtractStrp(arch_t arch, std::istream& is);
        bool parsed;
        bool parsedLine;
        bool parsedSections;
        penguinTrace::object::Parser* parser;
        std::string cuSrcName;
        std::unique_ptr<ComponentLogger> logger;
        SectionMap sections;
        std::map<std::string, uint64_t> sectionAddrs;
        std::shared_ptr<Frames> frames;
        std::map<uint64_t, CodeLocation> locationByPc;
        std::map<uint64_t, CodeLocation> locationByLine;
        std::map<uint64_t, std::map<uint64_t, AbbrevTableEntry> > abbrevTable;
        std::unique_ptr<DIE> info;
    };

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_INFO_H_ */
