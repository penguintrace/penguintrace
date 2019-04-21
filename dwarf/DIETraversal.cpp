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
// DWARF Debug Information Entries - DIE Tree Traversal

#include "DIE.h"

#include <sstream>

#include "../common/StreamOperations.h"
#include "../common/MemoryBuffer.h"

namespace penguinTrace
{
  namespace dwarf
  {

    std::string DIE::Traverse::exprlocString(dwarf_t::at_t at)
    {
      std::stringstream s;
      auto locIt = getAttribute(at);
      assert(locIt.first);

      MemoryBuffer buffer(locIt.second.getBuffer().data(),
          locIt.second.getBuffer().size());
      std::istream is(&buffer);
      is.peek();

      while (!is.eof())
      {
        dwarf_t::op_t op = dwarf_t::convert_op(ExtractUInt8(is));
        std::pair<uint64_t, uint64_t> operand = getOpOperand(op, is, arch);

        s << dwarf_t::op_str(op);
        s << " (" << (int64_t)(operand.first) << ")";
        s << std::endl;

        is.peek();
      }

      return s.str();
    }


    void DIE::Traverse::addAttribute(dwarf_t::at_t at, AttrValue value)
    {
      attributes.insert( {at, value} );
    }

    void DIE::Traverse::addChild(std::unique_ptr<DIE> die)
    {
      children.push_back(std::move(die));
    }

    DIE::Traverse::Iterator DIE::Traverse::getChildIterator(bool inherit)
    {
      return Iterator(&children, inherit);
    }

    std::pair<bool, AttrValue> DIE::Traverse::getAttribute(dwarf_t::at_t at)
    {
      auto it = attributes.find(at);
      if (it != attributes.end())
      {
        return {true, it->second};
      }
      auto specAttrIt = attributes.find(dwarf_t::DW_AT_specification);
      if (specAttrIt != attributes.end())
      {
        // Attributes that don't apply to specification
        if ((at != dwarf_t::DW_AT_sibling) &&
            (at != dwarf_t::DW_AT_declaration))
        {
          auto specIt = dies->find(specAttrIt->second.getInt());
          if (specIt != dies->end())
          {
            DIE* spec = specIt->second;
            return spec->traverse.getAttribute(at);
          }
        }
      }
      return {false, AttrValue()};
    }

    int DIE::Traverse::numChildren()
    {
      return children.size();
    }

    std::string DIE::Traverse::toString(bool recurse, int indent)
    {
      std::stringstream s;
      for (auto it = attributes.begin(); it != attributes.end(); ++it)
      {
        s << StringPad("", (indent+1)*2);
        s << "- ";
        s << StringPad(dwarf_t::at_str(it->first), 18);
        s << it->second.toString();
        s << " (" << dwarf_t::form_str(it->second.form()) << ")";
        s << std::endl;
        if (it->second.form() == dwarf_t::DW_FORM_exprloc)
        {
          s << StringPad("", (indent+2)*2);
          s << exprlocString(it->first);
        }
      }
      if (recurse)
      {
        for (auto it = children.begin(); it != children.end(); ++it)
        {
          s << (*it)->toString(true, indent+1);
        }
      }
      return s.str();
    }

    DIE::Traverse::Iterator::~Iterator()
    {
      if (nestedIt != nullptr)
      {
        delete nestedIt;
        nestedIt = nullptr;
      }
    }

    bool DIE::Traverse::Iterator::done()
    {
      return it == list->end();
    }

    void DIE::Traverse::Iterator::next()
    {
      if (nestedIt != nullptr)
      {
        nestedIt->next();
        if (nestedIt->done())
        {
          delete nestedIt;
          nestedIt = nullptr;
          ++it;
        }
      }
      else
      {
        ++it;
      }
    }

    DIE* DIE::Traverse::Iterator::value()
    {
      if (nestedIt != nullptr) return nestedIt->value();

      DIE* d = it->get();

      auto import = d->traverse.getAttribute(dwarf_t::DW_AT_import);
      if (import.first)
      {
        auto imported = d->dies->find(import.second.getInt());
        assert(imported != d->dies->end());
        return imported->second;
      }

      if (inherit && (d->tag == dwarf_t::DW_TAG_inheritance))
      {
        nestedIt = new Iterator(&d->getType()->traverse.children, true);
        return nestedIt->value();
      }

      return d;
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
