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

#ifndef DWARF_DIE_H_
#define DWARF_DIE_H_

#include "definitions.h"

#include "Common.h"

#include <map>
#include <list>
#include <memory>
#include <functional>

#include "AttrValue.h"
#include "Frame.h"

#include "../common/ComponentLogger.h"

namespace penguinTrace
{
  namespace dwarf
  {
    struct CompilationUnitHeader
    {
      public:
        CompilationUnitHeader(std::istream& ifs)
        {
          hdr_start = ifs.tellg();
          auto init_length = ExtractInitialLength(ifs);
          arch = init_length.first;
          unit_length = init_length.second;
          unit_start = ifs.tellg();
          version = ExtractUInt16(ifs);
          abbrev_offset = ExtractSectionOffset(ifs, arch);
          addr_bytes = ExtractUInt8(ifs);
        }
        arch_t Arch() { return arch; }
        uint16_t Version() { return version; }
        uint64_t AbbrevOffset() { return abbrev_offset; }
        uint8_t AddrBytes() { return addr_bytes; }
        uint64_t CUHeaderStart() { return hdr_start; }
        bool Contains(uint64_t offset)
        {
          return (offset < (unit_start + unit_length)) && (offset > unit_start);
        }
      private:
        arch_t arch;
        // Start (stream pointer at first byte of initial length)
        uint64_t hdr_start;
        // Length from unit_start
        uint64_t unit_length;
        // Start (stream pointer after initial length)
        uint64_t unit_start;
        uint16_t version;
        uint64_t abbrev_offset;
        uint8_t addr_bytes;
    };

    class DIE
    {
      public:
        typedef std::function<uint64_t(uint64_t)> MemCallback;
        typedef std::function<uint64_t(std::string)> RegCallback;
        typedef std::shared_ptr<std::map<uint64_t, DIE*> > SharedDIEMap;

        DIE(uint64_t offset, dwarf_t::tag_t tag, DIE* parent,
            SharedDIEMap dies,
            std::shared_ptr<Frames> frames, arch_t arch,
            std::shared_ptr<ComponentLogger> logger) :
            offset(offset), tag(tag), parent(parent),
            traverse(Traverse(dies, arch)), dies(dies),
            frames(frames), arch(arch), typeStrCache(""),
            logger(logger)
        {
        }
        DIE() = delete;
        virtual ~DIE();
        void addAttribute(dwarf_t::at_t at, AttrValue value);
        void addChild(std::unique_ptr<DIE> die);
        DIE* getParent()
        {
          return parent;
        }
        DIE* getFirstChild();
        dwarf_t::tag_t getTag()
        {
          return tag;
        }
        bool hasName()
        {
          return traverse.getAttribute(dwarf_t::DW_AT_name).first;
        }
        std::string getName()
        {
          auto val = traverse.getAttribute(dwarf_t::DW_AT_name);
          // Only handle DIEs with names, anon objects should be ignored
          assert(val.first);
          return val.second.getString();
        }
        std::string toString(bool recurse=true, int indent=0);
        bool isFunction()
        {
          // TODO other function types? e.g. inlined functions
          return (tag == dwarf_t::DW_TAG_subprogram) && hasPcRange();
        }
        bool isArtificial()
        {
          auto val = traverse.getAttribute(dwarf_t::DW_AT_artificial);
          if (val.first)
          {
            return val.second.getBool();
          }
          return false;
        }
        bool isDataObject()
        {
          return ((tag == dwarf_t::DW_TAG_variable) ||
                 (tag == dwarf_t::DW_TAG_formal_parameter) ||
                 (tag == dwarf_t::DW_TAG_constant)) &&
                 !isArtificial() && hasName() &&
                 hasAttr(dwarf_t::DW_AT_location) && hasType();
        }
        bool isFormalParam()
        {
          return (tag == dwarf_t::DW_TAG_formal_parameter) &&
                 !isArtificial() && hasName();
        }
        bool hasType()
        {
          return traverse.getAttribute(dwarf_t::DW_AT_type).first;
        }
        bool hasAttr(dwarf_t::at_t a)
        {
          return traverse.getAttribute(a).first;
        }
        DIE* getType()
        {
          if (!hasType()) throw Exception("DIE doesn't have type:\n"+toString(true), __EINFO__);
          auto it = dies->find(traverse.getAttribute(dwarf_t::DW_AT_type).second.getInt());
          assert(it != dies->end());
          return it->second;
        }
        std::string typeString();
        void typeList(std::vector<dwarf_t::tag_t>* list)
        {
          switch (tag)
          {
            case dwarf_t::DW_TAG_array_type:
            case dwarf_t::DW_TAG_pointer_type:
            case dwarf_t::DW_TAG_reference_type:
              list->push_back(tag);
              getType()->typeList(list);
              break;
            case dwarf_t::DW_TAG_base_type:
              list->push_back(tag);
              break;
            default:
              throw Exception("Unhandled type", __EINFO__);
          }
        }
        bool hasPcRange()
        {
          return (traverse.getAttribute(dwarf_t::DW_AT_low_pc).first) &&
                 (traverse.getAttribute(dwarf_t::DW_AT_high_pc).first);
        }
        uint64_t lowPC()
        {
          auto val = traverse.getAttribute(dwarf_t::DW_AT_low_pc);
          assert(val.first);
          return val.second.getInt();
        }
        bool containsPC(uint64_t pc);
        DIE* functionContaining(uint64_t pc)
        {
          // TODO recurse for inlined functions?
          if (isFunction())
          {
            if (containsPC(pc))
            {
              return this;
            }
            else
            {
              return nullptr;
            }
          }
          // Iterate over children
          for (auto it = traverse.getChildIterator(false); !it.done(); it.next())
          {
            DIE* d = it.value()->functionContaining(pc);
            if (d != nullptr) return d;
          }
          return nullptr;
        }
        DIE* functionByName(std::string name)
        {
          if (isFunction())
          {
            if (hasName() && getName().compare(name) == 0)
            {
              return this;
            }
          }
          for (auto it = traverse.getChildIterator(false); !it.done(); it.next())
          {
            DIE* d = it.value()->functionByName(name);
            if (d != nullptr) return d;
          }
          return nullptr;
        }
        void dataObjects(uint64_t pc, std::list<DIE*>& list)
        {
          if (hasPcRange())
          {
            if (!containsPC(pc))
            {
              return;
            }
          }

          if (isDataObject())
          {
            list.push_back(this);
          }

          // Loop over children
          // (assumes everything will be contained in a PC range)
          for (auto it = traverse.getChildIterator(false); !it.done(); it.next())
          {
            DIE * d = it.value();
            // Don't recurse over subprograms
            if (d->tag != dwarf_t::DW_TAG_subprogram)
            {
              d->dataObjects(pc, list);
            }
          }
        }
        void formalParams(uint64_t pc, std::list<DIE*>& list)
        {
          if (hasPcRange())
          {
            if (!containsPC(pc))
            {
              return;
            }
          }

          if (isFormalParam())
          {
            list.push_back(this);
          }

          // Loop over children
          // (assumes everything will be contained in a PC range)
          for (auto it = traverse.getChildIterator(false); !it.done(); it.next())
          {
            DIE * d = it.value();
            // Don't recurse over subprograms
            if (d->tag != dwarf_t::DW_TAG_subprogram)
            {
              d->formalParams(pc, list);
            }
          }
        }
        static std::pair<uint64_t, uint64_t> getOpOperand(dwarf_t::op_t op,
            std::istream& is, arch_t arch);
        uint64_t getFrameBase(RegCallback regCallback, MemCallback memCallback, uint64_t pc);
        uint64_t maskBytes(uint64_t value, int numBytes);
        std::pair<uint64_t, uint64_t> getValue(RegCallback regCallback, MemCallback memCallback,
            uint64_t pc, dwarf_t::at_t at);
        std::string getValueString(RegCallback regCallback, MemCallback memCallback, uint64_t pc);
        std::string getNamespace();
      private:
        typedef std::map<std::string, std::pair<DIE*, uint64_t> > ClsMemMap;
        class PrettyPrint;
        class Traverse {
          public:
            class Iterator {
              public:
                Iterator(std::list< std::unique_ptr<DIE> >* l, bool inherit)
                 : list(l), it(l->begin()), nestedIt(nullptr), inherit(inherit) { }
                DIE* value();
                bool done();
                void next();
                ~Iterator();
              private:
                std::list< std::unique_ptr<DIE> >* list;
                std::list< std::unique_ptr<DIE> >::iterator it;
                Iterator * nestedIt;
                bool inherit;
            };
            Traverse(SharedDIEMap dies, arch_t a)
              : dies(dies), arch(a) { }
            void addAttribute(dwarf_t::at_t at, AttrValue value);
            void addChild(std::unique_ptr<DIE> die);
            Iterator getChildIterator(bool inherit);
            std::pair<bool, AttrValue> getAttribute(dwarf_t::at_t at);
            int numChildren();
            std::string toString(bool recurse, int indent);
          private:
            std::string exprlocString(dwarf_t::at_t at);
            std::map<dwarf_t::at_t, AttrValue> attributes;
            std::list< std::unique_ptr<DIE> > children;
            SharedDIEMap dies;
            arch_t arch;
        };
        std::string getValueStringByAddr(RegCallback regCallback, MemCallback memCallback,
            uint64_t addr, std::list<dwarf_t::tag_t> tags, int depth, ClsMemMap* memberMap);
        uint64_t getValueByAddr(RegCallback regCallback, MemCallback memCallback,
            uint64_t addr, bool followPtr);
        uint64_t getNumBytes(bool followPtr);
        std::string tryPrettyPrint(RegCallback regCallback, MemCallback memCallback,
            std::string className, ClsMemMap* members);
        std::string tryGetString(uint64_t addr, MemCallback memCallback);
        std::string tryGetString16(uint64_t addr, MemCallback memCallback);
        std::string tryGetString32(uint64_t addr, MemCallback memCallback);
        std::pair<bool, int> arrayLength();
        bool isPointerType();
        std::string arrayStr();
        uint64_t offset;
        dwarf_t::tag_t tag;
        DIE* parent;
        Traverse traverse;
        SharedDIEMap dies;
        std::shared_ptr<Frames> frames;
        arch_t arch;
        std::string typeStrCache;
        std::shared_ptr<ComponentLogger> logger;
    };

  } /* namespace dwarf */
} /* namespace penguinTrace */

#endif /* DWARF_DIE_H_ */
