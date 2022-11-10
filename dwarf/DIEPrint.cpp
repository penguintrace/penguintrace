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
// DWARF Debug Information Entries - Pretty Printers

#include "DIE.h"

#include <sstream>

#include "../common/MemoryBuffer.h"
#include "../common/StreamOperations.h"
#include "../common/Config.h"

namespace penguinTrace
{
  namespace dwarf
  {

    class DIE::PrettyPrint {
      public:
        typedef std::function<std::string(RegCallback, MemCallback, ClsMemMap*)> PrettyPrinter;
        static PrettyPrinter getPrinter(std::string className, std::unique_ptr<ComponentLogger> l);
      private:
        static const int MaxLoops = 256;
        static std::string stringPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);
        static std::string vectorPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);
        static std::string listPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);
        static std::string mapPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);
        static std::string hashMapPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);
        static std::string pairPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);
        static std::string debugPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem);

        static uint64_t nextMapNodeAddr(uint64_t node, uint64_t l, uint64_t r, uint64_t p, MemCallback mCall);
    };

    DIE::PrettyPrint::PrettyPrinter DIE::PrettyPrint::getPrinter(std::string className, std::unique_ptr<ComponentLogger> l)
    {
      l->log(Logger::TRACE, [&]() {
        std::stringstream s;
        s << "Finding pretty printer for '" << className << "'";
        return s.str();
      });
      if (className.find("basic_string<") != std::string::npos)
      {
        l->log(Logger::TRACE, "Using stringPrinter");
        return &stringPrinter;
      }
      if (className.find("list<") != std::string::npos)
      {
        l->log(Logger::TRACE, "Using listPrinter");
        return &listPrinter;
      }
      if (className.find("vector<") != std::string::npos)
      {
        // Bool specialisation needs different printer
        if (className.find("vector<bool") == std::string::npos)
        {
          l->log(Logger::TRACE, "Using vectorPrinter");
          return &vectorPrinter;
        }
      }
      if (className.find("map<") != std::string::npos)
      {
        if (className.find("unordered_map<") != std::string::npos)
        {
          l->log(Logger::TRACE, "Using hashMapPrinter");
          return &hashMapPrinter;
        }
        else
        {
          l->log(Logger::TRACE, "Using mapPrinter");
          return &mapPrinter;
        }
      }
      if (className.find("pair<") != std::string::npos)
      {
        l->log(Logger::TRACE, "Using pairPrinter");
        return &pairPrinter;
      }
      return nullptr;
    }

    std::string DIE::tryPrettyPrint(RegCallback regCallback, MemCallback memCallback,
        std::string className, ClsMemMap* members)
    {
      auto printer = PrettyPrint::getPrinter(className, logger->subLogger("Print"));

      if (printer)
      {
        return printer(regCallback, memCallback, members);
      }
      else
      {
        return "";
      }
    }

    std::string DIE::PrettyPrint::stringPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;
      uint64_t startAddr = 0;
      uint64_t stringLen = 0;
      uint64_t addr = 0;
      DIE* charType = nullptr;

      auto stringPtrIt = mem->find("_M_p");
      auto stringLenIt = mem->find("_M_string_length");

      if ((stringPtrIt == mem->end()) || (stringLenIt == mem->end()))
      {
        return "";
      }

      startAddr = stringPtrIt->second.first->getValueByAddr(rCall, mCall, stringPtrIt->second.second, false);
      stringLen = stringLenIt->second.first->getValueByAddr(rCall, mCall, stringLenIt->second.second, false);
      addr = startAddr;
      charType = stringPtrIt->second.first->getType();

      if (stringLen > Config::get(C_CSTR_MAX_CHAR).Int())
      {
        stringLen = Config::get(C_CSTR_MAX_CHAR).Int();
      }

      s << '"';
      for (int i = 0; i < stringLen; i++)
      {
        std::list<dwarf_t::tag_t> tags;
        s << charType->getValueStringByAddr(rCall, mCall, addr, tags, -1, nullptr);
        addr += charType->getNumBytes(false);
      }
      s << '"';
      return s.str();
    }

    std::string DIE::PrettyPrint::vectorPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;
      uint64_t startAddr;
      uint64_t endAddr;
      uint64_t sEndAddr;
      DIE* elemType = nullptr;

      auto startIt = mem->find("_M_start");
      auto endIt = mem->find("_M_finish");
      auto sEndIt = mem->find("_M_end_of_storage");

      if ((startIt == mem->end()) || (endIt == mem->end()) || (sEndIt == mem->end()))
      {
        return "";
      }

      startAddr = startIt->second.first->getValueByAddr(rCall, mCall, startIt->second.second, false);
      endAddr = endIt->second.first->getValueByAddr(rCall, mCall, endIt->second.second, false);
      sEndAddr = endIt->second.first->getValueByAddr(rCall, mCall, sEndIt->second.second, false);
      elemType = startIt->second.first->getType();

      if ((startAddr == 0) || (startAddr == -1)) return "";
      if ((endAddr == 0) || (endAddr == -1)) return "";
      if ((sEndAddr == 0) || (sEndAddr == -1)) return "";
      if (startAddr >= endAddr) return "";
      if (startAddr >= sEndAddr) return "";
      if (endAddr >= sEndAddr) return "";

      s << '[';
      int count = 0;
      for (uint64_t addr = startAddr; addr < endAddr ; addr += elemType->getNumBytes(false))
      {
        if (addr != startAddr) s << ", ";
        std::list<dwarf_t::tag_t> tags;
        s << elemType->getValueStringByAddr(rCall, mCall, addr, tags, -1, nullptr);
        count++;
        if (count > MaxLoops) break;
      }
      s << ']';

      return s.str();
    }

    std::string DIE::PrettyPrint::listPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;

      auto nodeIt = mem->find("_M_node");
      auto typeIt = mem->find("@value_type");

      if ((nodeIt == mem->end()) || (typeIt == mem->end()))
      {
        return "";
      }

      //DIE* nodeType = nodeIt->second.first;
      DIE* next = nullptr;
      DIE* store = nullptr;
      uint64_t nextOffset = 0;
      uint64_t storeOffset = 0;

      for (auto it = nodeIt->second.first->traverse.getChildIterator(true); !it.done(); it.next())
      {
        DIE* d = it.value();
        if (d->hasName() && d->getName() == "_M_next")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            nextOffset = locIt.second.getInt();
            next = d;
          }
        }
        if (d->hasName() && d->getName() == "_M_storage")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            storeOffset = locIt.second.getInt();
            store = d;
          }
        }
      }

      if ((next == nullptr) || (store == nullptr)) return "";

      // Addr of first node
      uint64_t firstNode = nodeIt->second.second;
      // Addr of current node
      uint64_t curNode = mCall(firstNode+nextOffset);
      uint64_t nextNode = mCall(curNode+nextOffset);
      // Before constructing, nodes can be invalid addresses
      if (nextNode == 0 || nextNode == -1) return "";
      s << "[";
      bool first = true;
      int count = 0;
      while (curNode != firstNode)
      {
        if (first) first = false;
        else       s << ", ";
        std::list<dwarf_t::tag_t> blankTags;
        s << typeIt->second.first->getValueStringByAddr(rCall, mCall, curNode+storeOffset, blankTags, 0, nullptr);
        curNode = nextNode;
        nextNode = mCall(nextNode+nextOffset);
        // Just in case badly formed
        if (nextNode == 0 || nextNode == -1) return "";
        count++;
        if (count > MaxLoops) break;
      }
      s << "]";

      return s.str();
    }

    std::string DIE::PrettyPrint::mapPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;

      auto headerIt = mem->find("_M_header");
      auto valTypeIt = mem->find("@value_type");
      auto nodeTypeIt = mem->find("@_Link_type");

      if ((headerIt == mem->end()) ||
          (valTypeIt == mem->end()) ||
          (nodeTypeIt == mem->end())) return "";

      DIE* left = nullptr;
      DIE* right = nullptr;
      DIE* parent = nullptr;
      uint64_t leftOffset = 0;
      uint64_t rightOffset = 0;
      uint64_t parentOffset = 0;
      uint64_t storeOffset = 0;

      // TODO common pattern, move to function
      for (auto it = headerIt->second.first->traverse.getChildIterator(true); !it.done(); it.next())
      {
        DIE* d = it.value();
        if (d->hasName() && d->getName() == "_M_left")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            leftOffset = locIt.second.getInt();
            left = d;
          }
        }
        if (d->hasName() && d->getName() == "_M_right")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            rightOffset = locIt.second.getInt();
            right = d;
          }
        }
        if (d->hasName() && d->getName() == "_M_parent")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            parentOffset = locIt.second.getInt();
            parent = d;
          }
        }
      }

      if ((left == nullptr) || (right == nullptr) || (parent == nullptr)) return "";

      DIE* nodeDie = nodeTypeIt->second.first->getType()->getType();
      for (auto it = nodeDie->traverse.getChildIterator(true); !it.done(); it.next())
      {
        DIE* d = it.value();
        if (d->hasName() && d->getName() == "_M_storage")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            storeOffset = locIt.second.getInt();
          }
        }
      }

      // Address of header
      uint64_t endAddr = headerIt->second.second+parentOffset;
      // Pointer to first node
      //uint64_t startAddr = mCall(headerIt->second.second+leftOffset);
      uint64_t startAddr = nextMapNodeAddr(headerIt->second.second, leftOffset,
          rightOffset, parentOffset, mCall);
      if (startAddr == 0 || startAddr == -1) return "";

      uint64_t currNodeAddr = startAddr;
      uint64_t nextNodeAddr = nextMapNodeAddr(currNodeAddr, leftOffset, rightOffset, parentOffset, mCall);
      if (nextNodeAddr == 0 || nextNodeAddr == -1) return "";

      s << "{ ";
      bool first = true;
      int count = 0;
      while (currNodeAddr != endAddr)
      {
        if (first) first = false;
        else       s << ", ";
        std::list<dwarf_t::tag_t> blankTags;
        s << valTypeIt->second.first->getValueStringByAddr(rCall, mCall, currNodeAddr+storeOffset, blankTags, 0, nullptr);
        currNodeAddr = nextNodeAddr;
        nextNodeAddr = nextMapNodeAddr(currNodeAddr, leftOffset, rightOffset, parentOffset, mCall);
        if (nextNodeAddr == 0 || nextNodeAddr == -1) return "";
        count++;
        if (count > MaxLoops) break;
      }
      s << " }";

      return s.str();
    }

    uint64_t DIE::PrettyPrint::nextMapNodeAddr(uint64_t node, uint64_t l, uint64_t r, uint64_t p, MemCallback mCall)
    {
      uint64_t xptr = node;
      uint64_t right = mCall(xptr+r);
      if (right == -1) return 0;
      uint64_t left = 0;
      uint64_t parent = 0;

      if (right != 0)
      {
        xptr = right;
        left = mCall(xptr+l);
        if (left == -1) return 0;
        int count = 0;
        while (left != 0)
        {
          xptr = left;
          left = mCall(xptr+l);
          if (left == -1) return 0;
          count++;
          if (count > MaxLoops) break;
        }
      }
      else
      {
        parent = mCall(xptr+p);
        if (parent == -1) return 0;
        uint64_t yptr = parent;
        right = mCall(yptr+r);
        if (right == -1) return 0;
        int count = 0;
        while (xptr == right)
        {
          xptr = yptr;
          parent = mCall(yptr+p);
          if (parent == -1) return 0;
          yptr = parent;
          right = mCall(yptr+r);
          if (right == -1) return 0;
          count++;
          if (count > MaxLoops) break;
        }
        right = mCall(xptr+r);
        if (right == -1) return 0;
        if (right != yptr)
        {
          xptr = yptr;
        }
      }
      return xptr;
    }

    std::string DIE::PrettyPrint::pairPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;

      auto firstIt = mem->find("first");
      auto secondIt = mem->find("second");

      if ((firstIt == mem->end()) || (secondIt == mem->end())) return "";

      std::list<dwarf_t::tag_t> blankTags;
      s << firstIt->second.first->getValueStringByAddr(rCall, mCall, firstIt->second.second, blankTags, 0, nullptr);
      s << ": ";
      s << secondIt->second.first->getValueStringByAddr(rCall, mCall, secondIt->second.second, blankTags, 0, nullptr);
      return s.str();
    }

    std::string DIE::PrettyPrint::hashMapPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;

      auto beforeBeginIt = mem->find("_M_before_begin");
      auto valTypeIt = mem->find("@value_type");
      auto hashIt = mem->find("@_Hashtable");

      if ((beforeBeginIt == mem->end()) ||
          (valTypeIt == mem->end()) ||
          (hashIt == mem->end()))
      {
        return "";
      }

      DIE* nxt = nullptr;
      DIE* store = nullptr;
      uint64_t nxtOffset = 0;
      uint64_t storeOffset = 0;

      for (auto it = beforeBeginIt->second.first->traverse.getChildIterator(true); !it.done(); it.next())
      {
        DIE* d = it.value();
        if (d->hasName() && d->getName() == "_M_nxt")
        {
          auto locIt = d->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
          if (locIt.first)
          {
            nxtOffset = locIt.second.getInt();
            nxt = d;
          }
        }
      }

      DIE* hash = hashIt->second.first;
      while (hash->tag == dwarf_t::DW_TAG_typedef)
      {
        hash = hash->getType();
      }

      for (auto it = hash->traverse.getChildIterator(false); !it.done(); it.next())
      {
        DIE * d = it.value();
        if (d->hasName() && d->getName() == "__node_type")
        {
          while (d->tag == dwarf_t::DW_TAG_typedef)
          {
            d = d->getType();
          }
          for (auto it2 = d->traverse.getChildIterator(true); !it2.done(); it2.next())
          {
            DIE* d2 = it2.value();
            if (d2->hasName() && d2->getName() == "_M_storage")
            {
              auto locIt2 = d2->traverse.getAttribute(dwarf_t::DW_AT_data_member_location);
              if (locIt2.first)
              {
                storeOffset = locIt2.second.getInt();
                store = d2;
              }
            }
          }
        }
      }

      if ((nxt == nullptr) || (store == nullptr)) return "";

      uint64_t nodeAddr = mCall(beforeBeginIt->second.second+nxtOffset);

      int count = 0;
      bool first = true;
      s << "{ ";
      while (!((nodeAddr == 0) || (nodeAddr == -1)))
      {
        if (first) first = false;
        else       s << ", ";
        std::list<dwarf_t::tag_t> blankTags;
        s << valTypeIt->second.first->getValueStringByAddr(rCall, mCall, nodeAddr+storeOffset, blankTags, 0, nullptr);
        nodeAddr = mCall(nodeAddr+nxtOffset);
        count++;
        if (count > MaxLoops) break;
      }
      s << " }";

      return s.str();
    }

    std::string DIE::PrettyPrint::debugPrinter(RegCallback rCall, MemCallback mCall, ClsMemMap* mem)
    {
      std::stringstream s;
      for (auto it = mem->begin(); it != mem->end(); ++it)
      {
        if ((it->first.length() == 0) || (it->first.at(0) == '@')) continue;
        s << "-- " << it->first << " = ";
        std::list<dwarf_t::tag_t> blankTags;
        s << it->second.first->getValueStringByAddr(rCall, mCall, it->second.second, blankTags, 0, nullptr);
        if (it->second.first->isPointerType())
        {
          s << " <" << HexPrint(it->second.second, 1) << '>';
        }
        s << std::endl;
      }
      s << std::endl;

      return s.str();
    }

  } /* namespace dwarf */
} /* namespace penguinTrace */
