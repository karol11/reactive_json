#ifndef REACTIVE_JSON_READER_H
#define REACTIVE_JSON_READER_H

#include <string>
#include <vector>
#include <bitset>
#include <cassert>
#include "gunit.h"

namespace reactive_json
{
	struct reader
	{
		reader(const char* data) { reset(data); }
		void reset(const char* data) {
			pos = reinterpret_cast<const unsigned char*>(data);
			error_pos = nullptr;
			error_text.clear();
			skip_ws();
		}

		bool try_number(double& result) {
			auto moved = pos;
			result = strtod((const char*)pos, (char**)&moved);
			if (pos == moved)
				return false;
			skip_ws();
			if (!*moved || *moved == ',' || *moved == ']' || *moved == '}')
				return (pos = moved), true;
			return false;
		}

		double get_number(double default_val) {
			double r;
			return try_number(r)
				? r
				: default_val;
		}

		bool try_bool(bool& result) {
			if (is("false"))
				return (result = false), true;
			if (is("true") == 0)
				return (result = true), true;
			return false;
		}

		bool get_bool(bool default_val) {
			try_bool(default_val);
			return default_val;
		}

		bool get_null() {
			return is("null");
		}

		bool try_string(std::string& result, size_t max_size = ~0u) {
			return read_string_to_buffer([](size_t size, void* context) {
				auto str = reinterpret_cast<std::string*>(context);
				str->resize(size);
				return &str->operator[](0);
			}, &result, max_size);
		}

		std::string get_string(const char* default_value, size_t max_size = ~0u) {
			std::string r;
			return try_string(r, max_size) ? r : std::string(default_value);
		}

		bool read_string_to_buffer(char* (*allocator)(size_t size, void* context), void* context, size_t max_size) {
			if (*pos != '"') return false;
			size_t size = 0;
			pos++;
			auto p = pos;
			bool has_tail = false;
			auto is_escape = [](unsigned char c) {
				static auto mask = [] {
					std::bitset<128> r;
					for (auto c : "\\\"/bfnrt") r.set(c, true);
					return r;
				}();
				return c < 128 && mask[c];
			};
			while (*pos != '"') {
				if (!*pos) {
					set_error("incomplete string");
					return true;
				} else if (*pos == '\\') {
					auto c = *++pos;
					if (!c) {
						set_error("incomplete escape");
						return true;
					} else if (c == 'u') {
						size_t val = 0;
						if (!get_codepoint(val))
							return true;
						auto codepoint_size = val <= 0x7ff
							? val <= 0x7f ? 1 : 2
							: val <= 0xffff ? 3 : 4;
						if (size + codepoint_size > max_size) {
							has_tail = true;
							break;
						}
						size += codepoint_size;
						continue;
					} else if (!is_escape(c)) {
						set_error("invalid escape");
						return true;
					}
				}
				pos++;
				size++;
				if (size == max_size) {
					has_tail = true;
					break;
				}
			}
			auto dst = allocator(size, context);
			if (!dst) return true;
			auto stop_at = dst + size;
			while (dst != stop_at) {
				if (*p == '\\') {
					switch (*++p) {
					case '\\': *dst = '\\'; break;
					case '"': *dst = '"'; break;
					case '/': *dst = '/'; break;
					case 'b': *dst = '\b'; break;
					case 'f': *dst = '\f'; break;
					case 'n': *dst = '\n'; break;
					case 'r': *dst = '\r'; break;
					case 't': *dst = '\t'; break;
					case 'u':
						put_utf8(get_codepoint_no_check(++p), dst);
						continue;
					default:
						assert(false); // already checked with is_escape
						break;
					}
				} else
					*dst = *p;
				p++;
				dst++;
			}
			if (has_tail)
				skip_string();
			return true;
		}

		template<typename ON_ITEM>
		bool get_array(ON_ITEM on_item) {
			if (!is('[')) return false;
			if (is(']')) return true;
			do {
				auto p = pos;
				on_item();
				if (pos == p)
					skip_value();
			} while (is(','));
			if (!is(']'))
				set_error("expected ',' or ']'");
			return true;
		}

		template<typename ON_FIELD>
		bool get_object(ON_FIELD on_field) {
			if (!is('{')) return false;
			std::string field_name;
			if (auto p = handle_object_start(field_name)) {
				do
					on_field(field_name);
				while (handle_object_cont(field_name, p));
			}
			return true;
		}

		void set_error(std::string text) {
			if (!error_pos) {
				error_pos = pos;
				error_text = std::move(text);
				pos = (const unsigned char*) "";
			}
		}

	private:
		const unsigned char* handle_object_start(std::string& field_name) {
			if (is('}')) return nullptr;
			if (!handle_field_name(field_name))
				return nullptr;
			return pos;
		}

		bool handle_object_cont(std::string& field_name, const unsigned char*& start_pos) {
			if (pos == start_pos)
				skip_value();
			if (is(',')) {
				if (handle_field_name(field_name)) {
					start_pos = pos;
					return true;
				}
			}
			else if (!is('}'))
				set_error("expected ',' or '}'");
			return false;
		}

		bool get_codepoint(size_t val) {
			pos++;
			auto get_utf16 = [&] {
				auto hex = [&] {
					if (*pos >= '0' && *pos <= '9')
						val = val << 4 | (*pos - '0');
					else if (*pos >= 'a' && *pos <= 'f')
						val = val << 4 | (*pos - 'a' + 10);
					else if (*pos >= 'A' && *pos <= 'F')
						val = val << 4 | (*pos - 'A' + 10);
					else {
						set_error("not a hex digit");
						return false;
					}
					pos++;
					return true;
				};
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
				if (*pos != '\\' && pos[1] != 'u') {
					set_error("first surrogare without following \\u");
					return false;
				}
				pos += 2;
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

		size_t get_codepoint_no_check(const unsigned char*& pos) {
			pos++;
			auto get_utf16 = [&] {
				auto hex = [&](char c) {
					return
						c >= '0' && c <= '9' ? c - '0' :
						c >= 'a' && c <= 'f' ? c + 10 - 'a' :
						c + 10 - 'A';
				};
				size_t r = hex(pos[0]) << 24 | hex(pos[1]) << 16 | hex(pos[2]) << 8 | hex(pos[3]);
				pos += 4;
				return r;
			};
			auto r = get_utf16();
			return r < 0xdd00 || r > 0xDFFF
				? r
				: (pos += 2, ((r & 0x3ff) << 10 | (get_utf16() & 0x3ff)) + 0x10000);
		}

		void put_utf8(size_t v, char*& dst)
		{
			if (v <= 0x7f)
				*dst++ = char(v);
			else {
				if (v <= 0x7ff)
					*dst++ = char(v >> 6 | 0xc0);
				else {
					if (v <= 0xffff)
						*dst++ = char(v >> (6 + 6) | 0xe0);
					else {
						*dst++ = char(v >> (6 + 6 + 6) | 0xf0);
						*dst++ = char(((v >> (6 + 6)) & 0x3f) | 0x80);
					}
					*dst++ = char(((v >> 6) & 0x3f) | 0x80);
				}
				*dst++ = char((v & 0x3f) | 0x80);
			}
		}

		void skip_ws() {
			while (*pos <= ' ')
				pos++;
		}

		void skip_string() {
			for (;;) {
				if (!*pos) {
					set_error("incomplete string while skipping");
					break;
				} if (*pos == '\\') {
					if (!*++pos) {
						set_error("incomplete string escape while skipping");
						break;
					}
					++pos;
				} else if (*pos++ == '"') {
					break;
				}
			}
			skip_ws();
		}

		void skip_value() {
			if (*pos == '{')
				skip_until('}');
			else if (*pos == '[')
				skip_until(']');
			else if (*pos == '"')
				skip_string();
			else {
				while (*pos && *pos != ',' && *pos != '}' && *pos != ']') {
					pos++;
				}
			}
		}

		void skip_until(char term) {
			std::vector<char> expects{ term };
			for (;;) {
				char c = *pos++;
				switch (c) {
				case 0:
					set_error(term == '}' ? "incomplete object" : "incomplete array");
					return;
				case'"':
					skip_string();
					break;
				case'[':
				case'{':
					expects.push_back(c);
					break;
				case']':
				case'}':
					if (expects.empty() || expects.back() != c) {
						std::string error = "unpaired }";
						error.back() = c;
						set_error(error);
						return;
					}
					expects.pop_back();
					if (expects.empty()) {
						skip_ws();
						return;
					}
				default: break;
				}
			}
		}

		bool is(char term) {
			if (*pos != term)
				return false;
			pos++;
			skip_ws();
			return true;
		}

		bool is(const char* term) {
			for (auto p = pos;; term++, p++) {
				if (!*term) {
					pos = p;
					skip_ws();
					return true;
				}
				if (*p != *term)
					return false;
			}
		}

		bool handle_field_name(std::string& field_name) {
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

		const unsigned char* pos;
		const unsigned char* error_pos;
		std::string error_text;
	};
}

#endif  // REACTIVE_JSON_READER_H
