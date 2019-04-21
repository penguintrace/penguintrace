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
// DWARF Attribute Value

#ifndef DWARF_ATTRVALUE_H_
#define DWARF_ATTRVALUE_H_

#include "definitions.h"

#include "Common.h"

namespace penguinTrace
{
  namespace dwarf
  {

    struct AttrValue
    {
      public:
        AttrValue() :
            attribForm(dwarf_t::DW_FORM_NULL), valueClass(DWARF_NULL), boolValue(
                false), intValue(0), intLength(0), intSigned(false), strValue(
                "")
        {
        }
        AttrValue(dwarf_t::form_t form, bool value) :
            attribForm(form), valueClass(DWARF_BOOL), boolValue(value), intValue(
                0), intLength(0), intSigned(false), strValue("")
        {
        }
        AttrValue(dwarf_t::form_t form, uint64_t value, uint8_t numBytes,
            bool sign) :
            attribForm(form), valueClass(DWARF_INT), boolValue(false), intValue(
                value), intLength(numBytes), intSigned(sign), strValue("")
        {
        }
        AttrValue(dwarf_t::form_t form, std::string value) :
            attribForm(form), valueClass(DWARF_STR), boolValue(false), intValue(
                0), intLength(0), intSigned(false), strValue(value)
        {
        }
        AttrValue(dwarf_t::form_t form, std::vector<uint8_t> value) :
            attribForm(form), valueClass(DWARF_BUF), boolValue(false), intValue(
                0), intLength(0), intSigned(false), strValue(""), bufValue(
                value)
        {
        }
        bool getBool()
        {
          if (valueClass != DWARF_BOOL) throw Exception("Value is not a boolean", __EINFO__);
          return boolValue;
        }
        uint64_t getInt()
        {
          if (valueClass != DWARF_INT) throw Exception("Value is not an integer", __EINFO__);
          if (intSigned) throw Exception("Value is signed", __EINFO__);
          return intValue;
        }
        int64_t getSInt()
        {
          if (valueClass != DWARF_INT) throw Exception("Value is not an integer", __EINFO__);
          if (!intSigned) throw Exception("Value is not signed", __EINFO__);
          return intValue;
        }
	      bool isSigned()
        {
          return intSigned;
        }
        std::string getString()
        {
          if (valueClass != DWARF_STR) throw Exception("Value is not a string", __EINFO__);
          return strValue;
        }
        uint8_t numBytes()
        {
          if (valueClass != DWARF_INT) throw Exception("Value is not an integer", __EINFO__);
          return intLength;
        }
        std::vector<uint8_t>& getBuffer()
        {
          if (valueClass != DWARF_BUF) throw Exception("Value is not a buffer", __EINFO__);
          return bufValue;
        }
        value_class_t type()
        {
          return valueClass;
        }
        dwarf_t::form_t form()
        {
          return attribForm;
        }
        std::string toString(bool plus=false);
      private:
        dwarf_t::form_t attribForm;
        value_class_t valueClass;
        bool          boolValue;
        uint64_t      intValue;
        uint8_t       intLength; // In Bytes
        bool          intSigned;
        std::string   strValue;
        std::vector<uint8_t> bufValue;
    };

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_ATTRVALUE_H_ */
