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

#include "AttrValue.h"

#include <sstream>
#include "../common/StreamOperations.h"

namespace penguinTrace
{
  namespace dwarf
  {

    std::string AttrValue::toString(bool plus)
    {
      std::stringstream s;
      if (valueClass == DWARF_BOOL)
      {
        s << (boolValue ? "true" : "false");
      }
      else if (valueClass == DWARF_INT)
      {
        if (intSigned)
        {
          if ((int64_t)intValue > 0 && plus)
          {
            s << "+";
          }
          s << (int64_t)intValue;
        }
        else
        {
          s << HexPrint(intValue, intLength*2);
        }
      }
      else if (valueClass == DWARF_STR)
      {
        s << strValue;
      }
      else if (valueClass == DWARF_BUF)
      {
        s << "{";
        for (unsigned i = 0; i < bufValue.size(); ++i)
        {
          if (i != 0) s << ", ";
          s << HexPrint(bufValue[i], 2);
        }
        s << "}";
      }
      else
      {
        s << "NULL";
      }
      return s.str();
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
