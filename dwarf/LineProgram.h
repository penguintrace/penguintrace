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
// DWARF Line Program

#ifndef DWARF_LINEPROGRAM_H_
#define DWARF_LINEPROGRAM_H_

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>

#include <istream>
#include <iomanip>

#include "definitions.h"

#include "Common.h"

namespace penguinTrace
{
  namespace dwarf
  {

    struct LineProgramFileEntry
    {
      public:
        LineProgramFileEntry(std::istream& ifs)
          : md5(0)
        {
          filename = ExtractString(ifs);
          if (Valid())
          {
            directory_index = ExtractULEB128(ifs);
            last_modified = ExtractULEB128(ifs);
            file_bytes = ExtractULEB128(ifs);
          }
          else
          {
            directory_index = 0;
            last_modified = 0;
            file_bytes = 0;
          }
        }
        LineProgramFileEntry(std::string name, uint64_t dir_index, uint64_t last_mod, uint64_t size, __uint128_t md5)
          : filename(name), directory_index(dir_index), last_modified(last_mod), file_bytes(size), md5(md5)
        {
        }
        bool Valid() { return filename.length() > 0; }
        std::string Name() { return filename; }
        uint64_t DirIndex() { return directory_index; }
        uint64_t Timestamp() { return last_modified; }
        uint64_t FileSize() { return file_bytes; }
        __uint128_t MD5() { return md5; }
      private:
        std::string filename;
        uint64_t directory_index;
        uint64_t last_modified;
        uint64_t file_bytes;
        __uint128_t md5;
    };

    struct LineProgramHeader
    {
      public:
        LineProgramHeader(std::istream& ifs, SectionMap& sections)
        {
          lp_start = ifs.tellg();
          auto init_length = ExtractInitialLength(ifs);
          arch = init_length.first;
          unit_length = init_length.second;
          unit_start = ifs.tellg();
          version = ExtractUInt16(ifs);
          if (version >= 5)
          {
            v5_address_size = ExtractUInt8(ifs);
            v5_segment_selector_size = ExtractUInt8(ifs);
          }
          header_length = ExtractSectionOffset(ifs, arch);
          minimum_instruction_length = ExtractUInt8(ifs);
          if (version >= 4)
          {
            maximum_operations_per_instruction = ExtractUInt8(ifs);
          }
          else
          {
            maximum_operations_per_instruction = 1;
          }
          default_is_stmt = ExtractUInt8(ifs) != 0;
          line_base = ExtractUInt8(ifs);
          line_range = ExtractUInt8(ifs);
          opcode_base = ExtractUInt8(ifs);
          for (int i = 0; i < opcode_base - 1; i++)
          {
            standard_opcode_lengths.push_back(ExtractUInt8(ifs));
          }
          if (version >= 5)
          {
            std::vector<std::pair<dwarf_t::lnct_t, dwarf_t::form_t>> directory_entries;
            std::vector<std::pair<dwarf_t::lnct_t, dwarf_t::form_t>> file_entries;
            int directory_entry_format_count = ExtractUInt8(ifs);
            for (int i = 0; i < directory_entry_format_count; i++)
            {
              dwarf_t::lnct_t content_type_code = dwarf_t::convert_lnct(ExtractULEB128(ifs));
              dwarf_t::form_t form_code = dwarf_t::convert_form(ExtractULEB128(ifs));
              directory_entries.push_back(std::make_pair(content_type_code, form_code));
            }
            uint64_t dir_names_count = ExtractULEB128(ifs);
            for (int i = 0; i < dir_names_count; i++)
            {
              std::string dir_name = "";
              for (auto it = directory_entries.begin(); it != directory_entries.end(); it++)
              {
                std::string tmp;
                switch (it->second) {
                  case dwarf_t::DW_FORM_line_strp:
                    tmp = ExtractStrp(arch, ifs, sections, dwarf_t::DW_SECTION_line_str);
                    break;
                  default:
                    throw Exception("Unknown form", __EINFO__);
                }
                switch (it->first) {
                  case dwarf_t::DW_LNCT_path:
                    dir_name = tmp;
                    break;
                  default:
                    throw Exception("Unknown type", __EINFO__);
                }
              }
              include_directories.push_back(dir_name);
            }

            int file_name_entry_format_count = ExtractUInt8(ifs);
            for (int i = 0; i < file_name_entry_format_count; i++)
            {
              dwarf_t::lnct_t content_type_code = dwarf_t::convert_lnct(ExtractULEB128(ifs));
              dwarf_t::form_t form_code = dwarf_t::convert_form(ExtractULEB128(ifs));
              file_entries.push_back(std::make_pair(content_type_code, form_code));
            }
            int file_names_count = ExtractULEB128(ifs);
            for (int i = 0; i < file_names_count; i++)
            {
              std::string filename = "";
              uint64_t dir_offset = 0;
              __uint128_t md5 = 0;
              uint64_t size = 0;
              uint64_t timestamp = 0;
              for (auto it = file_entries.begin(); it != file_entries.end(); it++)
              {
                std::stringstream s;
                __uint128_t value128;
                uint64_t value;
                switch (it->second) {
                  case dwarf_t::DW_FORM_string:
                    s << ExtractString(ifs);
                  case dwarf_t::DW_FORM_line_strp:
                    s << ExtractStrp(arch, ifs, sections, dwarf_t::DW_SECTION_line_str);
                    break;
                  case dwarf_t::DW_FORM_strp:
                    s << ExtractStrp(arch, ifs, sections, dwarf_t::DW_SECTION_str);
                    break;
                  // TODO supplemental string section
                  //case dwarf_t::DW_FORM_strp_sup:
                  //  s << "supplemental offset: " << ExtractSectionOffset(ifs, arch);
                  //  break;
                  case dwarf_t::DW_FORM_data1:
                    value = ExtractUInt8(ifs);
                    break;
                  case dwarf_t::DW_FORM_data2:
                    value = ExtractUInt16(ifs);
                    break;
                  case dwarf_t::DW_FORM_data4:
                    value = ExtractUInt32(ifs);
                    break;
                  case dwarf_t::DW_FORM_data8:
                    value = ExtractUInt64(ifs);
                    break;
                  case dwarf_t::DW_FORM_data16:
                    value128 = ExtractUInt128(ifs);
                    break;
                  case dwarf_t::DW_FORM_udata:
                    value = ExtractULEB128(ifs);
                    break;
                  default:
                    printf("%s\n", dwarf_t::form_str(it->second).c_str());
                    throw Exception("Unknown form", __EINFO__);
                }
                switch (it->first) {
                  case dwarf_t::DW_LNCT_path:
                    filename = s.str();
                    //TODO
                    break;
                  case dwarf_t::DW_LNCT_directory_index:
                    dir_offset = value;
                    break;
                  case dwarf_t::DW_LNCT_timestamp:
                    timestamp = value;
                    break;
                  case dwarf_t::DW_LNCT_size:
                    size = value;
                    break;
                  case dwarf_t::DW_LNCT_MD5:
                    md5 = value128;
                    break;
                  default:
                    printf("%s\n", dwarf_t::lnct_str(it->first).c_str());
                    throw Exception("Unknown LNCT", __EINFO__);
                }
              }
              auto file = LineProgramFileEntry(filename, dir_offset, timestamp, size, md5);
              file_names.push_back(file);
            }
          }
          else
          {
            bool valid;
            do
            {
              std::string inc_dir = ExtractString(ifs);
              valid = inc_dir.length() > 0;
              if (valid)
              {
                include_directories.push_back(inc_dir);
              }
            }
            while (valid);
            do
            {
              LineProgramFileEntry file(ifs);
              valid = file.Valid();
              if (valid)
              {
                file_names.push_back(file);
              }
            }
            while (valid);
          }
        }
        uint64_t LPHeaderStart() { return lp_start; }
        arch_t Arch() { return arch; }
        uint64_t UnitLength() { return unit_length; }
        uint16_t Version() { return version; }
        uint64_t HeaderLength() { return header_length; }
        uint8_t MinInstrLength() { return minimum_instruction_length; }
        uint8_t MaxOpsPerInstr() { return maximum_operations_per_instruction; }
        bool DefaultIsStmt() { return default_is_stmt; }
        int8_t LineBase() { return line_base; }
        uint8_t LineRange() { return line_range; }
        uint8_t OpcodeBase() { return opcode_base; }
        std::vector<uint8_t>& OpcodeLengths() { return standard_opcode_lengths; }
        std::vector<std::string>& IncDirs() { return include_directories; }
        std::vector<LineProgramFileEntry>& FileNames() { return file_names; }
        std::string Filename(int index);
        bool Contains(uint64_t offset)
        {
          return (offset < (unit_start + unit_length)) && (offset > unit_start);
        }
        void addFilename(std::istream& is);
      private:
        uint64_t lp_start;
        arch_t arch;
        uint64_t unit_length;
        uint64_t unit_start;
        uint16_t version;
        uint64_t header_length;
        uint8_t minimum_instruction_length;
        uint8_t maximum_operations_per_instruction;
        bool default_is_stmt;
        int8_t line_base;
        uint8_t line_range;
        uint8_t opcode_base;
        uint8_t v5_address_size;
        uint8_t v5_segment_selector_size;
        std::vector<uint8_t> standard_opcode_lengths;
        std::vector<std::string> include_directories;
        std::vector<LineProgramFileEntry> file_names;
    };

    struct LineStateMachine
    {
      public:
        LineStateMachine(LineProgramHeader& header)
            : header(header)
        {
          ResetInitial();
        }
        void SetAddr(uint64_t a)
        {
          address = a;
          op_index = 0;
        }
        void AdvancePC(uint64_t adv)
        {
          // Increment address/op_index
          address += (header.MinInstrLength()
              * ((op_index + adv) / header.MaxOpsPerInstr()));
          op_index = (op_index + adv) % header.MaxOpsPerInstr();
        }
        void FixedAdvancePC(uint16_t adv)
        {
          address += adv;
          op_index = 0;
        }
        void AdvanceLine(int64_t l)
        {
          line += l;
        }
        void SetDiscriminator(uint64_t d)
        {
          discriminator = d;
        }
        void SetColumn(uint64_t c)
        {
          column = c;
        }
        void SetFile(uint64_t f)
        {
          file = f;
        }
        void SetISA(uint64_t i)
        {
          isa = i;
        }
        void ToggleStmt()
        {
          is_stmt = !is_stmt;
        }
        void SetPrologueEnd()
        {
          prologue_end = true;
        }
        uint64_t GetPC()
        {
          return address;
        }
        uint64_t GetFile()
        {
          return file;
        }
        uint64_t GetLine()
        {
          return line;
        }
        uint64_t GetColumn()
        {
          return column;
        }
        bool GetPrologueEnd()
        {
          return prologue_end;
        }
        bool GetEpilogueBegin()
        {
          return epilogue_begin;
        }
        void SetBasicBlock()
        {
          basic_block = true;
        }
        void SetEpilogueBegin()
        {
          epilogue_begin = true;
        }
        LineStateMachine SpecialOp(uint8_t op)
        {
          int32_t adj_op = (int32_t)op - (int32_t)header.OpcodeBase();
          int32_t adv = adj_op / header.LineRange();
          // Add a signed int to line
          line += header.LineBase() + (adj_op % header.LineRange());
          // Increment address/op_index
          address += (header.MinInstrLength()
              * ((op_index + adv) / header.MaxOpsPerInstr()));
          op_index = (op_index + adv) % header.MaxOpsPerInstr();
          // Append a row to Matrix
          LineStateMachine row(*this);
          // Reset values
          ResetRegs();
          return row;
        }
        LineStateMachine Copy()
        {
          LineStateMachine row(*this);
          ResetRegs();
          return row;
        }
        LineStateMachine EndSequence()
        {
          end_sequence = true;
          LineStateMachine row(*this);
          ResetInitial();
          return row;
        }
        static std::string HeaderString()
        {
          return "address,op_index,file,line,column,is_stmt,"
              "basic_block,end_sequence,prologue_end,"
              "epilogue_begin,isa,discriminator";
        }
        std::string String()
        {
          std::stringstream ss;
          ss << "0x" << std::setbase(16) << address << ",";
          ss << op_index << ",";
          ss << file << ",";
          ss << line << ",";
          ss << column << ",";
          ss << (is_stmt ? "true" : "false") << ",";
          ss << (basic_block ? "true" : "false") << ",";
          ss << (end_sequence ? "true" : "false") << ",";
          ss << (prologue_end ? "true" : "false") << ",";
          ss << (epilogue_begin ? "true" : "false") << ",";
          ss << isa << ",";
          ss << discriminator;
          return ss.str();
        }
      private:
        void ResetInitial()
        {
          address = 0;
          op_index = 0;
          file = 1;
          line = 1;
          column = 0;
          is_stmt = header.DefaultIsStmt();
          basic_block = false;
          end_sequence = false;
          prologue_end = false;
          epilogue_begin = false;
          isa = 0;
          discriminator = 0;
        }
        void ResetRegs()
        {
          basic_block = false;
          prologue_end = false;
          epilogue_begin = false;
          discriminator = 0;
        }

        LineProgramHeader& header;
        uint64_t address;
        uint64_t op_index;
        uint64_t file;
        uint64_t line;
        uint64_t column;
        bool is_stmt;
        bool basic_block;
        bool end_sequence;
        bool prologue_end;
        bool epilogue_begin;
        uint64_t isa;
        uint64_t discriminator;
    };

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_LINEPROGRAM_H_ */
