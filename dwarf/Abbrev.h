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
// DWARF Abbreviation Table Entry

#ifndef DWARF_ABBREV_H_
#define DWARF_ABBREV_H_

#include "definitions.h"

#include <vector>

namespace penguinTrace
{
  namespace dwarf
  {

    struct AbbrevAttrib
    {
      public:
        AbbrevAttrib(dwarf_t::at_t a, dwarf_t::form_t f)
            : at(a), form(f)
        {
        }
        dwarf_t::at_t GetAT()
        {
          return at;
        }
        dwarf_t::form_t GetForm()
        {
          return form;
        }
      private:
        dwarf_t::at_t at;
        dwarf_t::form_t form;
    };

    struct AbbrevTableEntry
    {
      public:
        typedef std::vector<AbbrevAttrib> AttribList;

        AbbrevTableEntry(dwarf_t::tag_t t, dwarf_t::children_t ch)
            : tag(t), has_children(ch)
        {
        }
        void AddAttrib(const AbbrevAttrib& a)
        {
          attributes.push_back(a);
        }
        dwarf_t::tag_t GetTag()
        {
          return tag;
        }
        dwarf_t::children_t HasChildren()
        {
          return has_children;
        }
        AttribList& GetAttrs()
        {
          return attributes;
        }
      private:
        dwarf_t::tag_t tag;
        dwarf_t::children_t has_children;
        AttribList attributes;
    };

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_ABBREV_H_ */
