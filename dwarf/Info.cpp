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

#include "Info.h"

#include <sstream>

#include "../common/StreamOperations.h"

namespace penguinTrace
{
  namespace dwarf
  {

    Info::Info(penguinTrace::object::Parser* p,
        std::unique_ptr<ComponentLogger> logger) :
        parsed(false), parsedLine(false), parsedSections(false),
        parser(p), cuSrcName(""), logger(std::move(logger))
    {

    }

    Info::~Info()
    {
    }

    void Info::printInfo()
    {
      // TODO best approach?
      if (!parsed) parse();

      logger->log(Logger::DBG, "DWARF Sections:");

      for (auto it = sections.begin(); it != sections.end(); ++it)
      {
        std::stringstream s;
        s << "  " << dwarf_t::convert_section(it->first);
        logger->log(Logger::DBG, s.str());
      }

      logger->log(Logger::DBG, "Section Addrs:");

      for (auto it = sectionAddrs.begin(); it != sectionAddrs.end(); ++it)
      {
        std::stringstream s;
        s << "  " << it->first << " = " << HexPrint(it->second, 8);
        logger->log(Logger::DBG, s.str());
      }

      logger->log(Logger::DBG, "Line Program:");

      unsigned lineMax = 9;
      for (auto it : locationByPc)
      {
        lineMax = (it.second.filename().size() > lineMax) ?
            it.second.filename().size() : lineMax;
      }
      std::stringstream lnHdr;
      lnHdr << StringPad("PC", 11);
      lnHdr << StringPad("Filename", lineMax);
      lnHdr << StringPad("Line", 5);
      lnHdr << StringPad("Column", 5);
      logger->log(Logger::DBG, lnHdr.str());

      for (auto it : locationByPc)
      {
        std::stringstream s;
        s << HexPrint(it.second.pc(), 8) << ' ';
        s << StringPad(it.second.filename(), lineMax);
        s << std::setw(5) << it.second.line();
        s << std::setw(5) << it.second.column();

        logger->log(Logger::DBG, s.str());
      }

      std::stringstream lnHdr2;
      lnHdr2 << StringPad("Line", 5);
      lnHdr2 << ' ';
      lnHdr2 << StringPad("PC", 11);
      logger->log(Logger::DBG, lnHdr2.str());

      for (auto it : locationByLine)
      {
        std::stringstream s;
        s << std::setw(5) << it.second.line();
        s << ' ';
        s << HexPrint(it.second.pc(), 8);
        logger->log(Logger::DBG, s.str());
      }

      logger->log(Logger::DBG, "Abbreviations:");

      // Print out AbbrevTable
      for (auto it = abbrevTable.begin(); it != abbrevTable.end(); ++it)
      {
        std::stringstream s;
        s << "Compilation Unit @";
        s << HexPrint(it->first, 2) << std::endl;

        for (auto jt = it->second.begin(); jt != it->second.end(); ++jt)
        {
          auto attrs = jt->second.GetAttrs();

          s << "  Abbrev Code <" << jt->first << "> ";
          s << dwarf_t::tag_str(jt->second.GetTag()) << " ";
          s << dwarf_t::children_str(jt->second.HasChildren());
          s << std::endl;

          for (auto kt = attrs.begin(); kt != attrs.end(); ++kt)
          {
            s << "    " << StringPad(dwarf_t::at_str(kt->GetAT()), 24);
            s << " " << dwarf_t::form_str(kt->GetForm()) << std::endl;
          }
        }
        logger->log(Logger::DBG, s.str());
      }

      logger->log(Logger::DBG, "DIEs:");

      logger->log(Logger::DBG, info->toString());

      logger->log(Logger::DBG, "");
      logger->log(Logger::DBG, "Frames:");

      std::map<uint64_t, std::unique_ptr<FrameEntry> >& frameEntries =
          frames->getFramesByOffset();

      for (auto it = frameEntries.begin(); it != frameEntries.end(); ++it)
      {
        std::stringstream s;
        s << "<" << HexPrint(it->first, 3) << ">";
        s << " ";
        s << it->second->repr();
        logger->log(Logger::DBG, s.str());
      }
    }

    void Info::parseSections()
    {
      auto allSections = parser->getSectionNameMap();

      for (auto it = allSections.begin(); it != allSections.end(); ++it)
      {
        dwarf_t::section_t section = dwarf_t::convert_section(it->first);

        if (section != dwarf_t::DW_SECTION_NULL)
        {
          sections[section] = it->second;
        }

        sectionAddrs[it->first] = it->second->getAddress();
      }
      parsedSections = true;
    }


    void Info::parse()
    {
      if (!parsedSections) parseSections();
      if (!parsedLine) parseLineSection();

      parseAbbrev();
      parseFrame();
      parseInfo();

      auto cuDie = info->getFirstChild();
      if (cuDie != nullptr)
      {
        if (cuDie->getTag() == dwarf_t::DW_TAG_compile_unit && cuDie->hasName())
        {
          cuSrcName = cuDie->getName();
        }
      }

      for (auto it : locationByPc)
      {
        if (it.second.filename() == cuSrcName)
        {
          // Only store first location by line
          if (locationByLine.find(it.second.line()) == locationByLine.end())
          {
            locationByLine.insert({it.second.line(), it.second});
          }
        }
      }

      parsed = true;
    }

    std::string Info::ExtractStrp(arch_t arch, std::istream& is)
    {
      uint64_t offset = ExtractSectionOffset(is, arch);

      auto it = sections.find(dwarf_t::DW_SECTION_str);
      assert(it != sections.end() && "DW_FORM_strp without .debug_str");
      auto iStrm = it->second->getContents();
      std::istream istr(&iStrm);

      istr.seekg(offset);

      return ExtractString(istr);
    }

    Info::CodeLocation Info::locationByPC(uint64_t pc, bool matchSrc)
    {
      if (!parsedLine) parseLineSection();

      auto it = locationByPc.find(pc);

      if (it != locationByPc.end())
      {
        if (matchSrc && cuSrcName.size() > 0 && it->second.filename() != cuSrcName) return CodeLocation();
        return it->second;
      }
      else
      {
        it = locationByPc.lower_bound(pc);
        if (it != locationByPc.end() && it != locationByPc.begin())
        {
          --it;
          if (matchSrc && cuSrcName.size() > 0 && it->second.filename() != cuSrcName) return CodeLocation();
          return it->second;
        }
      }

      return CodeLocation();
    }

    Info::CodeLocation Info::exactLocationByPC(uint64_t pc)
    {
      if (!parsedLine) parseLineSection();

      auto it = locationByPc.find(pc);

      if (it != locationByPc.end())
      {
        if (cuSrcName.size() > 0 && it->second.filename() != cuSrcName) return CodeLocation();
        return it->second;
      }

      return CodeLocation();
    }

    Info::CodeLocation Info::exactLocationByLine(uint64_t line)
    {
      if (!parsed) parse();

      // Location by line only in source

      auto it = locationByLine.find(line);

      if (it != locationByLine.end())
      {
        return it->second;
      }

      return CodeLocation();
    }

    void Info::parseAbbrev()
    {
      if (sections.find(dwarf_t::DW_SECTION_abbrev) == sections.end())
      {
        logger->log(Logger::ERROR, "No .debug_abbrev section in binary");
        return;
      }
      assert(sections.find(dwarf_t::DW_SECTION_abbrev) != sections.end());

      auto abbrevSection = sections[dwarf_t::DW_SECTION_abbrev];
      auto abbrevStream = abbrevSection->getContents();
      std::istream is(&abbrevStream);

      bool done = false;

      uint64_t cuIndex = 0;

      while (!done)
      {
        // Done for compilation unit
        bool cuDone = false;
        uint64_t cuOffset = is.tellg();

        // Create map for this CU
        abbrevTable[cuOffset];

        while (!cuDone)
        {
          uint64_t abbrevCode = ExtractULEB128(is);

          if (abbrevCode == 0)
          {
            cuDone = true;
            continue;
          }

          uint64_t abbrevTag = ExtractULEB128(is);
          dwarf_t::tag_t tag = dwarf_t::convert_tag(abbrevTag);

          uint8_t abbrevHasChildren = is.get();
          dwarf_t::children_t hasChildren = dwarf_t::convert_children(abbrevHasChildren);

          auto it = abbrevTable[cuOffset].insert(
              {abbrevCode, AbbrevTableEntry(tag, hasChildren)}
          );

          AbbrevTableEntry* entry = &(it.first->second);

          bool entryDone = false;

          while (!entryDone)
          {
            uint64_t attribName = ExtractULEB128(is);
            uint64_t attribForm = ExtractULEB128(is);
            dwarf_t::at_t name = dwarf_t::convert_at(attribName);
            dwarf_t::form_t form = dwarf_t::convert_form(attribForm);

            entryDone = (attribName == 0) && (attribForm == 0);

            if (!entryDone)
            {
              // Add attrib to entry
              entry->AddAttrib( AbbrevAttrib(name, form) );
            }
          }
        }

        cuIndex++;

        is.peek();
        done = is.eof();
      }
    }

    void Info::parseInfo()
    {
      if (sections.find(dwarf_t::DW_SECTION_info) == sections.end())
      {
        logger->log(Logger::ERROR, "No .debug_info section in binary");
        return;
      }
      assert(sections.find(dwarf_t::DW_SECTION_info) != sections.end());

      auto infoSection = sections[dwarf_t::DW_SECTION_info];
      auto infoStream = infoSection->getContents();
      std::istream is(&infoStream);

      is.peek();

      int cuIndex = 0;

      std::shared_ptr<std::map<uint64_t, DIE*> > dies(new std::map<uint64_t, DIE*>());
      std::shared_ptr<ComponentLogger> dieLogger = logger->subLogger("DIE");
      DIE* currentDie = new DIE(0, dwarf_t::DW_TAG_NULL, nullptr, dies, frames, DWARF64, dieLogger);

      while (!is.eof())
      {
        CompilationUnitHeader header(is);

        assert(header.Version() > 2 && "Only DWARFv3/4 supported");

        auto cuAbbrevTblIt = abbrevTable.find(header.AbbrevOffset());
        assert(cuAbbrevTblIt != abbrevTable.end());
        auto cuAbbrevTbl = cuAbbrevTblIt->second;

        bool cuDone = !header.Contains(is.tellg());

        while (!cuDone)
        {
          assert((uint64_t)is.tellg() > header.CUHeaderStart());
          // DIE offsets are not relative to header
          uint64_t dieOffset = (uint64_t)is.tellg();
          uint64_t abbrevCode = ExtractULEB128(is);

          if (abbrevCode == 0)
          {
            if (currentDie->getParent() != nullptr)
            {
              // Pop - replace current DIE with parent
              currentDie = currentDie->getParent();
            }
          }
          else
          {
            auto abbrevEntryIt = cuAbbrevTbl.find(abbrevCode);
            if (abbrevEntryIt == cuAbbrevTbl.end())
            {
              logger->log(Logger::ERROR, "Cannot parse debug information (missing abbreviation for DIE)");
              DIE * nullDIE = new DIE(0, dwarf_t::DW_TAG_NULL, nullptr, dies, frames, DWARF64, dieLogger);
              info = std::unique_ptr<DIE>(nullDIE);
              return;
            }
            else
            {
              assert(abbrevEntryIt != cuAbbrevTbl.end());
              AbbrevTableEntry& abbrevEntry = abbrevEntryIt->second;

              bool nextHasChildren = abbrevEntry.HasChildren() == dwarf_t::DW_CHILDREN_yes;

              DIE* die = new DIE(dieOffset, abbrevEntry.GetTag(), currentDie, dies, frames, header.Arch(), dieLogger);
              dies->insert( {dieOffset, die} );

              for (auto it = abbrevEntry.GetAttrs().begin();
                  it != abbrevEntry.GetAttrs().end(); ++it)
              {
                AttrValue val;
                std::vector<uint8_t> tmpBuffer;

                switch (it->GetForm())
                {
                  case dwarf_t::DW_FORM_strp:
                    val = AttrValue(it->GetForm(), ExtractStrp(header.Arch(), is));
                    break;
                  case dwarf_t::DW_FORM_string:
                    val = AttrValue(it->GetForm(), ExtractString(is));
                    break;
                  case dwarf_t::DW_FORM_udata:
                    // Data length is based on maximum
                    val = AttrValue(it->GetForm(), ExtractULEB128(is), 8, true);
                    break;
                  case dwarf_t::DW_FORM_sdata:
                    // Data length is based on maximum
                    val = AttrValue(it->GetForm(), ExtractSLEB128(is), 8, true);
                    break;
                  case dwarf_t::DW_FORM_data1:
                    val = AttrValue(it->GetForm(), ExtractUInt8(is), 1, false);
                    break;
                  case dwarf_t::DW_FORM_data2:
                    val = AttrValue(it->GetForm(), ExtractUInt16(is), 2, false);
                    break;
                  case dwarf_t::DW_FORM_data4:
                    val = AttrValue(it->GetForm(), ExtractUInt32(is), 4, false);
                    break;
                  case dwarf_t::DW_FORM_data8:
                    val = AttrValue(it->GetForm(), ExtractUInt64(is), 8, false);
                    break;
                  case dwarf_t::DW_FORM_addr:
                    val = AttrValue(it->GetForm(), ExtractNumBytes(is, header.AddrBytes()),
                        header.AddrBytes(), false);
                    break;
                  case dwarf_t::DW_FORM_ref_addr:
                    // In DWARFv2, size of address on target
                    // In DWARFv3/4, depends on DWARF arch
                    if (header.Version() == 2)
                    {
                      val = AttrValue(it->GetForm(), ExtractNumBytes(is, sizeof(void*)),
                          sizeof(void*), false);
                    }
                    else
                    {
                      val = AttrValue(it->GetForm(), ExtractSectionOffset(is, header.Arch()),
                          ((header.Arch() == dwarf::DWARF64) ? 8 : 4), false);
                    }
                    break;
                  case dwarf_t::DW_FORM_sec_offset:
                    val = AttrValue(it->GetForm(), ExtractSectionOffset(is, header.Arch()),
                        ((header.Arch() == dwarf::DWARF64) ? 8 : 4), false);
                    break;
                  case dwarf_t::DW_FORM_flag:
                    val = AttrValue(it->GetForm(), ExtractUInt8(is) != 0);
                    break;
                  case dwarf_t::DW_FORM_flag_present:
                    val = AttrValue(it->GetForm(), true);
                    break;
                  case dwarf_t::DW_FORM_ref4:
                    val = AttrValue(it->GetForm(), ExtractUInt32(is)+header.CUHeaderStart(), 4, false);
                    break;
                  case dwarf_t::DW_FORM_block1:
                    ExtractBlock(is, tmpBuffer, 1);
                    val = AttrValue(it->GetForm(), tmpBuffer);
                    break;
                  case dwarf_t::DW_FORM_exprloc:
                    ExtractExprLoc(is, tmpBuffer);
                    val = AttrValue(it->GetForm(), tmpBuffer);
                    break;
                  default:
                    throw Exception("Unhandled: "+dwarf_t::form_str(it->GetForm()), __EINFO__);
                }

                die->addAttribute(it->GetAT(), val);
              }

              currentDie->addChild(std::unique_ptr<DIE>(die));
              if (nextHasChildren)
              {
                currentDie = die;
              }
            }
          }

          cuDone = !header.Contains(is.tellg());
        }
        cuIndex++;
        is.peek();
      }

      info = std::unique_ptr<DIE>(currentDie);
    }

    void Info::parseLineSection()
    {
      if (!parsedSections) parseSections();

      if (sections.find(dwarf_t::DW_SECTION_line) == sections.end())
      {
        logger->log(Logger::ERROR, "No .debug_line section in binary");
        return;
      }
      assert(sections.find(dwarf_t::DW_SECTION_line) != sections.end());

      auto lineSection = sections[dwarf_t::DW_SECTION_line];
      auto lineStream = lineSection->getContents();
      std::istream is(&lineStream);

      is.peek();
      while (!is.eof())
      {
        auto lineProgram = std::unique_ptr<LineProgramHeader>(new LineProgramHeader(is));
        LineStateMachine state(*lineProgram);

        is.peek();
        while (!is.eof() && lineProgram->Contains(is.tellg()))
        {
          uint8_t firstByte = ExtractUInt8(is);

          if (is.eof())
          {
            break;
          }

          if (firstByte == 0x00)
          {
            // Extended Opcode
            // length(ULEB128), opcode(byte), data(length - 1)
            uint64_t length = ExtractULEB128(is);
            uint64_t dataLength = length-1;
            uint8_t opval = ExtractUInt8(is);
            dwarf_t::lne_t op = dwarf_t::convert_lne(opval);

            switch (op)
            {
              case dwarf_t::DW_LNE_end_sequence:
                addLineStateMachine(state.EndSequence(), lineProgram.get());
                break;
              case dwarf_t::DW_LNE_set_address:
                state.SetAddr(ExtractNumBytes(is, dataLength));
                break;
              case dwarf_t::DW_LNE_define_file:
                lineProgram->addFilename(is);
                break;
              case dwarf_t::DW_LNE_set_discriminator:
                state.SetDiscriminator(ExtractULEB128(is));
                break;
              default:
                throw Exception("Unknown extended opcode", __EINFO__);
                break;
            }
          }
          else
          {
            if (firstByte >= lineProgram->OpcodeBase())
            {
              // Special opcode (no operands)
              addLineStateMachine(state.SpecialOp(firstByte), lineProgram.get());
            }
            else
            {
              dwarf_t::lns_t op = dwarf_t::convert_lns(firstByte);
              // Standard opcode (operands according to table)

              std::vector<uint64_t> operands;
              int64_t sOperand = 0;
              uint16_t uOperand = 0;

              if (op == dwarf_t::DW_LNS_fixed_advance_pc)
              {
                uOperand = ExtractUInt16(is);
              }
              else if (op == dwarf_t::DW_LNS_advance_line)
              {
                sOperand = ExtractSLEB128(is);
              }
              else
              {
                for (int i = 0; i < lineProgram->OpcodeLengths()[firstByte-1]; ++i)
                {
                  operands.push_back(ExtractULEB128(is));
                }
              }

              switch (op)
              {
                case dwarf_t::DW_LNS_copy:
                  addLineStateMachine(state.Copy(), lineProgram.get());
                  break;
                case dwarf_t::DW_LNS_advance_pc:
                  state.AdvancePC(operands[0]);
                  break;
                case dwarf_t::DW_LNS_fixed_advance_pc:
                  state.FixedAdvancePC(uOperand);
                  break;
                case dwarf_t::DW_LNS_advance_line:
                  state.AdvanceLine(sOperand);
                  break;
                case dwarf_t::DW_LNS_set_file:
                  state.SetFile(operands[0]);
                  break;
                case dwarf_t::DW_LNS_set_column:
                  state.SetColumn(operands[0]);
                  break;
                case dwarf_t::DW_LNS_negate_stmt:
                  state.ToggleStmt();
                  break;
                case dwarf_t::DW_LNS_set_basic_block:
                  state.SetBasicBlock();
                  break;
                case dwarf_t::DW_LNS_const_add_pc:
                  state.AdvancePC(
                      (255 - lineProgram->OpcodeBase())
                          / lineProgram->LineRange());
                  break;
                case dwarf_t::DW_LNS_set_prologue_end:
                  state.SetPrologueEnd();
                  break;
                case dwarf_t::DW_LNS_set_epilogue_begin:
                  state.SetEpilogueBegin();
                  break;
                case dwarf_t::DW_LNS_set_isa:
                  state.SetISA(operands[0]);
                  break;
                default:
                  throw Exception("Unknown standard opcode", __EINFO__);
                  break;
              }
            }
          }
        }
        is.peek();
      }

      parsedLine = true;
    }

    void Info::addLineStateMachine(LineStateMachine lsm, LineProgramHeader* hdr)
    {
      locationByPc.insert({lsm.GetPC(),
        CodeLocation(lsm.GetPC(), hdr->Filename(lsm.GetFile()), lsm.GetLine(),
                     lsm.GetColumn(), lsm.GetPrologueEnd(), lsm.GetEpilogueBegin())});
    }

    void Info::parseFrame()
    {
      logger->log(Logger::DBG, "Parsing Frames");

      if (sections.find(dwarf_t::DW_SECTION_frame)    == sections.end() && \
          sections.find(dwarf_t::DW_SECTION_eh_frame) == sections.end())
      {
        // This is expected for assembly so not necessarily an error
        logger->log(Logger::WARN, "No Frame information, creating a dummy frame");

        frames = std::unique_ptr<Frames>(new Frames());
      }

      auto frameSectionIt = sections.find(dwarf_t::DW_SECTION_frame);
      auto ehFrameSectionIt = sections.find(dwarf_t::DW_SECTION_eh_frame);

      object::Parser::SectionPtr sPtr = nullptr;

      bool eh = false;
      if (ehFrameSectionIt != sections.end())
      {
        logger->log(Logger::DBG, "DW_SECTION_eh_frame");
        sPtr = ehFrameSectionIt->second;
        eh = true;
      }
      if (frameSectionIt != sections.end())
      {
        logger->log(Logger::DBG, "DW_SECTION_frame");
        sPtr = frameSectionIt->second;
        eh = false;
      }

      if (sPtr != nullptr)
      {
        auto frameStream = sPtr->getContents();
        std::istream is(&frameStream);

        frames = std::unique_ptr<Frames>(new Frames(is, sectionAddrs, eh));
        std::stringstream s;
        s << "addr = " << HexPrint(sPtr->getAddress(), 1);
        s << std::endl;
        s << "size = " << HexPrint(sPtr->getSize(), 1);
        s << std::endl;
        s << "name = " << sPtr->getName();
        s << std::endl;
        s << "No. section addrs = " << sectionAddrs.size();
        s << std::endl;
        s << "No. frame entries = " << frames->getFramesByOffset().size();
        logger->log(Logger::TRACE, s.str());
      }
    }

    bool Info::inFunctionPrologue(uint64_t pc)
    {
      auto function = functionByPC(pc);

      if (function != nullptr)
      {
        auto it = locationByPc.find(function->lowPC());
        while (it != locationByPc.end() && function->containsPC(pc))
        {
          if (it->second.prologueEnd() && !it->second.epilogueBegin())
          {
            // Stack is corrupted after return so don't display vars
            return (pc < it->first) || !function->containsPC(pc+MIN_INSTR_BYTES);
          }

          ++it;
        }
      }

      return false;
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
