# reactive_json
C++ library that parses JSONs directly into the application data structures, skipping all irrelevant pieces of data.

99% of time applications read JSONs to fill-out their own data structures. Existing parser provide two different approaches to do so:

* Read it to the Document Object Model (DOM) and later convert these DOM nodes to application objects.
* Parse JSON with some SAX/StAX parser (that's actually a lexer) providing it with hand-written ingenious state machine, that will:
  * map keys to fields,
  * switch contexts on object and arrays start/end,
  * skip all unneeded structures,
  * handle different variants of mappings,
  * convert, check and normalize data.

ReactiveJSON works in the whole different paradigm:
* Like SAX/StAX it parses data on the fly without building intermediate DOM.
* But unlike them, ReactiveJSON doesnt feed application with the stream of tokens, instead it allows app to query data this app needed.

## Example:

Application has Point and Polygon classes:

```C++
struct point{
    int x, int y;
};
struct polygon {
    std::string name;
    std::vector<point> points;
    bool is_active;
}

Application expects JSON to contain an array of points:

```JSON
[
    {
        "active": false,
        "name": "p1",
        "points": [
            {"x": 11, "y": 32},
            {"y": 23, "x": 12},
            {"x": -1, "y": 4},
        ]
    },
    {
        "points": [
            {"x": 10, "y": 0},
            {"x": 0, "y": 10},
            {"y": 0, "x": 0},
        ],
        "active": true,
        "name": "Corner"
    }

]
```

This structure can be parsed as simle as:

```C++
std::vector<polygon> parse_json(const char* data) {
    reactive_json::reader json(data);
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
                    poly.emplace_back();
                    auto& p = poly.back();
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

This code handles all cases:
* If JSON has an unexpected field, it will be skipped along with its subtree.
* This code is tolerant to any order of fields in objects.
* If some fields are absent from the JSON, they will have default values.
* If some field, root element or array item will have different type, it will be skipped and replaced with default value; for example if json.get_bool(true) is called on a string data, it'll skip this string and return true.
* There is an additional `reader` methods `try_*` that allow to probe for different data types.
