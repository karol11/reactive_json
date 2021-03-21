#ifndef REACTIVE_JSON_READER_H
#define REACTIVE_JSON_READER_H

#include <string>

namespace reactive_json
{
    struct reader
    {
        reader(const char* data, size_t length = 0)
        {
            reset(data, length);
        }

        /// Prepares the reader to a new parsing session.
        void reset(const char* data, size_t length = 0);

        // Checks if passing ended successfully.
        bool success() {
            return pos == end && !error_pos;
        }

        /// Attempts to extract the number from current position.
        /// On success
        /// - returns true,
        /// - fills `result` with the extracted value
        /// - and advances the stream position.
        /// On failure
        /// - leaves the `result` and the current position intact
        /// - returns false.
        bool try_number(double& result);

        /// Extracts the number from current position.
        /// On failure returns the `default_val`.
        /// Always skips the current element.
        double get_number(double default_val);

        /// Attempts to extract the boolean value from the current position.
        /// On success
        /// - returns true,
        /// - fills `result` with the extracted value
        /// - and advances the stream position.
        /// On failure
        /// - leaves the `result` and the current position intact
        /// - returns false.
        bool try_bool(bool& result);

        /// Extracts the boolean value from the current position.
        /// On failure returns the `default_val`.
        /// Always skips the current element.
        bool get_bool(bool default_val);

        /// Checks if current value contains `null`.
        /// If it does, skips it and returns true.
        /// Otherwise returns false and leave the position intact, allowing to `try_*` or `get_*` the real data.
        bool get_null()
        {
            return is("null");
        }

        /// Attempts to extract the string from the current position.
        /// On success
        /// - returns true,
        /// - fills `result` with the extracted value
        /// - and advances the stream position.
        /// On failure
        /// - leaves the `result` and the current position intact
        /// - returns false.
        /// The `max_size` parameter defines the maxinum amount of bytes to be extracted (the remainder gets skipped).
        bool try_string(std::string& result, size_t max_size = ~0u);

        /// Extracts the string from the current position.
        /// On failure returns the `default_val`.
        /// Always skips the current element.
        /// The returned string is limited to the given `max_value` (the remainder gets skipped).
        std::string get_string(const char* default_val, size_t max_size = ~0u);

        /// Attempts to extract the string from the current position to the arbitrary application-defined data structure.
        /// Expands the \uXXXX escapes to utf8 encoding. Handles surrogate pairs.
        /// On success
        /// - returns true,
        /// - calls the allocatos with given `context` and calculated real size in bytes capped with fills `result`,
        /// - allocator should returns the `char*` pointer to the application data buffer.
        /// - if allocator returns null, string is skipped, otherwise it is filled with text data.
        /// - and advances the stream position past the string.
        /// If the string is larger than `max_size` only `max_size` are returned, but all string will be skipped.
        /// Reader doesn't return the partial utf8 runes made from the `\uXXXX` escapes,
        /// thus the resulting string size might be smaller than the `max_size` by 1..4 bytes.
        /// On failure
        /// - doesn't call the `allocator`,
        /// - leaves the current position intact,
        /// - returns false.
        bool read_string_to_buffer(char* (*allocator)(size_t size, void* context), void* context, size_t max_size = ~0u);

        /// Attempts to extract an array from the current position.
        /// On success
        /// - returns true,
        /// - calls `on_item` for each array element.
        /// - and advances the stream position.
        /// On failure
        /// - leaves the `result` and the current position intact
        /// - returns false.
        /// The `on_array` handler is a `void()` lambda, that must call any `reader` methods
        /// to extract the array item data.
        /// Example:
        /// reader json("[1,2,3,4]");
        /// std::vector<double> result;
        /// bool it_was_array = json.try_array([&]{
        ///     result.push_back(json.get_number(0));
        /// });
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
        /// On success calls `on_item` for each array element.
        /// Skips current json element.
        /// The `on_array` handler is a `void()` lambda, that must call any `reader` methods to extract the array item data.
        /// Example:
        /// reader json("[1,2,3,4]");
        /// std::vector<double> result;
        /// json.get_array([&]{
        ///     result.push_back(json.get_number(0));
        /// });
        template<typename ON_ITEM>
        void get_array(ON_ITEM on_item)
        {
            if (!try_array(std::move(on_item)))
                skip_value();
        }

        /// Attempts to extract an object from the current position.
        /// On success
        /// - returns true,
        /// - calls `on_field` for each field.
        /// - and advances the stream position.
        /// On failure
        /// - leaves the `result` and the current position intact
        /// - returns false.
        /// The `on_field` handler is a `void(std::string field_name)` lambda, that:
        /// - reseives the field name as a string,
        /// - can use any any `reader` methods to access the field data.
        /// Example:
        /// reader json(R"-( { "x": 1, "y": "hello" } )-");
        /// std::pair<double, std::string> result;
        /// bool it_was_object = json.try_object([&] (auto name){
        ///     if (name == "x") result.first = json.get_number(0);
        ///     else if (name == "y") result.second = json.get_string("");
        /// });
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
        /// On success calls `on_field` for each field.
        /// Skips current json element.
        /// The `on_field` handler is a `void(std::string field_name)` lambda, that:
        /// - reseives the field name as a string,
        /// - can use any any `reader` methods to access the field data.
        /// Example:
        /// reader json(R"-( { "x": 1, "y": "hello" } )-");
        /// std::pair<double, std::string> result;
        /// json.get_object([&] (auto name){
        ///     if (name == "x") result.first = json.get_number(0);
        ///     else if (name == "y") result.second = json.get_string("");
        /// });
        template<typename ON_FIELD>
        void get_object(ON_FIELD on_field)
        {
            if (!try_object(std::move(on_field)))
                skip_value();
        }

        /// Sets error state, can be called from any `on_field` / `on_item` handlers, to terminate parsing.
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

#endif  // REACTIVE_JSON_READER_H
