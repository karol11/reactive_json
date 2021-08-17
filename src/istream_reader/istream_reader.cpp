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

#include <vector>
#include <bitset>
#include <cassert>
#include <cfenv>

#include "istream_reader.h"

namespace reactive_json
{
    void istream_reader::reset(std::unique_ptr<std::istream> stream)
    {
        this->stream = std::move(stream);
        error_text.clear();
        getch();
        skip_ws();
    }

    std::optional<double> istream_reader::try_number()
    {
        if (cur != '-' && cur != '.' && (cur < '0' || cur > '9'))
            return std::nullopt;
        std::feclearexcept(FE_ALL_EXCEPT);
        auto get_int = [&] {
            int sign = is('-') ? -1 : (is('+'), 1);
            double r = 0;
            for (; cur >= '0' && cur < '9'; getch())
                r = r * 10 + cur - '0';
            return r * sign;
        };
        double r = get_int();
        if (is('.')) {
            for (double weight = r < 0 ? -0.1 : 0.1; cur >= '0' && cur < '9'; weight *= 0.1, getch())
                r += weight * (cur - '0');            
        }
        if (is('e') || is('E'))
            r *= pow(10, get_int());

        if (std::fetestexcept(FE_OVERFLOW | FE_UNDERFLOW)) {
            set_error("numeric overflow");
            return r;
        }
        skip_ws_after_value();
        return r;
    }

    unsigned char istream_reader::getch() {
        cur = (unsigned char) stream->get();
        return stream->good() ? cur : (cur = 0);
    }

    double istream_reader::get_number(double default_val)
    {
        auto r = try_number();
        return r ? *r : (skip_value(), default_val);
    }

    std::optional<bool> istream_reader::try_bool()
    {
        if (is("false"))
            return false;
        if (is("true"))
            return true;
        return std::nullopt;
    }

    bool istream_reader::get_bool(bool default_val)
    {
        if (is("false"))
            return false;
        if (is("true"))
            return true;
        skip_value();
        return default_val;
    }

    bool istream_reader::try_string(std::string& result, size_t max_size)
    {
        if (cur != '"')
            return false;
        result.clear();
        uint32_t code_point;
        getch();
        for (;;) {
            switch (cur) {
            case 0:
                set_error("incomplete string");
                return true;
            case '"':
                getch();
                skip_ws();
                return true;
            case '\\':
                switch (getch()) {
                case 0:
                    set_error("incomplete escape");
                    return true;
                case '\\': result.push_back('\\'); break;
                case '"': result.push_back('"'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u':
                    code_point = 0;
                    if (!get_codepoint(code_point))
                        return true;
                    if (!put_utf8(code_point, result, max_size)) {
                        skip_string();
                        return true;
                    }
                    continue;
                default:
                    set_error("invalid escape");
                    return true;
                }
                break;
            default:
                result.push_back(cur);
                break;
            }
            getch();
            if (--max_size == 0) {
                skip_string();
                return true;
            }
        }
    }

    std::optional<std::string> istream_reader::try_string(size_t max_size)
    {
        std::string result;
        return try_string(result, max_size)
            ? skip_ws_after_value(), std::optional(std::move(result))
            : std::nullopt;
    }

    std::string istream_reader::get_string(const char* default_val, size_t max_size)
    {
        auto r = try_string(max_size);
        return r
            ? std::move(*r)
            : (skip_value(), std::string(default_val));
    }

    void istream_reader::set_error(std::string text)
    {
        if (error_text.empty()) {
            error_text = std::move(text);
            cur = 0;
        }
    }

    std::streamoff istream_reader::handle_object_start(std::string& field_name)
    {
        if (is('}')) return 0;
        if (!handle_field_name(field_name))
            return 0;
        return stream->tellg();
    }

    bool istream_reader::handle_object_cont(std::string& field_name, std::streamoff& start_pos)
    {
        if (stream->tellg() == start_pos)
            skip_value();
        if (is(',')) {
            if (handle_field_name(field_name)) {
                start_pos = stream->tellg();
                return true;
            }
        }
        else if (!is('}'))
            set_error("expected ',' or '}'");
        skip_ws_after_value();
        return false;
    }

    bool istream_reader::get_codepoint(uint32_t& val)
    {
        auto get_utf16 = [&] {
            auto hex = [&] {
                if (cur == 0) {
                    set_error("incomplete \\uXXXX sequence");
                    return false;
                }
                if (cur >= '0' && cur <= '9')
                    val = (val << 4) | (cur - '0');
                else if (cur >= 'a' && cur <= 'f')
                    val = (val << 4) | (cur - 'a' + 10);
                else if (cur >= 'A' && cur <= 'F')
                    val = (val << 4) | (cur - 'A' + 10);
                else {
                    set_error("not a hex digit");
                    return false;
                }
                getch();
                return true;
            };
            getch();
            return hex() && hex() && hex() && hex();
        };
        if (!get_utf16())
            return false;
        if (val >= 0xd800 && val <= 0xdbff) {
            set_error("second surrogare without first one");
            return false;
        }
        else if (val >= 0xdd00 && val <= 0xDFFF) {
            auto first = val;
            val = 0;
            if (cur != '\\' || getch() != 'u') {
                set_error("first surrogare without following \\u");
                return false;
            }
            if (!get_utf16())
                return false;
            if (!(val >= 0xd800 && val <= 0xdbff)) {
                set_error("first surrogare without second one");
                return false;
            }
            val = ((first & 0x3ff) << 10 | (val & 0x3ff)) + 0x10000;
        }
        return true;
    }

    bool istream_reader::put_utf8(size_t v, std::string& dst, size_t& left)
    {
        if (v <= 0x7f) {
            dst.push_back(char(v));
            return --left > 0;
        }
        if (v <= 0x7ff) {
            if (left < 2)
                return false;
            left -= 2;
            dst.push_back(char(v >> 6 | 0xc0));
        } else {
            if (v <= 0xffff) {
                if (left < 3)
                    return false;
                left -= 3;
                dst.push_back(char(v >> (6 + 6) | 0xe0));
            } else {
                if (left < 4)
                    return false;
                left -= 4;
                dst.push_back(char(v >> (6 + 6 + 6) | 0xf0));
                dst.push_back(char(((v >> (6 + 6)) & 0x3f) | 0x80));
            }
            dst.push_back(char(((v >> 6) & 0x3f) | 0x80));
        }
        dst.push_back(char((v & 0x3f) | 0x80));
    }

    void istream_reader::skip_ws()
    {
        while (cur && cur <= ' ')
            getch();
    }

    void istream_reader::skip_ws_after_value() {
        skip_ws();
        static const auto mask = std::bitset<128>().set(0).set(',').set('}').set(']');
        if (cur > 128 || !mask[cur])
           set_error("unexpected value at the end of value");
    }

    void istream_reader::skip_string()
    {
        for (;; getch()) {
            if (cur == 0) {
                set_error("incomplete string while skipping");
                break;
            } if (cur == '\\') {
                if (!getch()) {
                    set_error("incomplete string escape while skipping");
                    break;
                }
            }
            else if (cur == '"') {
                getch();
                break;
            }
        }
        skip_ws();
    }

    void istream_reader::skip_value()
    {
        if (!cur)
            return;
        if (cur == '{') {
            skip_until('}');
        } else if (cur == '[') {
            skip_until(']');
        } else if (cur == '"') {
            getch();
            skip_string();
        } else {
            static auto mask = [] {
                std::bitset<128> r;
                r.set('-').set('.').set('+');
                for (auto c = 'a'; c <= 'z'; c++)
                    r.set(c);
                for (auto c = '0'; c <= '9'; c++)
                    r.set(c);
                for (auto c = 1; c <= ' '; c++)
                    r.set(c);
                return r;
            }();
            while (cur && cur < 128 && mask[cur]) {
                getch();
            }
        }
    }

    void istream_reader::skip_until(char term)
    {
        getch();
        std::vector<char> expects{ term };
        while (cur) {
            switch (cur) {
            case'"':
                getch();
                skip_string();
                continue;
            case'[':
                expects.push_back(']');
                break;
            case'{':
                expects.push_back('}');
                break;
            case']':
            case'}':
                if (expects.empty() || expects.back() != cur) {
                    std::string error = "mismatched }";
                    error.back() = cur;
                    set_error(error);
                    return;
                }
                expects.pop_back();
                if (expects.empty()) {
                    getch();
                    skip_ws();
                    return;
                }
                break;
            default:
                break;
            }
            getch();
        }
        set_error(term == '}' ? "incomplete object" : "incomplete array");
        return;
    }

    bool istream_reader::is(char term) {
        if (cur != term)
            return false;
        getch();
        skip_ws();
        return true;
    }

    bool istream_reader::is(const char* term) {
        if (cur != *term)
            return false;
        for (;;) {
            getch();
            if (*++term == 0)
                break;
            if (*term != cur) {
                set_error(std::string("expected ") + term);
                return true;
            }
        }
        skip_ws_after_value();
        return true;
    }

    bool istream_reader::handle_field_name(std::string& field_name) {
        if (!try_string(field_name)) {
            set_error("expected field name");
            return false;
        }
        if (!is(':')) {
            set_error("expected ':'");
            return false;
        }
        return true;
    }
}
