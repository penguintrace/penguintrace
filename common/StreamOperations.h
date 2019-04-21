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

#ifndef COMMON_STREAMOPERATIONS_H_
#define COMMON_STREAMOPERATIONS_H_

#include <iostream>
#include <iomanip>
#include <ios>

namespace penguinTrace
{

  class HexPrint
  {
    public:
      HexPrint(uint64_t i, int n)
          : i(i), n(n)
      {
      }
      void print(std::ostream &out) const
      {
        std::ios::fmtflags flags(out.flags());
        out << "0x" << std::hex << std::setfill('0') << std::setw(n) << i;
        out.flags(flags);
      }
    private:
      uint64_t i;
      int n;
  };

  std::ostream &operator<<(std::ostream &out, const HexPrint &obj);

  class IntPrint
  {
    public:
      IntPrint(uint64_t i, int n)
          : i(i), n(n)
      {
      }
      void print(std::ostream &out) const
      {
        std::ios::fmtflags flags(out.flags());
        out << std::dec << std::setfill(' ') << std::setw(n) << i;
        out.flags(flags);
      }
    private:
      uint64_t i;
      int n;
  };

  std::ostream &operator<<(std::ostream &out, const IntPrint &obj);

  class SIntPrint
  {
    public:
      SIntPrint(int64_t i, int n)
          : i(i), n(n)
      {
      }
      void print(std::ostream &out) const
      {
        std::ios::fmtflags flags(out.flags());
        out << std::dec << std::setfill(' ') << std::setw(n) << i;
        out.flags(flags);
      }
    private:
      int64_t i;
      int n;
  };

  std::ostream &operator<<(std::ostream &out, const SIntPrint &obj);

  class StringPad
  {
    public:
      StringPad(const std::string &str, int i, bool l = true)
          : str(str), i(i), l(l)
      {
      }
      void print(std::ostream &out) const
      {
        std::ios::fmtflags flags(out.flags());
        if (l)
          out << std::left;
        out << std::setfill(' ') << std::setw(i) << str;
        out.flags(flags);
      }
    private:
      const std::string &str;
      int i;
      bool l;
  };

  std::ostream &operator<<(std::ostream &out, const StringPad &obj);

  class UTF32ToUTF8
  {
    public:
      UTF32ToUTF8(char32_t codepoint)
          : codepoint(codepoint)
      {
      }
      void print(std::ostream &out) const;
    private:
      char32_t codepoint;
  };

  std::ostream &operator<<(std::ostream &out, const UTF32ToUTF8 &obj);

} /* namespace penguinTrace */

#endif /* COMMON_STREAMOPERATIONS_H_ */
