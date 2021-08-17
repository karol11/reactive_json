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

#ifndef REACTIVE_JSON_MEMORY_BLOCK_READER_H
#define REACTIVE_JSON_MEMORY_BLOCK_READER_H

#include <string>
#include <optional>

namespace reactive_json
{
    /// Reads JSON from preallocated fixed buffer containing the whole JSON image.
    struct memory_block_reader
    {
        memory_block_reader(const char* data, size_t length = 0)
        {
            reset(data, length);
        }

        /// Prepares the memory_block_reader to a new parsing session.
        void reset(const char* data, size_t length = 0);

        // Checks if passing ended successfully.
        bool success() {
            return pos == end && !error_pos;
        }

        /// Attempts to extract a number from the current position.
        /// If the current position contains a number:
        /// - returns the extracted value
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns `nullopt`.
        /// If the contains ill-formed number, the memory_block_reader switches to error state.
        std::optional<double> try_number();

        /// Extracts a number from the current position.
        /// On failure returns the `default_val`.
        /// Always skips the current element.
        double get_number(double default_val);

        /// Attempts to extract a boolean value from the current position.
        /// If the current position contains `true` or `false`:
        /// - returns the extracted value
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns nullopt.
        std::optional<bool> try_bool();

        /// Extracts a boolean value from the current position.
        /// On failure returns the `default_val`.
        /// Always skips the current element.
        bool get_bool(bool default_val);

        /// Checks if the current position contains `null`.
        /// If it does, skips it and returns true.
        /// Otherwise returns false and leave the position intact, allowing to `try_*` or `get_*` the real data.
        bool get_null()
        {
            return is("null");
        }

        /// Attempts to extract the string from the current position.
        /// Expands the \uXXXX escapes to utf8 encoding. Handles surrogate pairs.
        /// If current position contains a string:
        /// - returns true,
        /// - fills `result` with the extracted value
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the `result` and the current position intact
        /// - returns false.
        /// The `max_size` parameter defines the maxinum amount of bytes to be extracted (the remainder gets skipped).
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs, `memory_block_reader` switches to the error state.
        bool try_string(std::string& result, size_t max_size = ~0u);

        /// Attempts to extract the string from the current position.
        /// Expands the \uXXXX escapes to utf8 encoding. Handles surrogate pairs.
        /// If current position contains a string:
        /// - returns the extracted value
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns nullopt.
        /// The `max_size` parameter defines the maxinum amount of bytes to be extracted (the remainder gets skipped).
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs, `memory_block_reader` switches to the error state.
        std::optional<std::string> try_string(size_t max_size = ~0u);

        /// Extracts the string from the current position.
        /// Expands the \uXXXX escapes to utf8 encoding. Handles surrogate pairs.
        /// If current position doesn't contain a string, returns the `default_val`.
        /// Always skips the current element.
        /// The returned string is limited to the given `max_value` (the remainder gets skipped).
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs, `memory_block_reader` switches to the error state.
        std::string get_string(const char* default_val, size_t max_size = ~0u);

        /// Attempts to extract the string from the current position to the arbitrary application-defined data structure.
        /// Expands the \uXXXX escapes to utf8 encoding. Handles surrogate pairs.
        /// If current position contains a string:
        /// - returns true,
        /// - calls the allocator with given `context` and calculated real size in bytes capped with `max_size`
        /// - allocator should return the `char*` pointer to the application data buffer.
        /// - if allocator returns null, string is skipped, otherwise it is filled with the text data.
        /// - advances the position past the string.
        /// If the string is larger than `max_size` only `max_size` are returned, but all string will be skipped.
        /// Reader doesn't return the partial utf8 runes made from the `\uXXXX` escapes,
        /// thus the resulting string size might be smaller than the `max_size` by 1..4 bytes.
        /// If current position doesn't contain a string:
        /// - doesn't call the `allocator`,
        /// - leaves the current position intact,
        /// - returns false.
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs,
        /// the `memory_block_reader` switches to the error state and never calls the `allocator`.
        bool read_string_to_buffer(char* (*allocator)(size_t size, void* context), void* context, size_t max_size = ~0u);

        /// Attempts to extract an array from the current position.
        /// If current position contains an array:
        /// - returns true,
        /// - calls `on_item` for each array element.
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns false.
        /// The `on_array` handler is a `void()` lambda, that is invoked on each array item.
        /// It must call any `memory_block_reader` methods to extract the array item data.
        /// Example:
        /// memory_block_reader json("[1,2,3,4]");
        /// std::vector<double> result;
        /// bool it_was_array = json.try_array([&]{
        ///     result.push_back(json.get_number(0));
        /// });
        /// If the array is malformed, the `memory_block_reader` switches to the error state.
        template<typename ON_ITEM>
        bool try_array(ON_ITEM on_item)
        {
            if (!is('['))
                return false;
            if (is(']'))
                return true;
            do
                on_item();
            while (is(','));
            if (!is(']'))
                set_error("expected ',' or ']'");
            return true;
        }

        /// Extracts an array from the current position.
        /// If the current position contains an array it calls `on_item` for each array element.
        /// Alway skips current json element.
        /// The `on_array` handler is a `void()` lambda, that must call any `memory_block_reader` methods to extract the array item data.
        /// Example:
        /// memory_block_reader json("[1,2,3,4]");
        /// std::vector<double> result;
        /// json.get_array([&]{
        ///     result.push_back(json.get_number(0));
        /// });
        /// If the array is malformed, the `memory_block_reader` switches to the error state.
        template<typename ON_ITEM>
        void get_array(ON_ITEM on_item)
        {
            if (!try_array(std::move(on_item)))
                skip_value();
        }

        /// Attempts to extract an object from the current position.
        /// If the current position contains an object:
        /// - returns true,
        /// - calls `on_field` for each field.
        /// - advances the position past the object.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns false.
        /// The `on_field` handler is a `void(std::string field_name)` lambda, that:
        /// - receives the field name as a string,
        /// - can use any any `memory_block_reader` methods to access the field data.
        /// Example:
        /// memory_block_reader json(R"-( { "x": 1, "y": "hello" } )-");
        /// std::pair<double, std::string> result;
        /// bool it_was_object = json.try_object([&] (auto name){
        ///     if (name == "x") result.first = json.get_number(0);
        ///     else if (name == "y") result.second = json.get_string("");
        /// });
        /// If the object is malformed, the `memory_block_reader` switches to the error state.
        template<typename ON_FIELD>
        bool try_object(ON_FIELD on_field)
        {
            if (!is('{'))
                return false;
            std::string field_name;
            if (auto p = handle_object_start(field_name))
            {
                do
                    on_field(std::move(field_name));
                while (handle_object_cont(field_name, p));
            }
            return true;
        }

        /// Extracts an object from the current position.
        /// If the current position contains an object, calls `on_field` for each field.
        /// Skips current json element.
        /// The `on_field` handler is a `void(std::string field_name)` lambda, that:
        /// - reseives the field name as a string,
        /// - can use any any `memory_block_reader` methods to access the field data.
        /// Example:
        /// memory_block_reader json(R"-( { "x": 1, "y": "hello" } )-");
        /// std::pair<double, std::string> result;
        /// json.get_object([&] (auto name){
        ///     if (name == "x") result.first = json.get_number(0);
        ///     else if (name == "y") result.second = json.get_string("");
        /// });
        /// If the object is malformed, the `memory_block_reader` switches to the error state.
        template<typename ON_FIELD>
        void get_object(ON_FIELD on_field)
        {
            if (!try_object(std::move(on_field)))
                skip_value();
        }

        /// Sets error state.
        /// It can be called from any `on_field` / `on_item` handlers, to terminate parsing.
        /// In the error state, the `parser` responds nullopt/false to all calls, quits all `get/try_object/array` aggregated calls.
        void set_error(std::string text);

        // Returns error position in he parsed json or nullptr is there is no error.
        const char* get_error_pos() { return (const char*)error_pos; }

        // Returns error text both set by `set_error` manually and the internal parsing errors.
        // Returns an empty string if no error.
        const std::string& get_error_message() { return error_text; }

    private:
        const unsigned char* handle_object_start(std::string& field_name);
        bool handle_object_cont(std::string& field_name, const unsigned char*& start_pos);
        bool get_codepoint(size_t& val);
        size_t get_codepoint_no_check(const unsigned char*& pos);
        void put_utf8(size_t v, char*& dst);
        void skip_ws();
        void skip_string();
        void skip_value();
        void skip_until(char term);
        bool is(char term);
        bool is(const char* term);
        bool handle_field_name(std::string& field_name);

        const unsigned char* pos;
        const unsigned char* end;
        const unsigned char* error_pos;
        std::string error_text;
    };
}

#endif  // REACTIVE_JSON_MEMORY_BLOCK_READER_H
