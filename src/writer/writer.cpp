/*
Copyright 2021 Google LLC
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    https://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "writer.h"

namespace reactive_json
{

    writer::writer(std::unique_ptr<std::ostream> sink)
        : holder(std::move(sink))
        , sink(*holder)
    {}

    writer::writer(std::ostream& sink)
        : sink(sink)
    {}

    void writer::operator() (double val)
    {
        sink << val;
    }

    void writer::operator() (bool val)
    {
        sink << (val ? "true" : "false");
    }

    void writer::operator() (nullptr_t)
    {
        sink << "null";
    }

    void writer::operator() (const char* val)
    {
        sink << '"';
        for (; *val; val++)
            write_escaped_char(*val);
        sink << '"';
    }

    void writer::operator() (std::string_view val)
    {
        sink << '"';
        for (auto c : val)
            write_escaped_char(c);
        sink << '"';
    }

    char writer::hex(char c)
    {
        c &= 0xf;
        return c < 10 ? c + '0' : c - 10 + 'a';
    }

    void writer::write_escaped_char(unsigned char c)
    {
        switch (c) {
        case '"': sink << "\\\""; break;
        case '\\': sink << "\\\\"; break;
        case '\r': sink << "\\r"; break;
        case '\n': sink << "\\n"; break;
        case '\t': sink << "\\t"; break;
        case '\b': sink << "\\b"; break;
        case '\f': sink << "\\f"; break;
        default:
            if (c < ' ')
                sink << "\\u00" << hex(c >> 4) << hex(c);
            else
                sink << c;
            break;
        }
    }

    void writer::field_stream::add_field_name(const char* field_name)
    {
        if (is_first)
            is_first = false;
        else
            writer.sink << ',';
        writer(field_name);
        writer.sink << ':';
    }

}
