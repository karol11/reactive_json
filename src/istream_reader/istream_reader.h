#ifndef REACTIVE_JSON_ISTREAM_READER_H
#define REACTIVE_JSON_ISTREAM_READER_H

#include <istream>
#include <optional>

namespace reactive_json
{
    /// Reads JSON from std::istream.
    struct istream_reader
    {
        istream_reader(std::unique_ptr<std::istream> stream)
        {
            reset(std::move(stream));
        }

        /// Prepares the reader to a new parsing session.
        void reset(std::unique_ptr<std::istream> stream);

        // Checks if passing ended successfully.
        bool success() { return cur == 0 && error_text.empty(); }

        /// Attempts to extract a number from the current position.
        /// If the current position contains a number:
        /// - returns the extracted value
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns `nullopt`.
        /// If the contains ill-formed number, the reader switches to error state.
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
        bool get_null() { return is("null"); }

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
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs, `reader` switches to the error state.
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
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs, `reader` switches to the error state.
        std::optional<std::string> try_string(size_t max_size = ~0u);

        /// Extracts the string from the current position.
        /// Expands the \uXXXX escapes to utf8 encoding. Handles surrogate pairs.
        /// If current position doesn't contain a string, returns the `default_val`.
        /// Always skips the current element.
        /// The returned string is limited to the given `max_value` (the remainder gets skipped).
        /// If the parsed string has errors: unterminated, bad escapes, bad utf16 surrogate pairs, `reader` switches to the error state.
        std::string get_string(const char* default_val, size_t max_size = ~0u);

        /// Attempts to extract an array from the current position.
        /// If current position contains an array:
        /// - returns true,
        /// - calls `on_item` for each array element.
        /// - and advances the position.
        /// Otherwise:
        /// - leaves the current position intact
        /// - returns false.
        /// The `on_array` handler is a `void()` lambda, that is invoked on each array item.
        /// It must call any `reader` methods to extract the array item data.
        /// Example:
        /// reader json("[1,2,3,4]");
        /// std::vector<double> result;
        /// bool it_was_array = json.try_array([&]{
        ///     result.push_back(json.get_number(0));
        /// });
        /// If the array is malformed, the `reader` switches to the error state.
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
            skip_ws_after_value();
            return true;
        }

        /// Extracts an array from the current position.
        /// If the current position contains an array it calls `on_item` for each array element.
        /// Alway skips current json element.
        /// The `on_array` handler is a `void()` lambda, that must call any `reader` methods to extract the array item data.
        /// Example:
        /// reader json("[1,2,3,4]");
        /// std::vector<double> result;
        /// json.get_array([&]{
        ///     result.push_back(json.get_number(0));
        /// });
        /// If the array is malformed, the `reader` switches to the error state.
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
        /// - can use any any `reader` methods to access the field data.
        /// Example:
        /// reader json(R"-( { "x": 1, "y": "hello" } )-");
        /// std::pair<double, std::string> result;
        /// bool it_was_object = json.try_object([&] (auto name){
        ///     if (name == "x") result.first = json.get_number(0);
        ///     else if (name == "y") result.second = json.get_string("");
        /// });
        /// If the object is malformed, the `reader` switches to the error state.
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
        /// - can use any any `reader` methods to access the field data.
        /// Example:
        /// reader json(R"-( { "x": 1, "y": "hello" } )-");
        /// std::pair<double, std::string> result;
        /// json.get_object([&] (auto name){
        ///     if (name == "x") result.first = json.get_number(0);
        ///     else if (name == "y") result.second = json.get_string("");
        /// });
        /// If the object is malformed, the `reader` switches to the error state.
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
        std::streamoff get_error_pos() { error_text.empty() ? std::streampos() : stream->tellg(); }

        // Returns error text both set by `set_error` manually and the internal parsing errors.
        // Returns an empty string if no error.
        const std::string& get_error_message() { return error_text; }

    private:
        std::streamoff handle_object_start(std::string& field_name);
        bool handle_object_cont(std::string& field_name, std::streamoff& start_pos);
        bool get_codepoint(uint32_t& val);
        bool put_utf8(size_t v, std::string& dst, size_t& left);
        void skip_ws();
        void skip_ws_after_value();
        void skip_string();
        void skip_value();
        void skip_until(char term);
        bool is(char term);
        bool is(const char* term);
        bool handle_field_name(std::string& field_name);
        unsigned char istream_reader::getch();

        std::unique_ptr<std::istream> stream;
        unsigned char cur;
        std::string error_text;
    };
}

#endif  // REACTIVE_JSON_ISTREAM_READER_H
