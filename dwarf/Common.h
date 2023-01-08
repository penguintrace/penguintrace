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
// DWARF Common Functions

#ifndef DWARF_COMMON_H_
#define DWARF_COMMON_H_

#include <cassert>

#include <istream>
#include <stdint.h>
#include <string>
#include <vector>

#include "../common/Exception.h"

#include "../object/Parser.h"

namespace penguinTrace
{
  namespace dwarf
  {

    typedef std::map<dwarf_t::section_t, penguinTrace::object::Parser::SectionPtr> SectionMap;

    // Mask of data bits
    const uint8_t LEB128_MASK = 0x7f;
    // Mask for end bit
    const uint8_t LEB128_nMASK = 0x80;
    const uint8_t LEB128_sMASK = 0x40;

    const uint32_t DWARF_INIT_LEN_MAX = 0xfffffff0;
    const uint32_t DWARF_INIT_LEN_64B = 0xffffffff;

    enum arch_t
    {
      DWARF32,
      DWARF64
    };

    enum value_class_t
    {
      DWARF_NULL,
      DWARF_BOOL, // Boolean (bool)
      DWARF_INT,  // Integer (uint64_t)
      DWARF_STR,  // String  (std::string)
      DWARF_BUF   // Buffer  (std::vector<uint8_t>)
    };

    inline bool LEB128_LAST(uint8_t byte)
    {
      return !((byte & LEB128_nMASK) == LEB128_nMASK);
    }

    inline bool LEB128_SIGNED(uint8_t byte)
    {
      return ((byte & LEB128_sMASK) == LEB128_sMASK);
    }

    inline uint8_t ExtractUInt8(std::istream& ifs)
    {
      return ifs.get();
    }

    inline uint16_t ExtractUInt16(std::istream& ifs)
    {
      uint16_t value;
      ifs.read(reinterpret_cast<char *>(&value), sizeof(value));
      return value;
    }

    inline uint32_t ExtractUInt32(std::istream& ifs)
    {
      uint32_t value;
      ifs.read(reinterpret_cast<char *>(&value), sizeof(value));
      return value;
    }

    inline uint64_t ExtractUInt64(std::istream& ifs)
    {
      uint64_t value;
      ifs.read(reinterpret_cast<char *>(&value), sizeof(value));
      return value;
    }

    inline __uint128_t ExtractUInt128(std::istream& ifs)
    {
      __uint128_t value;
      ifs.read(reinterpret_cast<char *>(&value), sizeof(value));
      return value;
    }

    inline uint64_t ExtractULEB128(std::istream& ifs)
    {
      uint64_t value = 0;
      uint8_t byte;
      int offset = 0;
      bool done;

      do
      {
        byte = ifs.get();
        done = LEB128_LAST(byte) || (offset >= 64);
        value |= ((byte & LEB128_MASK) << offset);
        offset += 7;
      }
      while (!done);

      if ((offset >= 64) && !LEB128_LAST(byte))
      {
        throw Exception("ULEB128 overflow", __EINFO__);
      }

      return value;
    }

    inline int64_t ExtractSLEB128(std::istream& ifs)
    {
      uint64_t value = 0;
      uint8_t byte;
      int offset = 0;
      bool done;

      do
      {
        byte = ifs.get();
        done = LEB128_LAST(byte) || (offset >= 64);
        value |= ((byte & LEB128_MASK) << offset);
        offset += 7;
      }
      while (!done);

      if ((offset < 64) && LEB128_SIGNED(byte))
      {
        value |= (~0ULL << offset);
      }

      if ((offset >= 64) && !LEB128_LAST(byte))
      {
        throw Exception("SLEB128 overflow", __EINFO__);
      }

      return (int64_t)value;
    }

    inline std::pair<arch_t, uint64_t> ExtractInitialLength(std::istream& ifs)
    {
      uint32_t val = ExtractUInt32(ifs);
      assert(
          ((val <= DWARF_INIT_LEN_MAX) || (val == DWARF_INIT_LEN_64B))
              && "Reserved value for DWARF initial length");
      arch_t arch = (val == DWARF_INIT_LEN_64B) ? DWARF64 : DWARF32;

      if (arch == DWARF64)
      {
        return std::pair<arch_t, uint64_t>(arch, ExtractUInt64(ifs));
      }
      else
      {
        return std::pair<arch_t, uint64_t>(arch, static_cast<uint64_t>(val));
      }
    }

    inline uint64_t ExtractNumBytes(std::istream& ifs, uint8_t num_bytes)
    {
      switch (num_bytes)
      {
        case 1: return ExtractUInt8(ifs);
        case 2: return ExtractUInt16(ifs);
        case 4: return ExtractUInt32(ifs);
        case 8: return ExtractUInt64(ifs);
        default:
          throw Exception("Unsupported number of bytes", __EINFO__);
      }
    }

    inline uint64_t ExtractSectionOffset(std::istream& ifs, arch_t a)
    {
      return (a == DWARF64) ? ExtractUInt64(ifs) : ExtractUInt32(ifs);
    }

    inline std::string ExtractString(std::istream& ifs)
    {
      std::string s = "";
      ifs.peek();
      if (ifs.eof()) return s;

      char c = ifs.get();
      while (c != '\0' && !ifs.eof())
      {
        s += c;
        c = ifs.get();
      }

      return s;
    }

    inline std::string ExtractStrp(arch_t arch, std::istream& is, SectionMap& sections, dwarf_t::section_t section)
    {
      uint64_t offset = ExtractSectionOffset(is, arch);

      auto it = sections.find(section);
      assert(it != sections.end() && "Indirect string without matching section present");
      auto iStrm = it->second->getContents();
      std::istream istr(&iStrm);

      istr.seekg(offset);

      return ExtractString(istr);
    }

    inline std::string ExtractIndirectStrp(uint64_t off, arch_t arch, SectionMap& sections, dwarf_t::section_t section, dwarf_t::section_t ssection)
    {
      auto it = sections.find(section);
      assert(it != sections.end() && "Indirect string without matching section present");
      auto iStrm = it->second->getContents();
      std::istream istr(&iStrm);

      uint64_t actual_offset =
        // Unit length
        ((arch == DWARF64) ? 12 : 4) +
        // Version
        2 +
        // Padding
        2 +
        // Offset
        (off * ((arch == DWARF64) ? 8 : 4));

      istr.seekg(actual_offset);

      uint64_t offset = ExtractSectionOffset(istr, arch);

      auto sit = sections.find(ssection);
      assert(sit != sections.end() && "Indirect string without matching section present");
      auto siStrm = sit->second->getContents();
      std::istream sistr(&siStrm);

      sistr.seekg(offset);

      return ExtractString(sistr);
    }

    inline void ExtractBuffer(std::istream& ifs, std::vector<uint8_t>& buf, uint64_t length)
    {
      for (uint64_t i = 0; i < length; i++)
      {
        buf.push_back(ifs.get());
      }
    }

    inline uint64_t ExtractBlock(std::istream& ifs, std::vector<uint8_t>& buf, uint8_t num_bytes)
    {
      uint64_t length = ExtractNumBytes(ifs, num_bytes);
      assert(length != UINT64_MAX);
      ExtractBuffer(ifs, buf, length);
      return length;
    }

    inline uint64_t ExtractExprLoc(std::istream& ifs, std::vector<uint8_t>& buf)
    {
      uint64_t length = ExtractULEB128(ifs);
      assert(length != UINT64_MAX);
      ExtractBuffer(ifs, buf, length);
      return length;
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_COMMON_H_ */
