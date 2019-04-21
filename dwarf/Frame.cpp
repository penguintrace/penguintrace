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

#include "Frame.h"

#include "Arch.h"

#include <stdio.h>
#include <inttypes.h>

namespace penguinTrace
{
  namespace dwarf
  {

    std::string RegRule::toString(bool cfa)
    {
      std::stringstream s;
      std::string regName = cfa ? reg : "CFA";
      switch (type)
      {
        case UNDEFINED: return "u";
        case SAME:      return "s";
        case OFFSET:
          s << "[" << regName << operand.toString(true) << "]";
          return s.str();
        case VAL_OFFSET:
          s << regName << operand.toString(true);
          return s.str();
        case REGISTER:  return reg;
        case EXPR:      return "[e]";
        case VAL_EXPR:  return "e";
        default:        return "?";
      }
    }

    CFA CFA::parseCFA(std::istream& ifs, uint64_t offset)
    {
      dwarf_t::cfa_t cfa = dwarf_t::convert_cfa(ExtractUInt8(ifs));
      AttrValue operand1;
      AttrValue operand2;

      if ((cfa >= dwarf_t::DW_CFA_advance_loc_delta0
          && cfa <= dwarf_t::DW_CFA_advance_loc_delta63) ||
          (cfa >= dwarf_t::DW_CFA_restore_reg0
          && cfa <= dwarf_t::DW_CFA_restore_reg63) ||
          cfa == dwarf_t::DW_CFA_nop ||
          cfa == dwarf_t::DW_CFA_remember_state ||
          cfa == dwarf_t::DW_CFA_restore_state)
      {

      }
      else if ((cfa >= dwarf_t::DW_CFA_offset_reg0
          && cfa <= dwarf_t::DW_CFA_offset_reg63) ||
          cfa == dwarf_t::DW_CFA_restore_extended ||
          cfa == dwarf_t::DW_CFA_undefined ||
          cfa == dwarf_t::DW_CFA_same_value ||
          cfa == dwarf_t::DW_CFA_def_cfa_register ||
          cfa == dwarf_t::DW_CFA_def_cfa_offset ||
          cfa == dwarf_t::DW_CFA_GNU_args_size)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractULEB128(ifs), 8, false);
      }
      else if (cfa == dwarf_t::DW_CFA_offset_extended_sf ||
               cfa == dwarf_t::DW_CFA_def_cfa_sf ||
               cfa == dwarf_t::DW_CFA_val_offset_sf)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractULEB128(ifs), 8, false);
        operand2 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractSLEB128(ifs), 8, true);
      }
      else if (cfa == dwarf_t::DW_CFA_def_cfa_offset_sf)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractSLEB128(ifs), 8, true);
      }
      else if (cfa == dwarf_t::DW_CFA_def_cfa_expression)
      {
        std::vector<uint8_t> tmpBuf;
        ExtractExprLoc(ifs, tmpBuf);
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, tmpBuf);
      }
      else if (cfa == dwarf_t::DW_CFA_expression ||
               cfa == dwarf_t::DW_CFA_val_expression)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractULEB128(ifs), 8, false);
        std::vector<uint8_t> tmpBuf;
        ExtractExprLoc(ifs, tmpBuf);
        operand2 = AttrValue(dwarf_t::DW_FORM_NULL, tmpBuf);
      }
      else if (cfa == dwarf_t::DW_CFA_set_loc)
      {
        std::cout << "set_loc" << std::endl;
        std::cout << HexPrint(offset, 1) << std::endl;
        assert(0 && "Address size?");
      }
      else if (cfa == dwarf_t::DW_CFA_advance_loc1)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractUInt8(ifs), 1, false);
      }
      else if (cfa == dwarf_t::DW_CFA_advance_loc2)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractUInt16(ifs), 2, false);
      }
      else if (cfa == dwarf_t::DW_CFA_advance_loc4)
      {
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractUInt32(ifs), 4, false);
      }
      else if (cfa == dwarf_t::DW_CFA_offset_extended ||
               cfa == dwarf_t::DW_CFA_def_cfa ||
               cfa == dwarf_t::DW_CFA_register ||
               cfa == dwarf_t::DW_CFA_val_offset)
      {
        // Number of bytes is not entirely relevant
        operand1 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractULEB128(ifs), 8, false);
        operand2 = AttrValue(dwarf_t::DW_FORM_NULL, ExtractULEB128(ifs), 8, false);
      }
      else
      {
        std::stringstream err;
        err << "Unknown CFA: ";
        err << dwarf_t::cfa_str(cfa) << std::endl;
        err << HexPrint(offset, 1);
        throw Exception(err.str(), __EINFO__);
      }

      return CFA(cfa, operand1, operand2);
    }

    FrameEntry::FrameEntry(uint64_t offset, std::list<CFA> instrs) :
        offset(offset), instructions(instrs)
    {

    }

    FrameEntry::~FrameEntry()
    {

    }

    CIE::CIE(uint64_t offset, uint64_t version, std::string aug,
        uint64_t codeAlign, int64_t dataAlign, uint64_t retAddrCol,
        uint8_t ptrRepr, std::list<CFA> instrs, uint64_t addrSize,
        uint64_t segmentSize, bool signal) :
        FrameEntry(offset, instrs), version(version), augmentation(aug), codeAlign(
            codeAlign), dataAlign(dataAlign), returnAddrColumn(retAddrCol),
            addrSize(addrSize), segmentSize(segmentSize), signal(signal)
    {
      ptrFormat = dwarf_t::convert_eh_pe(ptrRepr & 0x0f);
      ptrApplication = dwarf_t::convert_eh_pe(ptrRepr & 0xf0);
    }

    std::string CIE::headerRepr()
    {
      std::stringstream s;
      s << "CIE(";
      s << HexPrint(offset, 3);
      s << ") v" << version;
      s << " " << '"' << augmentation << '"';
      return s.str();
    }

    std::string CIE::repr()
    {
      std::stringstream s;
      s << "CIE(";
      s << HexPrint(offset, 3);
      s << ") v" << version;
      s << " " << '"' << augmentation << '"';
      s << std::endl;
      s << "  codeAlign=" << codeAlign;
      s << std::endl;
      s << "  dataAlign=" << dataAlign;
      s << std::endl;
      s << "  retAddr  =" << returnAddrColumn;
      s << std::endl;
      s << "  ptrFormat=" << dwarf_t::eh_pe_str(ptrFormat);
      s << std::endl;
      s << "  ptrApplic=" << dwarf_t::eh_pe_str(ptrApplication);
      for (auto it = instructions.begin(); it != instructions.end(); ++it)
      {
        s << std::endl;
        s << "    " << dwarf_t::cfa_str(it->type());
        // op1
        if (it->op1().type() != DWARF_NULL)
        {
          s << " '" << it->op1().toString() << "'";
        }
        // op2
        if (it->op2().type() != DWARF_NULL)
        {
          s << " '" << it->op2().toString() << "'";
        }
      }

      return s.str();
    }


    FDE::FDE(uint64_t offset, uint64_t ciePtr, CIE* cie, uint64_t start,
        uint64_t end, std::list<CFA> instrs) :
        FrameEntry(offset, instrs), ciePtr(ciePtr), cie(cie), startAddr(start), endAddr(
            end)
    {

    }

    std::string FDE::headerRepr()
    {
      std::stringstream s;
      s << "FDE(";
      s << HexPrint(offset, 3);
      s << ") cie=" << HexPrint(ciePtr, 3);
      return s.str();
    }

    std::string FDE::repr()
    {
      std::stringstream s;
      s << "FDE(";
      s << HexPrint(offset, 3);
      s << ") cie=" << HexPrint(ciePtr, 3);
      s << " " << HexPrint(startAddr, 16);
      s << ".." << HexPrint(endAddr, 16);
      for (auto it = instructions.begin(); it != instructions.end(); ++it)
      {
        s << std::endl;
        s << "    " << dwarf_t::cfa_str(it->type());
        // op1
        if (it->op1().type() != DWARF_NULL)
        {
          s << " '" << it->op1().toString() << "'";
        }
        // op2
        if (it->op2().type() != DWARF_NULL)
        {
          s << " '" << it->op2().toString() << "'";
        }
      }
      s << std::endl;
      s << cie->headerRepr();
      return s.str();
    }

    Frames::Frames()
    {
    }

    Frames::Frames(std::istream& ifs,
        std::map<std::string, uint64_t>& sectionAddrs, bool eh)
    {
      ifs.peek();
      bool done = ifs.eof();

      while (!done)
      {
        int64_t offset = ifs.tellg();
        auto initLen = ExtractInitialLength(ifs);

        if (initLen.second != 0)
        {
          int64_t offset2 = ifs.tellg();
          uint64_t ident = ExtractSectionOffset(ifs, initLen.first);
          // .eh_frame uses ident = 0
          // .dwarf_frame uses all bits set
          bool isCie = eh ? (ident == 0) : ((ident == 0xffffffff) || (ident == ((uint64_t)-1)));
          if (isCie)
          {
            int64_t last = offset2 + initLen.second;

            uint64_t version = ExtractUInt8(ifs);
            std::string aug = ExtractString(ifs);

            assert(((version == 1) || (version == 4)) && "Only support DWARFv1/4 currently");
            uint64_t addrSize = 0;
            uint64_t segmentSize = 0;
            bool signal = false;

            if (version == 4)
            {
              addrSize = ExtractUInt8(ifs);
              segmentSize = ExtractUInt8(ifs);
            }

            uint64_t codeAlign = ExtractULEB128(ifs);
            int64_t dataAlign = ExtractSLEB128(ifs);
            // Version 2 is unused for frame information
            uint64_t returnAddrCol;
            if (version == 1)
            {
              returnAddrCol = ExtractUInt8(ifs);
            }
            else if ((version == 3) || (version == 4))
            {
              returnAddrCol = ExtractULEB128(ifs);
            }
            else
            {
              std::stringstream s;
              s << "Unsupported DWARF version '" << version;
              s << "'. Don't know return address column size.";
              throw Exception(s.str(), __EINFO__);
            }

            uint8_t ptrRepr = 0xff;
            if (aug == "zR")
            {
              uint64_t augLen = ExtractULEB128(ifs);
              assert(augLen == 1);
              ptrRepr = ExtractUInt8(ifs);
            }
            else if (aug == "zRS")
            {
              uint64_t augLen = ExtractULEB128(ifs);
              assert(augLen == 1);
              ptrRepr = ExtractUInt8(ifs);
              signal = true;
              aug = "zR";
            }
            else if (aug == "")
            {

            }
            else if (aug[0] == 'z' && aug[aug.length()-1] == 'R')
            {
              uint64_t augLen = ExtractULEB128(ifs);
              std::vector<uint8_t> tmpBuf;
              ExtractBuffer(ifs, tmpBuf, augLen-1);
              ptrRepr = ExtractUInt8(ifs);
            }
            else
            {
              throw Exception("Unknown augmentation: " + aug, __EINFO__);
            }

            std::list<CFA> cfas;
            auto lastPos = ifs.tellg();
            while (ifs.tellg() != last)
            {
              if (ifs.eof() || ifs.tellg() > last)
              {
                throw Exception("Reached EOF in CIE", __EINFO__);
              }
              lastPos = ifs.tellg();
              cfas.push_back(CFA::parseCFA(ifs, offset));
            }

            CIE* cie = new CIE(offset, version, aug, codeAlign, dataAlign,
                returnAddrCol, ptrRepr, cfas, addrSize, segmentSize, signal);

            std::unique_ptr<FrameEntry> ptr(cie);
            frames.insert(std::make_pair(offset, std::move(ptr)));
          }
          else
          {
            int64_t last = offset2 + initLen.second;
            uint64_t ciePtr = eh ? offset2-ident : ident;
            auto it = frames.find(ciePtr);
            assert(it != frames.end());
            CIE* cie = dynamic_cast<CIE*>(it->second.get());

            uint64_t startAddr = 0;
            uint64_t endAddr = 0;

            if (cie->application() == dwarf_t::DW_EH_PE_pcrel)
            {
              uint64_t base = sectionAddrs[dwarf_t::convert_section(
                  dwarf_t::DW_SECTION_eh_frame)];
              uint64_t curOffset = ifs.tellg();

              if (cie->format() == dwarf_t::DW_EH_PE_sdata4)
              {
                int64_t offset = (int32_t)ExtractUInt32(ifs);
                int64_t size   = (int32_t)ExtractUInt32(ifs);
                startAddr = (int64_t) base + (int64_t) curOffset
                    + (int64_t)offset;
                endAddr   = (int64_t)startAddr + (int64_t)size;
              }
              else
              {
                throw Exception("Unknown pointer format", __EINFO__);
              }
              // PC relative is relative to the start of the
              //  .eh_frame section + this offset
              // e.g. ifs.tellg()+[.eh_frame]+sdata4
              // So need to get address of .eh_frame section
              //  (or other sections if .text relative etc.)
            }
            else
            {
              if (cie->getVersion() == 4 && cie->getAddrSize() != 0)
              {
                uint64_t start = ExtractNumBytes(ifs, cie->getAddrSize());
                uint64_t end   = ExtractNumBytes(ifs, cie->getAddrSize());
                startAddr = start;
                endAddr   = start+end;
              }
              else
              {
                throw Exception("Unknown pointer application", __EINFO__);
              }
            }

            //uint64_t initLoc = ExtractNumBytes(ifs, ADDR_BYTES);
            //uint64_t lastLoc = ExtractNumBytes(ifs, ADDR_BYTES);

            if (cie->getAugmentation() == "zR")
            {
              uint64_t augLen = ExtractULEB128(ifs);
              assert(augLen == 0);
            }
            else if (cie->getAugmentation() == "")
            {
            }
            else if (cie->getAugmentation() == "zPLR")
            {
              uint64_t augLen = ExtractULEB128(ifs);
              std::vector<uint8_t> tmpBuf;
              ExtractBuffer(ifs, tmpBuf, augLen);
            }
            else
            {
              std::stringstream err;
              err << "Unknown augmentation: " << cie->getAugmentation();
              throw Exception(err.str(), __EINFO__);
            }

            std::list<CFA> cfas;
            auto lastPos = ifs.tellg();
            while (ifs.tellg() != last)
            {
              if (ifs.eof() || ifs.tellg() > last)
              {
                throw Exception("Reached EOF in FDE", __EINFO__);
              }
              lastPos = ifs.tellg();
              cfas.push_back(CFA::parseCFA(ifs, offset));
            }

            FDE* fde = new FDE(offset, ciePtr, cie, startAddr, endAddr, cfas);

            std::unique_ptr<FrameEntry> ptr(fde);
            frames.insert(std::make_pair(offset, std::move(ptr)));
          }
        }
        ifs.peek();
        done = ifs.eof();
      }

      for (auto it = frames.begin(); it != frames.end(); ++it)
      {
        FDE* fde = dynamic_cast<FDE*>(it->second.get());
        if (fde != nullptr)
        {
          fdeByPc[fde->getStartAddr()] = fde;
        }
      }
    }

    FDE* Frames::getFDEByPc(uint64_t pc)
    {
      for (auto it = fdeByPc.begin(); it != fdeByPc.end(); ++it)
      {
        if (it->second->contains(pc)) return it->second;
      }
      return nullptr;
    }

    std::unique_ptr<std::map<std::string, RegRule> > FDE::unwindInfo(uint64_t pc)
    {
      auto info = std::unique_ptr<std::map<std::string, RegRule> >(
          new std::map<std::string, RegRule>());

      uint64_t loc = startAddr;
      std::list<CFA> instrs(cie->getInstructions());
      instrs.insert(instrs.end(), instructions.begin(), instructions.end());

      // Initial instructions
      // CFA defined as " " to ensure first in map
      for (auto it : instrs)
      {
        dwarf_t::cfa_t cfa = it.type();
        if (cfa >= dwarf_t::DW_CFA_advance_loc_delta0 &&
            cfa <= dwarf_t::DW_CFA_advance_loc_delta63)
        {
          uint64_t delta = (cfa - dwarf_t::DW_CFA_advance_loc_delta0)
              * cie->getCodeAlign();
          uint64_t newLoc = loc + delta;

          if (pc >= newLoc) loc = newLoc;
          else              break;
        }
        else if ((cfa == dwarf_t::DW_CFA_advance_loc1) ||
                 (cfa == dwarf_t::DW_CFA_advance_loc2) ||
                 (cfa == dwarf_t::DW_CFA_advance_loc4))
        {
          uint64_t delta = it.op1().getInt() * cie->getCodeAlign();
          uint64_t newLoc = loc + delta;

          if (pc >= newLoc) loc = newLoc;
          else              break;
        }
        else if (cfa >= dwarf_t::DW_CFA_restore_reg0 &&
                 cfa <= dwarf_t::DW_CFA_restore_reg63)
        {
          //uint64_t reg = cfa - dwarf_t::DW_CFA_restore_reg0;
          // TODO restore reg
          throw Exception("restore_reg unhandled", __EINFO__);
        }
        else if (cfa >= dwarf_t::DW_CFA_offset_reg0 &&
                 cfa <= dwarf_t::DW_CFA_offset_reg63)
        {
          uint64_t reg = cfa - dwarf_t::DW_CFA_offset_reg0;
          std::string regName = (reg == cie->raCol()) ? "ra" : dwarfRegString(reg);
          int64_t offset = ((int64_t)it.op1().getInt())*cie->getDataAlign();
          AttrValue value = AttrValue(dwarf_t::DW_FORM_NULL, offset, 8, true);
          if (info->find(regName) != info->end()) info->erase(regName);
          info->insert(
            { regName, RegRule(RegRule::OFFSET, regName, value) });
        }
        else if (cfa == dwarf_t::DW_CFA_def_cfa)
        {
          std::string regName = dwarfRegString(it.op1().getInt());
          uint64_t offset = it.op2().getInt();
          if (info->find(" ") != info->end()) info->erase(" ");
          info->insert(
                { " ", RegRule(RegRule::VAL_OFFSET, regName,
                    AttrValue(dwarf_t::DW_FORM_NULL, offset, 1, true)) });
        }
        else if (cfa == dwarf_t::DW_CFA_def_cfa_offset)
        {
          assert(info->find(" ") != info->end());
          auto currentRuleIt = info->find(" ");
          auto currentRule = currentRuleIt->second.regName();
          uint64_t offset = it.op1().getInt();
          if (info->find(" ") != info->end()) info->erase(" ");
          info->insert(
                { " ", RegRule(RegRule::VAL_OFFSET, currentRule,
                    AttrValue(dwarf_t::DW_FORM_NULL, offset, 1, true)) });
        }
        else if (cfa == dwarf_t::DW_CFA_def_cfa_register)
        {
          auto currentRuleIt = info->find(" ");
          auto currentRule = currentRuleIt->second.op();
          std::string regName = dwarfRegString(it.op1().getInt());
          if (info->find(" ") != info->end()) info->erase(" ");
          info->insert(
                { " ", RegRule(RegRule::VAL_OFFSET, regName,
                    currentRule) });
        }
        else if (cfa == dwarf_t::DW_CFA_nop)
        {

        }
        else
        {
          std::stringstream err;
          err << dwarf_t::cfa_str(cfa) << " unhandled";
          throw Exception(err.str(), __EINFO__);
        }
      }

      if (loc != pc)
      {
        loc = endAddr;
      }

      assert(loc >= pc);

      return info;
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
