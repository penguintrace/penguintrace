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
// DWARF Debug Information Entries - Extracting values from tracee

#include "DIE.h"

#include <sstream>

#include "../common/MemoryBuffer.h"
#include "../common/StreamOperations.h"
#include "../common/Config.h"

#include <queue>

namespace penguinTrace
{
  namespace dwarf
  {

    std::string DIE::getValueString(std::function<uint64_t(std::string)> regCallback,
        std::function<uint64_t(uint64_t)> memCallback, uint64_t pc)
    {
      std::stringstream s;
      if (!hasType() || !hasAttr(dwarf_t::DW_AT_location))
      {
        return "";
      }
      auto v = getValue(regCallback, memCallback, pc, dwarf_t::DW_AT_location);
      uint64_t value = v.first;
      uint64_t addr  = v.second;
      auto type = getType();

      // argc/argv special cased, as otherwise wouldn't know length of
      //  a pointer-to-pointer
      if (type->tag == dwarf_t::DW_TAG_pointer_type && parent->hasName())
      {
        //  if string not null, try to read up to N bytes and display if all chars printable
        bool isArgv = parent->getName() == "main";
        int argc = 0;

        if (isArgv)
        {
          isArgv = false;
          std::vector<DIE*> parentParams;
          assert(parent != nullptr);
          for (auto it = parent->traverse.getChildIterator(false); !it.done(); it.next())
          {
            DIE* d = it.value();
            if (d->tag == dwarf_t::DW_TAG_formal_parameter)
            {
              parentParams.push_back(d);
            }
          }

          if (parentParams.size() >= 2)
          {
            if (parentParams[0]->getType()->hasName()
                && parentParams[0]->getType()->getName() == "int")
            {
              argc = parentParams[0]->getValue(regCallback, memCallback, pc, dwarf_t::DW_AT_location).first;
              argc = argc > Config::get(C_ARGC_MAX).Int() ? Config::get(C_ARGC_MAX).Int() : argc;

              if (parentParams[1] == this)
              {
                if (getType()->typeString() == "char**")
                {
                  isArgv = true;
                }
              }
            }
          }
        }

        if (isArgv)
        {
          // If argv - want to try and get strings
          for (int i = 0; i < argc; ++i)
          {
            if (i != 0) s << ", ";

            uint64_t ptrToStr = memCallback(value+(i*sizeof(void*)));
            s << tryGetString(ptrToStr, memCallback);

          }
        }
        else
        {
          // Other pointer - should recurse
          // Only follow pointers at this point, not multiple levels
          std::list<dwarf_t::tag_t> tags;
          s << type->getValueStringByAddr(regCallback, memCallback, addr, tags, 0, nullptr);
        }
      }
      else if (type->hasName() && (type->typeString() == "int"))
      {
        s << (int)value;
      }
      else if (type->hasName() && (type->typeString() == "bool"))
      {
        s << ((value == 0) ? "false" : "true");
      }
      else if (type->tag == dwarf_t::DW_TAG_const_type)
      {
        // Traverse down if const
        s << type->getValueString(regCallback, memCallback, pc);
      }
      else
      {
        if (addr != 0)
        {
          std::list<dwarf_t::tag_t> tags;
          s << type->getValueStringByAddr(regCallback, memCallback, addr, tags, 0, nullptr);
        }
        else
        {
          // TODO what to do if not address?
          s << HexPrint(value, sizeof(void*)*2);
        }
      }

      return s.str();
    }

    uint64_t DIE::getNumBytes(bool followPtr)
    {
      if (isPointerType() && followPtr)
      {
        return getType()->getNumBytes(followPtr);
      }
      else
      {
        auto it = traverse.getAttribute(dwarf_t::DW_AT_byte_size);
        if (it.first)
        {
          return it.second.getInt();
        }
        else
        {
          throw Exception("Unknown byte size:\n"+toString(true), __EINFO__);
        }
      }
    }

    uint64_t DIE::getValueByAddr(RegCallback regCallback, MemCallback memCallback, uint64_t addr, bool followPtr)
    {
      if ((tag == dwarf_t::DW_TAG_array_type) ||
          (tag == dwarf_t::DW_TAG_class_type) ||
          (tag == dwarf_t::DW_TAG_structure_type))
      {
        throw Exception("Can't get int value of array/class/struct", __EINFO__);
      }
      else if ((tag == dwarf_t::DW_TAG_typedef) ||
               (tag == dwarf_t::DW_TAG_const_type))
      {
        return getType()->getValueByAddr(regCallback, memCallback, addr, followPtr);
      }
      else if (isPointerType())
      {
        if (followPtr)
        {
          return getType()->getValueByAddr(regCallback, memCallback, addr, true);
        }
        else
        {
          return memCallback(addr);
        }
      }
      else if (tag == dwarf_t::DW_TAG_base_type)
      {
        int numBytes = sizeof(void*);
        dwarf_t::ate_t enc = dwarf_t::DW_ATE_NULL;
        auto it = traverse.getAttribute(dwarf_t::DW_AT_byte_size);
        auto encIt = traverse.getAttribute(dwarf_t::DW_AT_encoding);

        if (it.first)
        {
          numBytes = it.second.getInt();
        }
        if (encIt.first)
        {
          enc = dwarf_t::convert_ate(encIt.second.getInt());
        }

        uint64_t value = maskBytes(memCallback(addr), numBytes);

        if (enc == dwarf_t::DW_ATE_boolean)
        {
          return value;
        }
        else if ((enc == dwarf_t::DW_ATE_signed) ||
                 (enc == dwarf_t::DW_ATE_signed_char))
        {
          if      (numBytes == 1) return (int64_t)(int8_t)value;
          else if (numBytes == 2) return (int64_t)(int16_t)value;
          else if (numBytes == 4) return (int64_t)(int32_t)value;
          else if (numBytes == 8) return (int64_t)value;
          else                    return (int64_t)value;
        }
        else if (enc == dwarf_t::DW_ATE_unsigned)
        {
          if      (numBytes == 1) return (uint32_t)((uint8_t)value);
          else if (numBytes == 2) return (uint32_t)((uint16_t)value);
          else if (numBytes == 4) return (uint32_t)value;
          else if (numBytes == 8) return (uint64_t)value;
          else                    return (uint64_t)value;
        }
        else if (enc == dwarf_t::DW_ATE_unsigned_char)
        {
          return value;
        }
        else
        {
          throw Exception("Unknown encoding to get int value: "+dwarf_t::ate_str(enc), __EINFO__);
        }
      }
      else
      {
        throw Exception("Unknown type\n"+toString(true), __EINFO__);
      }
    }

    std::string DIE::getValueStringByAddr(std::function<uint64_t(std::string)> regCallback,
        std::function<uint64_t(uint64_t)> memCallback, uint64_t addr,
        std::list<dwarf_t::tag_t> tags, int depth, ClsMemMap* memberMap)
    {
      bool likelyString = false;
      std::stringstream s;

      if (tags.size() >= 2)
      {
        likelyString = true;
        auto it = tags.rbegin();
        likelyString &= *(it++) == dwarf_t::DW_TAG_const_type;
        likelyString &= *it     == dwarf_t::DW_TAG_pointer_type;
        it = tags.rbegin();
        likelyString |= *it     == dwarf_t::DW_TAG_pointer_type;

      }

      if (tag == dwarf_t::DW_TAG_array_type)
      {
        auto arrLen = arrayLength();
        int numBytes = 0;
        auto it = getType()->traverse.getAttribute(dwarf_t::DW_AT_byte_size);

        if (it.first)
        {
          numBytes = it.second.getInt();
        }

        if (arrLen.first && (numBytes != 0))
        {
          s << '[';

          for (int i = 0; i < arrLen.second; ++i)
          {
            if (i != 0) s << ", ";
            std::list<dwarf_t::tag_t> newTags(tags);
            newTags.push_back(tag);
            s << getType()->getValueStringByAddr(regCallback, memCallback, addr+(i*numBytes), newTags, depth+1, nullptr);
          }

          s << ']';
        }
        else
        {
          s << "[...unknown length...]";
        }
      }
      else if (isPointerType())
      {
        if (depth < Config::get(C_PTR_DEPTH).Int() && hasType())
        {
          std::list<dwarf_t::tag_t> newTags(tags);
          newTags.push_back(tag);
          s << "*[" << getType()->getValueStringByAddr(regCallback, memCallback, memCallback(addr), newTags, depth+1, memberMap) << ']';
        }
        else if (hasType())
        {
          uint64_t value = maskBytes(memCallback(addr), sizeof(void*));

          s << "*[" << HexPrint(value, (sizeof(void*)*2)) << ']';
        }
        else
        {
          // Pointer with no type is a void pointer
          s << "*[void]";
        }
      }
      else if ((tag == dwarf_t::DW_TAG_class_type) ||
               (tag == dwarf_t::DW_TAG_structure_type))
      {
        std::stringstream clsStrm;
        std::string clsName = "?";
        switch (tag)
        {
          case dwarf_t::DW_TAG_class_type: clsStrm << "class"; break;
          case dwarf_t::DW_TAG_structure_type: clsStrm << "struct"; break;
          default: break;
        }
        clsStrm << ' ';
        if (hasName())
        {
          clsName = getName();
        }
        clsStrm << clsName;
        clsStrm << " <" << HexPrint(addr, 1) << "> ";
        clsStrm << '{';
        bool first = true;
        // Member name -> (type, value)
        ClsMemMap members;
        ClsMemMap* m = memberMap == nullptr ? &members : memberMap;
        for (auto it = traverse.getChildIterator(true); !it.done(); it.next())
        {
          DIE * child = it.value();
          if (child->tag == dwarf_t::DW_TAG_member)
          {
            if (child->hasAttr(dwarf_t::DW_AT_data_member_location) &&
                child->hasType() && child->hasName())
            {
              if (first) first = false;
              else       clsStrm << ", ";

              uint64_t memAddr = addr + child->traverse.getAttribute(dwarf_t::DW_AT_data_member_location).second.getInt();
              clsStrm << child->getType()->typeString() << ' ' << child->getName() << " = ";
              // Reset tags, as only intended for const/pointer chains
              std::list<dwarf_t::tag_t> blankTags;
              clsStrm << child->getType()->getValueStringByAddr(regCallback, memCallback, memAddr, blankTags, depth+1, m);

              DIE * temp = child;
              while (temp->hasType())
              {
                if (temp->getType()->isPointerType() ||
                    (temp->getType()->tag == dwarf_t::DW_TAG_base_type) ||
                    (temp->getType()->tag == dwarf_t::DW_TAG_structure_type))
                {
                  if (m->find(child->getName()) == m->end())
                  {
                    m->insert({child->getName(), {temp->getType(), memAddr}});
                  }
                }
                else if ((temp->getType()->tag != dwarf_t::DW_TAG_typedef) &&
                         (temp->getType()->tag != dwarf_t::DW_TAG_const_type))
                {
                  // Only traverse down through typedefs or const
                  break;
                }
                temp = temp->getType();
              }
            }
          }
          else if (child->tag == dwarf_t::DW_TAG_typedef)
          {
            if (child->hasName())
            {
              std::string name = child->getName();
              if (name == "value_type")
              {
                m->insert({"@value_type", {child, 0}});
              }
              else if (name == "_Link_type")
              {
                m->insert({"@_Link_type", {child, 0}});
              }
              else if (name == "_Hashtable")
              {
                m->insert({"@_Hashtable", {child, 0}});
              }
            }
          }
        }
        clsStrm << " }";

        bool prettyPrinted = false;
        if (Config::get(C_PRETTY_PRINT).Bool())
        {
          auto pp = tryPrettyPrint(regCallback, memCallback, clsName, m);

          if (pp.length() > 0)
          {
            s << pp;
            prettyPrinted = true;
          }
        }
        if (!(prettyPrinted && Config::get(C_HIDE_NON_PRETTY_PRINT).Bool()))
        {
          if (prettyPrinted) s << std::endl;
          s << clsStrm.str();
        }
      }
      else if (tag == dwarf_t::DW_TAG_typedef)
      {
        if (hasType())
        {
          std::list<dwarf_t::tag_t> newTags(tags);
          newTags.push_back(tag);
          s << getType()->getValueStringByAddr(regCallback, memCallback, addr, newTags, depth+1, memberMap);
        }
        else
        {
          // Typedef with no type implies void
          s << "void";
        }
      }
      else if (tag == dwarf_t::DW_TAG_const_type)
      {
        std::list<dwarf_t::tag_t> newTags(tags);
        newTags.push_back(tag);
        s << getType()->getValueStringByAddr(regCallback, memCallback, addr, newTags, depth+1, memberMap);
      }
      else if (tag == dwarf_t::DW_TAG_base_type)
      {
        std::string typeName = getName();
        int numBytes = sizeof(void*);
        dwarf_t::ate_t enc = dwarf_t::DW_ATE_NULL;
        auto it = traverse.getAttribute(dwarf_t::DW_AT_byte_size);
        auto encIt = traverse.getAttribute(dwarf_t::DW_AT_encoding);

        if (it.first)
        {
          numBytes = it.second.getInt();
        }
        if (encIt.first)
        {
          enc = dwarf_t::convert_ate(encIt.second.getInt());
        }

        uint64_t value = maskBytes(memCallback(addr), numBytes);

        if (enc == dwarf_t::DW_ATE_boolean)
        {
          s << ((value == 0) ? "false" : "true");
        }
        else if ((enc == dwarf_t::DW_ATE_signed) ||
                 (enc == dwarf_t::DW_ATE_signed_char))
        {
          bool str = likelyString && (numBytes == 1) &&
              (enc == dwarf_t::DW_ATE_signed_char);
          if (str) s << tryGetString(addr, memCallback);
          else if (numBytes == 1) s << (int8_t)value;
          else if (numBytes == 2) s << (int16_t)value;
          else if (numBytes == 4) s << (int32_t)value;
          else if (numBytes == 8) s << (int64_t)value;
          else                    s << (int64_t)value;
        }
        else if (enc == dwarf_t::DW_ATE_unsigned)
        {
          if      (numBytes == 1) s << (uint32_t)((uint8_t)value);
          else if (numBytes == 2) s << (uint32_t)((uint16_t)value);
          else if (numBytes == 4) s << (uint32_t)value;
          else if (numBytes == 8) s << (uint64_t)value;
          else                    s << (uint64_t)value;
        }
        else if (enc == dwarf_t::DW_ATE_unsigned_char)
        {
          if (likelyString)
          {
            s << tryGetString(addr, memCallback);
          }
          else
          {
            if (depth >= 0) s << '\'';
            s << reinterpret_cast<unsigned char&>(value);
            if (depth >= 0) s << '\'';
          }
        }
        else if (enc == dwarf_t::DW_ATE_float)
        {
          if      (numBytes == 4) s << reinterpret_cast<float&>(value);
          else if (numBytes == 8) s << reinterpret_cast<double&>(value);
          else                    s << "unsupported float";
        }
        else if (enc == dwarf_t::DW_ATE_UTF)
        {
          if (likelyString)
          {
            if      (numBytes == 1) s << tryGetString(addr, memCallback);
            else if (numBytes == 2) s << tryGetString16(addr, memCallback);
            else if (numBytes == 4) s << tryGetString32(addr, memCallback);
            else                    s << "Unknown UTF-" << (numBytes*8);
          }
          else
          {
            // Single char cannot (usefully) be a multi-word character
            //  so is just the codepoint
            if (depth >= 0) s << '\'';
            if      (numBytes == 1) s << (unsigned char)value;
            else if (numBytes == 2) s << UTF32ToUTF8(value);
            else if (numBytes == 4) s << UTF32ToUTF8(value);
            else                    s << "Unknown UTF-" << (numBytes*8);
            if (depth >= 0) s << '\'';
          }

          // TODO does endianness differ here?
        }
        else
        {
          s << "unknown encoding: " << dwarf_t::ate_str(enc);
        }
      }
      else
      {
        s << "Can't represent: " << typeString() << " (" << dwarf_t::tag_str(tag) << ')';
      }
      return s.str();
    }

    std::string DIE::tryGetString(uint64_t addr, std::function<uint64_t(uint64_t)> memCallback)
    {
      std::stringstream s;
      int charCount = 0;
      std::queue<char> chars;
      std::queue<char> utf8char;
      bool readMoreChars = true;
      bool stringOk = true;
      uint64_t ptrToStr = addr;

      if (addr == 0) return "NULL";

      uint64_t val;

      s << '"';

      while (stringOk)
      {
        if (readMoreChars)
        {
          val = memCallback(ptrToStr);
          for (unsigned i = 0; i < sizeof(long); i++)
          {
            chars.push((val >> (i*8)) & 0xff);
          }
          readMoreChars = false;
          ptrToStr += sizeof(long);
        }

        char c = chars.front();

        if (c == 0)
        {
          stringOk = false;
        }
        else if (c & 0x80)
        {
          // UTF8 num bytes is number of leading ones
          unsigned needChars = ((c & 0xe0) == 0xc0) ? 2 :
                               ((c & 0xf0) == 0xe0) ? 3 :
                               ((c & 0xf8) == 0xf0) ? 4 :
                                                      0;
          if (needChars == 0)
          {
            // Not valid UTF8
            stringOk = false;
          }
          else if (needChars > chars.size())
          {
            // Loop round again after fetching another word
            readMoreChars = true;
          }
          else
          {
            utf8char.push(c);
            chars.pop();
            for (unsigned i = 1; (i < needChars) & stringOk; i++)
            {
              char c2 = chars.front();
              if ((c2 & 0xc0) == 0x80)
              {
                utf8char.push(c2);
                chars.pop();
              }
              else
              {
                stringOk = false;
              }
            }

            if (stringOk)
            {
              while (!utf8char.empty())
              {
                s << utf8char.front();
                utf8char.pop();
              }
              charCount++;
            }
          }
        }
        else
        {
          s << c;
          chars.pop();
          charCount++;
        }

        readMoreChars |= chars.empty();
        stringOk &= charCount < Config::get(C_CSTR_MAX_CHAR).Int();
      }

      s << '"' << " <" << HexPrint(addr, 1) << ">";

      return s.str();
    }

    std::string DIE::tryGetString16(uint64_t addr, std::function<uint64_t(uint64_t)> memCallback)
    {
      std::stringstream s;
      int charCount = 0;
      std::queue<char16_t> chars;
      std::queue<char16_t> utf16char;
      bool readMoreChars = true;
      bool stringOk = true;
      uint64_t ptrToStr = addr;

      if (addr == 0) return "NULL";

      uint64_t val;

      s << '"';

      while (stringOk)
      {
        if (readMoreChars)
        {
          val = memCallback(ptrToStr);
          for (unsigned i = 0; i < (sizeof(long)/2); i++)
          {
            chars.push((val >> (i*8*2)) & 0xffff);
          }
          readMoreChars = false;
          ptrToStr += sizeof(long);
        }

        char16_t c = chars.front();

        if (c == 0)
        {
          stringOk = false;
        }
        else if (c >= 0xd800 && c <= 0xdbff)
        {
          // UTF16 only ever has two chars
          if (chars.size() < 2)
          {
            // Loop round again after fetching another word
            readMoreChars = true;
          }
          else
          {
            utf16char.push(c);
            chars.pop();
            char16_t c2 = chars.front();
            if (c2 >= 0xdc00 && c2 <= 0xdfff)
            {
              utf16char.push(c2);
              chars.pop();
            }
            else
            {
              stringOk = false;
            }

            if (stringOk)
            {
              assert (utf16char.size() == 2);
              char16_t c1, c2;
              char32_t codepoint;

              c1 = utf16char.front();
              utf16char.pop();
              c2 = utf16char.front();
              utf16char.pop();

              codepoint = 0x10000 | ((c1 & 0x3ff) << 10) | (c2 & 0x3ff);

              s << UTF32ToUTF8(codepoint);
              charCount++;
            }
          }
        }
        else
        {
          if (c >= 0xdc00 && c <= 0xdfff)
          {
            stringOk = false;
          }
          else
          {
            // Convert to utf8
            char32_t codepoint = c;
            s << UTF32ToUTF8(codepoint);
            chars.pop();
            charCount++;
          }
        }

        readMoreChars |= chars.empty();
        stringOk &= charCount < Config::get(C_CSTR_MAX_CHAR).Int();
      }

      s << '"';

      return s.str();
    }

    std::string DIE::tryGetString32(uint64_t addr, std::function<uint64_t(uint64_t)> memCallback)
    {
      std::stringstream s;
      int charCount = 0;
      std::queue<char32_t> chars;
      bool readMoreChars = true;
      bool stringOk = true;
      uint64_t ptrToStr = addr;

      if (addr == 0) return "NULL";

      uint64_t val;

      s << '"';

      while (stringOk)
      {
        if (readMoreChars)
        {
          val = memCallback(ptrToStr);
          for (unsigned i = 0; i < (sizeof(long)/4); i++)
          {
            chars.push((val >> (i*8*4)) & 0xffffffff);
          }
          readMoreChars = false;
          ptrToStr += sizeof(long);
        }

        char32_t c = chars.front();

        if (c == 0)
        {
          stringOk = false;
        }
        else
        {
          if (c > 0x10ffff || ((c >= 0xd800) && (c <= 0xdfff)))
          {
            // Illegal UTF-32 characters
            stringOk = false;
          }
          else
          {
            s << UTF32ToUTF8(c);
            chars.pop();
            charCount++;
          }
        }

        readMoreChars |= chars.empty();
        stringOk &= charCount < Config::get(C_CSTR_MAX_CHAR).Int();
      }

      s << '"';
      return s.str();
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
