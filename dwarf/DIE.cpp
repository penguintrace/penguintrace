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
// DWARF Debug Information Entries

#include "DIE.h"

#include "Arch.h"

#include <sstream>

#include "../common/MemoryBuffer.h"
#include "../common/StreamOperations.h"
#include "../common/Config.h"

#include <queue>

namespace penguinTrace
{
  namespace dwarf
  {

    DIE::~DIE()
    {
    }

    std::string DIE::toString(bool recurse, int indent)
    {
      std::stringstream s;

      s << StringPad("", indent*2) << "<" << HexPrint(offset, 2) << ">";
      if ((tag == dwarf_t::DW_TAG_NULL) && indent==0)
      {
        s << std::endl << "Root Node";
      }
      else
      {
        s << dwarf_t::tag_str(tag);
      }
      s << " (";
      s << traverse.numChildren();
      if (traverse.numChildren() == 1) s << " child)";
      else                             s << " children)";
      s << std::endl;
      s << traverse.toString(recurse, indent);
      return s.str();
    }

    bool DIE::containsPC(uint64_t pc)
    {
      assert(hasPcRange() && "DIE has no PC range");
      auto lowPcIt = traverse.getAttribute(dwarf_t::DW_AT_low_pc);
      auto highPcIt = traverse.getAttribute(dwarf_t::DW_AT_high_pc);
      uint64_t lowPc = lowPcIt.second.getInt();
      uint64_t highPc;
      if (highPcIt.second.form() == dwarf_t::DW_FORM_addr)
      {
        highPc = highPcIt.second.getInt();
      }
      else if ((highPcIt.second.form() == dwarf_t::DW_FORM_data4) ||
               (highPcIt.second.form() == dwarf_t::DW_FORM_data8))
      {
        highPc = lowPc + highPcIt.second.getInt();
      }
      else
      {
        std::stringstream err;
        err << "Unexpected PC range type: " << dwarf_t::form_str(highPcIt.second.form());

        throw Exception(err.str(), __EINFO__);
      }
      return (pc >= lowPc) && (pc < highPc);
    }

    std::pair<uint64_t, uint64_t> DIE::getOpOperand(dwarf_t::op_t op,
        std::istream& is, arch_t arch)
    {
      if (op == dwarf_t::DW_OP_addr)
      {
        return {ExtractNumBytes(is, sizeof(void*)), 0};
      }
      else if (op == dwarf_t::DW_OP_const1u)
      {
        return {ExtractUInt8(is), 0};
      }
      else if (op == dwarf_t::DW_OP_const1s)
      {
        return {(int64_t)((int8_t)ExtractUInt8(is)), 0};
      }
      else if (op == dwarf_t::DW_OP_const2u)
      {
        return {ExtractUInt16(is), 0};
      }
      else if (op == dwarf_t::DW_OP_const2s)
      {
        return {(int64_t)((int16_t)ExtractUInt16(is)), 0};
      }
      else if (op == dwarf_t::DW_OP_const4u)
      {
        return {ExtractUInt32(is), 0};
      }
      else if (op == dwarf_t::DW_OP_const4s)
      {
        return {(int64_t)((int32_t)ExtractUInt32(is)), 0};
      }
      else if (op == dwarf_t::DW_OP_const8u)
      {
        return {ExtractUInt64(is), 0};
      }
      else if (op == dwarf_t::DW_OP_const8s)
      {
        return {ExtractUInt64(is), 0};
      }
      else if (op == dwarf_t::DW_OP_constu)
      {
        return {ExtractULEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_consts)
      {
        return {ExtractSLEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_pick)
      {
        return {ExtractUInt8(is), 0};
      }
      else if (op == dwarf_t::DW_OP_plus_uconst)
      {
        return {ExtractULEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_skip)
      {
        return {(int64_t)((int16_t)ExtractUInt16(is)), 0};
      }
      else if (op == dwarf_t::DW_OP_bra)
      {
        return {(int64_t)((int16_t)ExtractUInt16(is)), 0};
      }
      else if (op >= dwarf_t::DW_OP_breg0 && op <= dwarf_t::DW_OP_breg31)
      {
        return {ExtractSLEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_regx)
      {
        return {ExtractULEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_fbreg)
      {
        return {ExtractSLEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_bregx)
      {
        return {ExtractULEB128(is), ExtractSLEB128(is)};
      }
      else if (op == dwarf_t::DW_OP_piece)
      {
        return {ExtractULEB128(is), 0};
      }
      else if (op == dwarf_t::DW_OP_deref_size)
      {
        return {ExtractUInt8(is), 0};
      }
      else if (op == dwarf_t::DW_OP_xderef_size)
      {
        return {ExtractUInt8(is), 0};
      }
      else if (op == dwarf_t::DW_OP_call2)
      {
        return {ExtractUInt16(is), 0};
      }
      else if (op == dwarf_t::DW_OP_call4)
      {
        return {ExtractUInt32(is), 0};
      }
      else if (op == dwarf_t::DW_OP_call_ref)
      {
        return {ExtractSectionOffset(is, arch), 0};
      }
      else if (op == dwarf_t::DW_OP_bit_piece)
      {
        return {ExtractULEB128(is), ExtractULEB128(is)};
      }
      else if (op == dwarf_t::DW_OP_implicit_value)
      {
        // TODO implicit value
        throw Exception("DW_OP_implicit_value not supported", __EINFO__);
      }
      return {0, 0};
    }

    uint64_t DIE::getFrameBase(std::function<uint64_t(std::string)> regCallback,
        std::function<uint64_t(uint64_t)> memCallback, uint64_t pc)
    {
      DIE* current = this;
      while (current->parent != nullptr)
      {
        auto it = current->traverse.getAttribute(dwarf_t::DW_AT_frame_base);
        if (it.first)
        {
          return current->getValue(regCallback, memCallback, pc, dwarf_t::DW_AT_frame_base).first;
        }

        current = current->parent;
      }
      throw Exception("frame_base not found", __EINFO__);
    }

    uint64_t DIE::maskBytes(uint64_t value, int numBytes)
    {
      if (numBytes > 8) return value;
      std::stringstream s;
      switch (numBytes)
      {
        case 0: return 0;
        case 1: return value & 0xff;
        case 2: return value & 0xffff;
        case 4: return value & 0xffffffff;
        case 8: return value;
        default:
          s << "Unsupported number of bytes: " << numBytes;
          throw Exception(s.str(), __EINFO__);
      }
    }

    std::pair<uint64_t, uint64_t> DIE::getValue(std::function<uint64_t(std::string)> regCallback,
        std::function<uint64_t(uint64_t)> memCallback,
        uint64_t pc, dwarf_t::at_t at)
    {
      auto locIt = traverse.getAttribute(at);
      if (!locIt.first)
      {
        std::cout << toString(true) << std::endl;
      }
      assert(locIt.first);

      // Can be exprloc or sec_offset
      if (locIt.second.form() == dwarf_t::DW_FORM_sec_offset)
      {
      // TODO support sec_offset (into loclist in .debug_loc)
        throw Exception("Unsupported location list", __EINFO__);
      }
      MemoryBuffer buffer(locIt.second.getBuffer().data(),
          locIt.second.getBuffer().size());
      std::istream is(&buffer);
      is.peek();

      // Pairs of value, address
      std::queue<std::pair<uint64_t, uint64_t> > stack;

      while (!is.eof())
      {
        dwarf_t::op_t op = dwarf_t::convert_op(ExtractUInt8(is));
        std::pair<uint64_t, uint64_t> operand = getOpOperand(op, is, arch);

        if (op == dwarf_t::DW_OP_nop)
        {

        }
        else if (op == dwarf_t::DW_OP_stack_value)
        {
          break;
        }
        else if (op == dwarf_t::DW_OP_plus_uconst)
        {
          uint64_t val = stack.front().first;
          stack.pop();
          stack.push({operand.first + val, 0});
        }
        else if (op == dwarf_t::DW_OP_const1u)
        {
          stack.push({operand.first, 0});
        }
        else if (op >= dwarf_t::DW_OP_reg0 && op <= dwarf_t::DW_OP_reg31)
        {
          // Reg statement must be only statement
          uint64_t regNum = op - dwarf_t::DW_OP_reg0;

          stack.push({regCallback(dwarfRegString(regNum)), 0});
        }
        else if (op >= dwarf_t::DW_OP_breg0 && op <= dwarf_t::DW_OP_breg31)
        {
          uint64_t regNum = op - dwarf_t::DW_OP_breg0;
          uint64_t ptr = regCallback(dwarfRegString(regNum))+(int64_t)operand.first;

          stack.push({memCallback(ptr), ptr});
        }
        else if (op == dwarf_t::DW_OP_fbreg)
        {
          uint64_t fbRegVal = getFrameBase(regCallback, memCallback, pc);
          uint64_t ptr = fbRegVal + (int64_t)operand.first;

          uint64_t rawValue = memCallback(ptr);
          auto type = getType();
          auto it = type->traverse.getAttribute(dwarf_t::DW_AT_byte_size);
          int numBytes = sizeof(void*);

          if (it.first)
          {
            numBytes = it.second.getInt();
          }

          stack.push({maskBytes(rawValue, numBytes), ptr});
        }
        else if (op == dwarf_t::DW_OP_addr)
        {
          stack.push({operand.first, operand.first});
        }
        else if (op == dwarf_t::DW_OP_deref)
        {
          uint64_t addr = stack.front().first;
          stack.pop();
          stack.push({memCallback(addr), addr});
        }
        else if (op == dwarf_t::DW_OP_call_frame_cfa)
        {
          // CFA is " " so it is first in map (and always SP)
          auto uMap = frames->getFDEByPc(pc)->unwindInfo(pc);
          auto f = uMap->find(" ");
          uint64_t newValue = regCallback(SP_REG_NAME);

          if (f->second.getType() == dwarf::RegRule::SAME)
          {

          }
          else if (f->second.getType() == dwarf::RegRule::EXPR)
          {
            throw Exception("Expression unwind unsupported", __EINFO__);
          }
          else if (f->second.getType() == dwarf::RegRule::OFFSET)
          {
            throw Exception("Expression offset unsupported for stack pointer", __EINFO__);
          }
          else if (f->second.getType() == dwarf::RegRule::VAL_OFFSET)
          {
            std::string regFrom = f->second.regName();
            uint64_t baseValue = regCallback(regFrom);
            if (f->second.op().isSigned())
            {
              int64_t offset = f->second.op().getSInt();
              newValue = baseValue+offset;
            }
            else
            {
              uint64_t offset = f->second.op().getInt();
              newValue = baseValue+offset;
            }
          }
          else
          {
            throw Exception("Unhandled unwind rule", __EINFO__);
          }

          stack.push({newValue, 0});
        }
        else
        {
          std::stringstream err;
          err << "Unhandled: " << dwarf_t::op_str(op) << std::endl;
          err << "On DIE: <" << HexPrint(offset, 2) << ">";
          if (hasName())
          {
            err << getName();
          }
          throw Exception(err.str(), __EINFO__);
        }
        is.peek();
      }

      if (stack.size() > 0)
      {
        return stack.front();
      }
      else
      {
        throw Exception("Empty stack on DIE parse", __EINFO__);
      }
    }

    std::pair<bool, int> DIE::arrayLength()
    {
      bool ok = false;
      // This only works for C-style arrays
      // Default lower bound is 1 for Ada, COBOL, Fortran, Modula-2, Pascal and PL/I
      int lower = 0;
      int upper = -1;
      if (tag != dwarf_t::DW_TAG_array_type)
      {
        throw Exception("Not an array", __EINFO__);
      }
      for (auto it = traverse.getChildIterator(false); !it.done(); it.next())
      {
        DIE* d = it.value();
        if (d->tag == dwarf_t::DW_TAG_subrange_type)
        {
          auto lowerA = d->traverse.getAttribute(dwarf_t::DW_AT_lower_bound);
          if (lowerA.first)
          {
            lower = lowerA.second.getInt();
          }
          auto upperA = d->traverse.getAttribute(dwarf_t::DW_AT_upper_bound);
          if (upperA.first)
          {
            upper = upperA.second.getInt();
            ok = true;
          }
        }
      }
      return {ok, upper-lower+1};
    }

    std::string DIE::arrayStr()
    {
      if (tag != dwarf_t::DW_TAG_array_type)
      {
        throw Exception("Not an array", __EINFO__);
      }

      auto len = arrayLength();
      if (len.first)
      {
        std::stringstream s;
        s << '[' << len.second << ']';
        return s.str();
      }
      else
      {
        return "[]";
      }
    }

    std::string DIE::typeString()
    {
      // Cache to save expense of travesing DIEs?
      if (typeStrCache.size() > 0) return typeStrCache;
      auto ns = getNamespace();
      switch (tag)
      {
        case dwarf_t::DW_TAG_const_type:
          typeStrCache = "const " + ns + getType()->typeString();
          break;
        case dwarf_t::DW_TAG_structure_type:
          typeStrCache = hasName() ? "struct " + ns + getName() : "{anon struct}";
          break;
        case dwarf_t::DW_TAG_union_type:
          typeStrCache = hasName() ? "union " + ns + getName() : "{anon union}";
          break;
        case dwarf_t::DW_TAG_enumeration_type:
          typeStrCache = hasName() ? "enum " + ns + getName() : "{anon enum}";
          break;
        case dwarf_t::DW_TAG_array_type:
          typeStrCache = getType()->typeString() + arrayStr();
          break;
        case dwarf_t::DW_TAG_pointer_type:
          typeStrCache = hasType() ? (getType()->typeString() + "*") : "void*";
          break;
        case dwarf_t::DW_TAG_reference_type:
          typeStrCache = getType()->typeString() + "&";
          break;
        case dwarf_t::DW_TAG_rvalue_reference_type:
          typeStrCache = getType()->typeString() + "&&";
          break;
        case dwarf_t::DW_TAG_base_type:
          assert(hasName());
          typeStrCache = ns + getName();
          break;
        case dwarf_t::DW_TAG_class_type:
          assert(hasName());
          typeStrCache = "class "+ ns + getName();
          break;
        case dwarf_t::DW_TAG_formal_parameter:
          assert(hasType());
          typeStrCache = getType()->typeString();
          break;
        case dwarf_t::DW_TAG_typedef:
          assert(hasName());
          typeStrCache = ns + getName();
          break;
        case dwarf_t::DW_TAG_subroutine_type:
          typeStrCache = ns;
          break;
        default:
          throw Exception(dwarf_t::tag_str(tag)+"\n"+toString(true), __EINFO__);
      }
      return typeStrCache;
    }

    std::string DIE::getNamespace()
    {
      std::string ns;
      DIE* current = this;

      while (current != nullptr)
      {
        if ((current->tag == dwarf_t::DW_TAG_namespace) ||
            (current->tag == dwarf_t::DW_TAG_class_type) ||
            (current->tag == dwarf_t::DW_TAG_structure_type) ||
            (current->tag == dwarf_t::DW_TAG_enumeration_type))
        {
          std::string name;
          if (current->hasName()) name = current->getName();
          else                    name = "<anon>";

          // Can't tell inline namespaces in DWARF so skip manually
          if (name == "__cxx11")
          {
            current = current->parent;
            continue;
          }
          ns = name+"::"+ns;
        }

        current = current->parent;
      }

      // Manually traversing tree so deal with specification elsewhere
      if (ns.length() == 0 && hasAttr(dwarf_t::DW_AT_specification))
      {
        ns = dies->find(traverse.getAttribute(dwarf_t::DW_AT_specification).second.getInt())->second->getNamespace();
      }

      return ns;
    }

    bool DIE::isPointerType()
    {
      return (tag == dwarf_t::DW_TAG_pointer_type) ||
             (tag == dwarf_t::DW_TAG_reference_type) ||
             (tag == dwarf_t::DW_TAG_rvalue_reference_type);
    }

    DIE* DIE::getFirstChild()
    {
      auto it = traverse.getChildIterator(false);
      if (!it.done())
      {
        return it.value();
      }
      return nullptr;
    }

    void DIE::addAttribute(dwarf_t::at_t at, AttrValue value)
    {
      traverse.addAttribute(at, value);
    }

    void DIE::addChild(std::unique_ptr<DIE> die)
    {
      traverse.addChild(std::move(die));
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
