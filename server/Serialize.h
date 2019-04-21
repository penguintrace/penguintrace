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
// Basic JSON Serialiser

#ifndef SERVER_SERIALIZE_H_
#define SERVER_SERIALIZE_H_

#include <string>

#include "../penguintrace/Session.h"

namespace penguinTrace
{
  namespace server
  {

    class Serialize
    {
      public:
        static std::unique_ptr<Serialize> stepState(Session& session);
        static std::unique_ptr<Serialize> sessionState(Session &session);
        static std::unique_ptr<Serialize> bkptState(Session &session, bool ok);
        void print(std::ostream &out) const
        {
          out << stream.str();
        }
        virtual ~Serialize ();
      private:
        std::stringstream stream;
        Serialize ();
        void addBool(std::string name, bool value);
        void addString(std::string name, std::string value);
        void addInt(std::string name, uint32_t value);
        template<typename I>
        void addArray(std::string name, I begin, I end, std::function<std::string(typename I::value_type)> f)
        {
          bool first = true;
          I it = begin;
          stream << '"' << name << "\": [";
          for (; it != end; ++it)
          {
            if (!first)
            {
              stream << ",";
            }
            first = false;
            stream << f(*it);
          }
          stream << ']';
        }
        void addBreakpoints(Session& session);
        void addStackTrace(Session& session);
        void addQueue(std::string name, std::queue<std::string>& queue);
        void addMap(std::string name, std::map<std::string, uint64_t> values);
        void addMap(std::string name, std::map<std::string, std::string> values);
        void addMap(std::string name, std::string propName, std::map<uint64_t, std::string> values);
        template<typename T>
        void addMap(std::string name, std::string propName,
                    std::map<uint64_t, T> values, std::function<std::string(T)> f)
        {
          addArray(name, values.begin(), values.end(), [&](std::pair<uint64_t, T> v) {
            std::stringstream s;
            s << "{\"pc\": ";
            s << v.first;
            s << ",\"" << propName << "\": \"" << jsonEscape(f(v.second));
            s << "\"}";
            return s.str();
          });
        }
        template<typename T>
        void addQueue(std::string name, std::queue<T>& queue, std::function<std::string(T)> f)
        {
          stream << '"' << name << "\": [";
          bool first = true;
          while (!queue.empty())
          {
            if (!first)
            {
              stream << ",";
            }
            first = false;
            stream << f(queue.front());
            queue.pop();
          }
          stream << "]";
        }
    };

    std::ostream &operator<<(std::ostream &out, const Serialize &obj);

  } /* namespace server */
} /* namespace penguinTrace */

#endif /* SERVER_SERIALIZE_H_ */
