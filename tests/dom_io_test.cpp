#include <variant>
#include <map>
#include <vector>
#include <string>
#include <sstream>

#include "gunit.h"
#include "../src/istream_reader/istream_reader.h"
#include "../src/writer/writer.h"

using std::variant;
using std::map;
using std::vector;
using std::string;
using std::move;
using std::get_if;

/// This test is a demonstration:
/// What if our application is that 1% of applications that need some arbitrary Document Object Model?
/// We can create i and tailor it to our needs with less than 20 lines of code.
/// 
/// And with just 16 more LoC it can be parsed from any input stream or memory block.
/// And 16 more LoC gives you ability to write it back.
/// 
/// And since this data model is completely decoupled from JSON parser/generator you can easily modify it,
/// or reuse one already existing in your application.
/// 
/// Please use this code as an example.


/// This will be our dom

struct Node
{
	std::variant<
		nullptr_t,
		bool,
		double,
		string,
		map<string, Node>,
		vector<Node>> value;

	Node& operator[] (size_t index);
	Node& operator() (const char* key);
	std::string_view as_str(const char* default_value="");
	bool as_bool(bool default_value = false);
	double as_num(double default_value = 0);
	operator bool();
};

/// This will be our DOM reader

Node read(reactive_json::istream_reader& stream) {
	if (stream.get_null())
		return {};
	if (auto v = stream.try_bool())
		return { *v };
	if (auto v = stream.try_number())
		return { *v };
	if (auto v = stream.try_string())
		return { move(*v) };
	vector<Node> arr;
	if (stream.try_array([&] { arr.push_back(read(stream)); }))
		return { move(arr) };
	map<string, Node> obj;
	if (stream.try_object([&](auto field) { obj.insert({ move(field), read(stream) }); }))
		return { move(obj) };
	stream.set_error("unexpected node type");
	return {};
};

// This will be our DOM writer

void write(reactive_json::writer& stream, const Node& n) {
	if (auto v = get_if<bool>(&n.value))
		stream(*v);
	else if (auto v = get_if<double>(&n.value))
		stream(*v);
	else if (auto v = get_if<nullptr_t>(&n.value))
		stream(*v);
	else if (auto v = get_if<string>(&n.value))
		stream(*v);
	else if (auto v = get_if<vector<Node>>(&n.value))
		stream.write_array(v->size(), [&](auto& s, size_t i) { write(s, (*v)[i]); });
	else if (auto v = get_if<map<string, Node>>(&n.value))
		stream.write_object([&](auto field_stream) {
			for (const auto& [field_name, field_val] : *v) {
				write(field_stream.write_field(field_name.c_str()), field_val);
			}
		});
}

TEST(ReactiveJson, DomManipulationExample)
{
	// Let's read it from file
	// Node dom = read(reactive_json::istream_reader(std::make_unique<std::ifstream>("test.json", std::ios::binary)));

	// Or let's read it from string
	Node dom = read(reactive_json::istream_reader(std::make_unique<std::stringstream>(R"-(
        [
            {
                "active": false,
                "name": "p1",
                "points": [
                    {"x": 11, "y": 32, "z": 30},
                    {"y": 23, "x": 12},
                    {"x": -1, "y": 4}
                ]
            },
            {
                "points": [
                    {"x": 10, "y": 0},
                    {"x": 0, "y": 10, "unexpected": "data"},
                    {"y": 0, "x": 0}
                ],
                "active": true,
                "name": "Corner"
            }
        ]
	)-")));

	// Access it
	ASSERT_EQ(dom[1]("name").as_str(), "Corner");
	ASSERT_EQ(dom[0]("points")[0]("z").as_num(), 30);

	// Modify it
	dom[0] = Node{ false };

	// Write it to file
	// write(reactive_json::writer(std::make_unique<std::ostream>(file_name, std::ios::binary)), root);

	// Write it to string
	std::stringstream s;
	write(reactive_json::writer(s), dom);
	ASSERT_EQ(s.str(), R"-([false,{"active":true,"name":"Corner","points":[{"x":10,"y":0},{"unexpected":"data","x":0,"y":10},{"x":0,"y":0}]}])-");
}


// Our DOM helpers, that keep our demo code clean

Node null_node;

Node& Node::operator[] (size_t index) {
	auto v = get_if<vector<Node>>(&value);
	return v && index < v->size() ? (*v)[index] : null_node;
}

Node& Node::operator() (const char* key) {
	if (auto v = get_if<map<string, Node>>(&value)) {
		auto it = v->find(key);
		if (it != v->end())
			return it->second;
	}
	return null_node;
}

std::string_view Node::as_str(const char* default_value) {
	auto v = get_if<string>(&value);
	return v ? std::string_view(*v) : std::string_view(default_value);
}

bool Node::as_bool(bool default_value) {
	auto v = get_if<bool>(&value);
	return v ? *v : default_value;
}

double Node::as_num(double default_value) {
	auto v = get_if<double>(&value);
	return v ? *v : default_value;
}

Node::operator bool() { return std::get_if<nullptr_t>(&value); }
