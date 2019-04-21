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
// Common Stream Operations

#include "StreamOperations.h"

namespace penguinTrace
{

  std::ostream & operator<<(std::ostream &out, const HexPrint &obj)
  {
    obj.print(out);
    return out;
  }

  std::ostream & operator<<(std::ostream &out, const IntPrint &obj)
  {
    obj.print(out);
    return out;
  }

  std::ostream & operator<<(std::ostream &out, const SIntPrint &obj)
  {
    obj.print(out);
    return out;
  }

  std::ostream & operator<<(std::ostream &out, const StringPad &obj)
  {
    obj.print(out);
    return out;
  }

  std::ostream & operator<<(std::ostream &out, const UTF32ToUTF8 &obj)
  {
    obj.print(out);
    return out;
  }

  void UTF32ToUTF8::print(std::ostream &out) const
  {
    if (codepoint < 0x80)
    {
      out << (char)(codepoint & 0xff);
    }
    else if (codepoint < 0x800)
    {
      unsigned char c1, c2;
      c1 = 0xc0 | ((codepoint >> 6) & 0x1f);
      c2 = 0x80 | (codepoint & 0x3f);
      out << c1 << c2;
    }
    else if (codepoint < 0x10000)
    {
      unsigned char c1, c2, c3;
      c1 = 0xe0 | ((codepoint >> 12) & 0xf);
      c2 = 0x80 | ((codepoint >> 6) & 0x3f);
      c3 = 0x80 | (codepoint & 0x3f);
      out << c1 << c2 << c3;
    }
    else // c < 0x110000
    {
      unsigned char c1, c2, c3, c4;
      c1 = 0xf0 | ((codepoint >> 18) & 0x7);
      c2 = 0x80 | ((codepoint >> 12) & 0x3f);
      c3 = 0x80 | ((codepoint >> 6) & 0x3f);
      c4 = 0x80 | (codepoint & 0x3f);
      out << c1 << c2 << c3 << c4;
    }
  }

} /* namespace penguinTrace */
