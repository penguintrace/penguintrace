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
// DWARF Frame Unwind Information

#ifndef DWARF_FRAME_H_
#define DWARF_FRAME_H_

#include <stdint.h>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <map>

#include <istream>
#include <iomanip>

#include "definitions.h"

#include "Common.h"
#include "../common/StreamOperations.h"

#include "AttrValue.h"

namespace penguinTrace
{
  namespace dwarf
  {

    class RegRule
    {
      public:
        enum regrule_t {
          UNDEFINED,
          SAME,
          OFFSET,
          VAL_OFFSET,
          REGISTER,
          EXPR,
          VAL_EXPR
        };
        RegRule(regrule_t type, std::string reg, AttrValue op) :
            type(type), reg(reg), operand(op)
        {

        }
        RegRule(regrule_t type, AttrValue op) :
            type(type), reg(""), operand(op)
        {

        }
        RegRule() = delete;
        std::string toString(bool cfa);
        std::string regName()
        {
          return reg;
        }
        AttrValue op()
        {
          return operand;
        }
        regrule_t getType()
        {
          return type;
        }
      private:
        regrule_t   type;
        std::string reg;
        AttrValue   operand;
    };

    class CFA
    {
      public:
        CFA(dwarf_t::cfa_t cfa, AttrValue op1, AttrValue op2) :
            cfa(cfa), operand1(op1), operand2(op2)
        {

        }
        CFA() = delete;
        static CFA parseCFA(std::istream& ifs, uint64_t offset);
        dwarf_t::cfa_t type()
        {
          return cfa;
        }
        AttrValue& op1()
        {
          return operand1;
        }
        AttrValue& op2()
        {
          return operand2;
        }
      private:
        dwarf_t::cfa_t cfa;
        AttrValue operand1;
        AttrValue operand2;
    };

    class FrameEntry
    {
      public:
        FrameEntry(uint64_t offset, std::list<CFA> instrs);
        FrameEntry() = delete;
        virtual ~FrameEntry();
        virtual std::string headerRepr() = 0;
        virtual std::string repr() = 0;
        std::list<CFA> getInstructions()
        {
          return instructions;
        }
      protected:
        uint64_t offset;
        std::list<CFA> instructions;
    };

    class CIE : public FrameEntry
    {
      public:
        CIE(uint64_t offset, uint64_t version, std::string aug,
            uint64_t codeAlign, int64_t dataAlign, uint64_t retAddrCol,
            uint8_t ptrRepr, std::list<CFA> instrs, uint64_t addrSize,
            uint64_t segmentSize, bool signal);
        CIE() = delete;
        virtual std::string headerRepr();
        virtual std::string repr();
        dwarf_t::eh_pe_t format()
        {
          return ptrFormat;
        }
        dwarf_t::eh_pe_t application()
        {
          return ptrApplication;
        }
        std::string getAugmentation()
        {
          return augmentation;
        }
        uint64_t getAddrSize()
        {
          return addrSize;
        }
        uint64_t getSegmentSize()
        {
          return segmentSize;
        }
        uint64_t getVersion()
        {
          return version;
        }
        uint64_t getDataAlign()
        {
          return dataAlign;
        }
        uint64_t getCodeAlign()
        {
          return codeAlign;
        }
        uint64_t raCol()
        {
          return returnAddrColumn;
        }
        bool getSignal()
        {
          return signal;
        }
      private:
        uint64_t version;
        std::string augmentation;
        uint64_t codeAlign;
        int64_t dataAlign;
        uint64_t returnAddrColumn;
        dwarf_t::eh_pe_t ptrFormat;
        dwarf_t::eh_pe_t ptrApplication;
        uint64_t addrSize;
        uint64_t segmentSize;
        bool signal;
    };

    class FDE : public FrameEntry
    {
      public:
        FDE(uint64_t offset, uint64_t ciePtr, CIE* cie, uint64_t start,
            uint64_t end, std::list<CFA> instrs);
        FDE() = delete;
        virtual std::string headerRepr();
        virtual std::string repr();
        uint64_t getStartAddr()
        {
          return startAddr;
        }
        bool contains(uint64_t pc)
        {
          return (pc >= startAddr) && (pc <= endAddr);
        }
        std::unique_ptr<std::map<std::string, RegRule> > unwindInfo(uint64_t pc);
      private:
        uint64_t ciePtr;
        CIE*     cie;
        uint64_t startAddr;
        uint64_t endAddr;
    };

    class Frames
    {
      public:
        Frames(std::istream& ifs, std::map<std::string, uint64_t>& sectionAddrs, bool eh);
        Frames();
        std::map<uint64_t, std::unique_ptr<FrameEntry> >& getFramesByOffset()
        {
          return frames;
        }
        FDE* getFDEByPc(uint64_t pc);
      private:
        std::map<uint64_t, std::unique_ptr<FrameEntry> > frames;
        std::map<uint64_t, FDE* > fdeByPc;
    };

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_FRAME_H_ */
