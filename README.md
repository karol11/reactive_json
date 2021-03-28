# reactive_json
## A C++ library that parses JSONs directly into the application data structures, skipping all irrelevant pieces of data.

99% of applications read JSONs to their own data structures. Other parsers provide two different approaches to do so:

* First approach: Read the file first to the Document Object Model (DOM) and let the application to convert these DOM nodes into application objects. This leads to the huge memory and CPU overhead.
* Second approach: Provide the application with some SAX/StAX interface. In this approach JSON library becomes just-a-lexer, and all actual parsing is delegated to the application, that has to implement some hand-written ingenious state machine, that will:
  * map keys to fields,
  * switch contexts on object and arrays start/end,
  * skip all unneeded structures,
  * handle different variants of mappings,
  * convert, check and normalize data.

ReactiveJSON works in a slightly different paradigm:
* Like SAX/StAX it parses data on the fly without building intermediate DOM.
* But unlike them, ReactiveJSON doesn't feed application with the stream of tokens, instead it allows app to query for the data structures this application expects.
* If some parts of JSON left not claimed, they will be skipped, and it's worth mentioning that in comparison to other libraries the skipping code is not resursive, this protects parser (and application that uses it) from stack overflows if say hacker sent a JSON of 4K '[' characters.

## Example:

Application has two classes - a Point and a Polygon:

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

Application expects JSON to contain an array of points, something like this:

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
    memory_block_reader::reader json(data);
    std::vector<polygon> result;
    json.get_array([&]{
        result.emplace_back();
        auto& poly = result.back();
        json.get_object([&](auto name){
            if (name == "active")
                poly.is_active = json.get_bool(false);
            else if (name == "name")
                poly.name = json.get_string("");
            else if (name == "points")
                json.get_array([&]{
                    poly.points.emplace_back();
                    auto& p = poly.points.back();
                    json.get_object([&](auto name){
                        if (name == "x")
                            p.x = (int) json.get_number(0);
                        else if (name == "y")
                            p.y = (int) json.get_number(0);
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
* If some field, root element or array item will have different type, it will be skipped and replaced with a default value; for example if json.get_bool(true) is called on a string data, it'll skip this string and return true.
* Since all parsing is performed in plain C++ code, we can easily add validating/transforming/versioning logic without inventing weird template-driven DSLs.
* There are additional `*reader::try_*` methods that allow to probe for different data types. Example:

```C++
bool get_my_bool(reader& json) {
    if (auto i = json.try_number())  // returns optional<double>
        return *i != 0;
    if (auto s = a.try_string(5))
        return v = *s == "true" || *s == "yes" || *s == "1";
    return a.get_bool(false);
}
```

## Library contents
* istream_reader - reads from `std::istream`.
* memory_block_reader - reads from the continuous block of memory
  * no memory overheads,
  * much faster,
  * one allocation per string,
  * but it requires the whole JSON to be in one memory block.
