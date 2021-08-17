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

#ifndef REACTIVE_JSON_JWRITER_H
#define REACTIVE_JSON_JWRITER_H

#include <ostream>
#include <memory>
#include <optional>

namespace reactive_json
{

    /// Outputs the JSON to the underlying std::ostream.
    class writer
    {
        friend class field_stream;
    public:
        /// Constructs writer that owns the underlined stream.
        writer(std::unique_ptr<std::ostream> sink);

        /// Constructs writer that borrows the underlined stream.
        writer(std::ostream& sink);

        /// Outputs single scalar numeric value.
        void operator() (double val);

        /// Outputs single scalar boolean value.
        void operator() (bool val);

        /// Outputs single scalar null value.
        void operator() (nullptr_t);

        /// Outputs single scalar string value.
        void operator() (const char* val);

        /// Outputs single scalar string value. This one can contain \u0000 characters.
        void operator() (std::string_view val);

        /// Outputs an array of items.
        /// `size` defines the array size.
        /// `on_item` Is a lambda to be called for each array item.
        ///           It receives this writer `instance` and the array item `index` from 0 to size - 1.
        ///           It should output array item using operator(), write_array or write_object calls.
        /// Example: 
        /// void write_vector(const vector<double>& vec) {
        ///     writer(std::cout).write_array(vec.size(), [&](auto& writer, size_t index){
        ///         writer(vec[index]);
        ///     });
        /// }
        template<typename ON_ITEM>
        void write_array(size_t size, ON_ITEM&& on_item)
        {
            sink << '[';
            if (size != 0) {
                on_item(*this, 0);
                for (size_t i = 0; ++i != size;) {
                    sink << ',';
                    on_item(*this, i);
                }
            }
            sink << ']';
        }

        /// Outputs an object with fields.
        /// `field_maker` - Is a lambda that is invoked on time to write object fields,
        ///                 It receives a special `field_writer` instance,
        ///                 that replicates the same methods as `writer` but extended with `field_name` parameter.
        /// Example:
        /// struct my_struct{ int x; char* str;};
        /// void write_my_struct(const my_struct& data) {
        ///    writer(std::cout).write_object([&](auto fields) {
        ///       fields
        ///         ("x", (double) s.x)
        ///         ("str", s.str);
        ///    });
        /// }
        template<typename FIELD_MAKER>
        void write_object(FIELD_MAKER&& field_maker)
        {
            sink << '{';
            field_maker(field_stream{ *this });
            sink << '}';
        }

        /// Object that internally created by `write_object` and passed to `field_maker` lambda.
        class field_stream
        {
            friend class writer;
        public:
            /// Outputs field name and returns item writer to store value.
            writer& write_field(const char* field_name) {
                add_field_name(field_name);
                return writer;
            }

            /// Outputs numeric/boolean/string/null field.
            /// Unlike `writer::operator()` this one has field_name
            /// and also it returns itself allowing chained fields definition.
            template<typename T>
            field_stream& operator() (const char* field_name, const T& val)
            {
                add_field_name(field_name);
                writer(val);
                return *this;
            }

            /// Outputs numeric/boolean/string/null optional field only if it has value.
            /// This is helpful when chained fields can be optional.
            /// Example, include fields in json only it they are not NaN:
            /// writer(std::cout).write_object([&](auto fields) {
            ///    fields
            ///      ("x", s.x != s.x ? nullopt : optional(s.x))
            ///      ("y", s.y != s.y ? nullopt : optional(s.y))
            /// });
            template<typename T>
            field_stream& operator() (const char* field_name, const std::optional<T>& val)
            {
                if (val) {
                    add_field_name(field_name);
                    writer(*val);
                }
                return *this;
            }

            /// Outputs array field.
            /// Unlike `writer::operator()` this one has field_name
            /// and also it returns itself allowing chained fields definition.
            template<typename ON_ITEM>
            field_stream& write_array(const char* field_name, size_t size, ON_ITEM&& on_item)
            {
                add_field_name(field_name);
                writer.write_array(size, std::move(on_item));
                return *this;
            }

            /// Outputs object field.
            /// Unlike `writer::operator()` this one has field_name
            /// and also it returns itself allowing chained fields definition.
            template<typename FIELD_MAKER>
            void write_object(const char* field_name, FIELD_MAKER&& field_maker)
            {
                add_field_name(field_name);
                writer.write_object(std::move(field_maker));
                return *this;
            }

        private:
            field_stream(writer& writer)
                : writer(writer)
            {}

            void add_field_name(const char* field_name);

            writer& writer;
            bool is_first = true;
        };

    private:
        std::unique_ptr<std::ostream> holder;
        std::ostream& sink;

        char hex(char c);
        void write_escaped_char(unsigned char c);
    };

}

#endif  // REACTIVE_JSON_JWRITER_H
