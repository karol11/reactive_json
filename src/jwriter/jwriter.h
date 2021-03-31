#ifndef _JWRITER_H_
#define _JWRITER_H_

#include <ostream>
#include <memory>
#include <optional>

/// Outputs the JSON to the underlying std::ostream.
class jwriter
{
    friend class field_stream;
public:
    /// Constructs jwriter that owns the underlined stream.
    jwriter(std::unique_ptr<std::ostream> sink)
        : holder(std::move(sink))
        , sink(*holder)
    {}

    /// Constructs jwriter that borrows the underlined stream.
    jwriter(std::ostream& sink)
        : sink(sink)
    {}

    /// Outputs single scalar numeric value.
    void operator() (double val) { sink << val; }

    /// Outputs single scalar boolean value.
    void operator() (bool val) { sink << (val ? "true" : "false"); }

    /// Outputs single scalar null value.
    void operator() (nullptr_t) { sink << "null"; }

    /// Outputs single scalar string value.
    void operator() (const char* val)
    {
        sink << '"';
        for (; *val; val++)
            write_escaped_char(*val);
        sink << '"';
    }

    /// Outputs single scalar string value. This one can contain \u0000 characters.
    void operator() (std::string_view val)
    {
        sink << '"';
        for (auto c : val)
            write_escaped_char(c);
        sink << '"';
    }

    /// Outputs an array of items.
    /// `size` defines the array size.
    /// `on_item` Is a lambda to be called for each array item.
    ///           It receives this jwriter `instance` and the array item `index` from 0 to size - 1.
    ///           It should output array item using operator(), write_array or write_object calls.
    /// Example: 
    /// void write_vector(const vector<double>& vec) {
    ///     jwriter(std::cout).write_array(vec.size(), [&](auto& writer, size_t index){
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
    ///                 that replicates the same methods as `jwriter` but extended with `field_name` parameter.
    /// Example:
    /// struct my_struct{ int x; char* str;};
    /// void write_my_struct(const my_struct& data) {
    ///    jwriter(std::cout).write_object([&](auto fields) {
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
        friend class jwriter;
    public:
        /// Outputs numeric/boolean/string/null field.
        /// Unlike `jwriter::operator()` this one has field_name
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
        /// jwriter(std::cout).write_object([&](auto fields) {
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
        /// Unlike `jwriter::operator()` this one has field_name
        /// and also it returns itself allowing chained fields definition.
        template<typename ON_ITEM>
        field_stream& write_array(const char* field_name, size_t size, ON_ITEM&& on_item)
        {
            add_field_name(field_name);
            writer.write_array(size, std::move(on_item));
            return *this;
        }

        /// Outputs object field.
        /// Unlike `jwriter::operator()` this one has field_name
        /// and also it returns itself allowing chained fields definition.
        template<typename FIELD_MAKER>
        void write_object(const char* field_name, FIELD_MAKER&& field_maker)
        {
            add_field_name(field_name);
            writer.write_object(std::move(field_maker));
            return *this;
        }

    private:
        field_stream(jwriter& writer)
            : writer(writer)
        {}

        void add_field_name(const char* field_name)
        {
            if (is_first)
                is_first = false;
            else
                writer.sink << ',';
            writer(field_name);
            writer.sink << ':';
        }

        jwriter& writer;
        bool is_first = true;
    };

private:
    std::unique_ptr<std::ostream> holder;
    std::ostream& sink;

    char hex(char c)
    {
        c &= 0xf;
        return c < 10 ? c + '0' : c - 10 + 'a';
    }

    void write_escaped_char(unsigned char c)
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
};

#endif  // _JWRITER_H_
