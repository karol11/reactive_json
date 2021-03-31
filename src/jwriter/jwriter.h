#ifndef _JWRITER_H_
#define _JWRITER_H_

#include <ostream>
#include <memory>
#include <optional>

class jwriter
{
	friend class field_stream;

public:
	jwriter(std::unique_ptr<std::ostream> sink) : holder(std::move(sink)), sink(*holder) {}
	jwriter(std::ostream& sink) : sink(sink) {}

	void operator() (double val) { sink << val; }
	void operator() (bool val) { sink << (val ? "true" : "false"); }
	void operator() (nullptr_t) { sink << "null"; }
	void operator() (const char* val) {
		sink << '"';
		for (; *val; val++)
			write_escaped_char(*val);
		sink << '"';
	}
	void operator() (std::string_view val) {
		sink << '"';
		for (auto c : val)
			write_escaped_char(c);
		sink << '"';
	}

	/// To write vector<double> v
	/// jwriter(std::cout).write_array(v.size(), [&](auto s, auto i){
	///     s(v[i]);
	/// });
	template<typename ON_ITEM>
	void write_array(size_t size, ON_ITEM&& on_item) {
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

	/// To write struct{ int x; char* str;} s;
	/// jwriter(std::cout).write_object([&](auto fields) {
	///     fields
	///         ("x", (double) s.x)
	///         ("str", s.str);
	/// });
	template<typename FIELD_MAKER>
	void write_object(FIELD_MAKER&& field_maker) {
		sink << '{';
		field_maker(field_stream{ *this });
		sink << '}';
	}

	struct field_stream
	{
		template<typename T>
		field_stream& operator() (const char* field_name, const std::optional<T>& val) {
			if (val) {
				add_field_name(field_name);
				writer(*val);
			}
			return *this;
		}

		template<typename T>
		field_stream& operator() (const char* field_name, const T& val) {
			add_field_name(field_name);
			writer(val);
			return *this;
		}

		template<typename ON_ITEM>
		field_stream& write_array(const char* field_name, size_t size, ON_ITEM&& on_item) {
			add_field_name(field_name);
			writer.write_array(size, std::move(on_item));
			return *this;
		}

		template<typename FIELD_MAKER>
		void write_object(const char* field_name, FIELD_MAKER&& field_maker) {
			add_field_name(field_name);
			writer.write_object(std::move(field_maker));
			return *this;
		}

		void add_field_name(const char* field_name) {
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

	char hex(char c) {
		c &= 0xf;
		return c < 10 ? c + '0' : c - 10 + 'a';
	}

	void write_escaped_char(unsigned char c) {
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
