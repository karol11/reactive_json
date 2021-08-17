IMPORTANT: This is not an officially supported Google product.

# Reactive JSON
## A C++ library that reads/writes JSONs directly from/into the application data structures, skipping all irrelevant pieces of data.

## Reader

99% of applications read JSONs to their own data structures.

Different parsers usually provide two main approaches to do so:

* Some of them read the file first to the Document Object Model (DOM) and let the application to convert these DOM nodes into application objects. This leads to the substantual memory and CPU overheads.
* Others provide the application with some SAX/StAX interface. In this approach JSON library becomes just-a-lexer, and all actual parsing is delegated to the application, that has to implement some hand-written ingenious state machine, that will:
  * map keys to fields,
  * switch contexts and mappings on object and array starts/ends,
  * skip all unneeded structures,
  * handle different variants of mappings,
  * convert, check and normalize data.

ReactiveJSON works in a slightly different paradigm:
* Like SAX/StAX it parses data on the fly without building intermediate DOM.
* But unlike them, ReactiveJSON doesn't feed application with the stream of tokens, instead it allows app to query for the data structures this application expects.
* If some parts of incoming JSON left not claimed, they will be skipped, and it's worth mentioning that in comparison to other libraries the skipping code is not resursive. This protects the parser (and the application that uses it) from stack overflows if say, a hacker send a 4K JSON of `[` characters.

### Example:

An application has two classes - a Point and a Polygon:

```C++
struct point{
    int x, y;
};
struct polygon {
    std::string name;
    std::vector<point> points;
    bool is_active;
};
```

This application expects JSON to contain an array of points, something like this:

```JSON
[
    {
        "active": false,
        "name": "p1",
        "points": [
            {"x": 11, "y": 32},
            {"y": 23, "x": 12},
            {"x": -1, "y": 4}
        ]
    },
    {
        "points": [
            {"x": 10, "y": 0},
            {"x": 0, "y": 10},
            {"y": 0, "x": 0}
        ],
        "active": true,
        "name": "Corner"
    }

]
```

This structure can be parsed in a straightforward way:

```C++
std::vector<polygon> parse_json(const char* data) {
    reactive_json::memory_block_reader json(data);
    std::vector<polygon> result;
    json.get_array([&]{
        result.emplace_back();
        json.get_object([&, &poly = result.back()](auto name){
            if (name == "active")
                poly.is_active = json.get_bool(false);
            else if (name == "name")
                poly.name = json.get_string("");
            else if (name == "points")
                json.get_array([&]{
                    poly.points.emplace_back();
                    json.get_object([&, &pt = poly.points.back()](auto name){
                        if (name == "x")
                            pt.x = (int) json.get_number(0);
                        else if (name == "y")
                            pt.y = (int) json.get_number(0);
                    });
                });
        });
    });
    return result;
}
```

This code handles all the edge cases:
* If a JSON has an unexpected field, this field will be skipped with all its subtree.
* This code is tolerant to any order of fields in objects.
* If some field is absent from the JSON, the corresponding object will have a default value.
* If some field, root element or array item will have a different type, it will be skipped and replaced with the default value; for example if `json.get_bool(true)` is called on a array of objects, it'll skip this array and return `true`.
* Since all parsing is performed in plain C++ code, we can easily add validating/transforming/versioning logic without inventing weird template-driven or string-encoded DSLs.
* There are additional `*reader::try_*` methods that allow to probe for different data types. Example:

```C++
bool get_bool_my_way(reader& json) {
    if (auto i = json.try_number())  // returns optional<double>
        return *i != 0;
    if (auto s = a.try_string(5))
        return *s == "true" || *s == "yes" || *s == "1";
    return a.get_bool(false);
}
```

## Writer

The `reactive_json::writer` allows to serialize application data directly to the `std::ostream` without creating of intermediate data structures.
This `writer` instance can be created as either having ownership over the `std::ostream` instance (using `unique_ptr`) or by borrowing the existing stream (by reference).

Writer has a number of overloaded `operator()` that allow to write `null`, `bool`, `double` and `string` primitives.

Arrays and objects are slightly different:
* Arrays are written by the `write_array` method, that takes two parameters: the `array_size` and an `on_item` lambda, that will write array items. This lambda is called for each array item. It receives item `index`. (:warning: This `on_item` lambda always takes its first `writer` parameter by reference, use `auto&`).
* Objects are serialized with the `write_object` method. It takes a `field_maker` lambda, that is called _one time_ with the `field_stream` object.

Field streams have the same methods as writer but they accept additional parameter `field_name`.

### Example

Write the above data structures from `std::vector<polygon> root` to a file:

```C++
reactive_json::writer(std::make_unique<std::ostream>(file_name, std::ios::binary))
.write_array(root.size(), [&](auto& writer, size_t index) {
    writer.write_object([&poly = root[index]](auto fields) {
        fields("name", poly.name)
              ("active", poly.is_active);
        fields.write_array("points", poly.points.size(), [&](auto& writer, size_t index) {
            writer.write_object([&pt = poly.points[index]](auto fields){
                fields("x", pt.x)("y", pt.y);
            });
        });
    });
});
```

## DOM

What if your application is in that 1% of applications which need some arbaitrary Document Object Model (DOM)?

* You can create it and tailor it to your needs with less than 20 lines of code (LoC).
* And with just 16 more LoC it can be parsed from any input stream or memory block.
* And 16 more LoC gives you ability to write it back.

And since this data model is completely decoupled from JSON parser/writer you can easily modify it,
or reuse one already existing in your application.

Please use this code as an example [dom_io_test](https://github.com/karol11/reactive_json/tree/main/tests/dom_io_test.cpp).

## Library contents
* istream_reader - reads from `std::istream`.
* memory_block_reader - reads from the continuous block of memory
  * no memory overheads,
  * much faster,
  * one allocation per string,
  * but it requires the whole JSON to be in one memory block.
* writer - writes JSON to `std::istream`.
