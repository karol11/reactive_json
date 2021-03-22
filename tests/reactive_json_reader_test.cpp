#include <vector>
#include "reactive_json_reader.h"
#include "gunit.h"

namespace
{
    using reactive_json::reader;

    TEST(ReactiveJson, Postive) {
        reader a("[[]   ]");
        int outer = 0, inner = 0;
        ASSERT_TRUE(a.try_array([&] {
            outer++;
            ASSERT_TRUE(a.try_array([&] {
                inner++;
            }));
        }));
        ASSERT_EQ(outer, 1);
        ASSERT_EQ(inner, 0);

        a.reset("-2.32e-11");
        ASSERT_EQ(a.get_number(0), -2.32e-11);

        a.reset("\"\"");
        ASSERT_EQ(a.get_number(0), 0.0);

        a.reset("false");
        ASSERT_EQ(a.get_bool(true), false);

        a.reset("true");
        ASSERT_EQ(a.get_bool(false), true);

        a.reset("0");
        ASSERT_EQ(a.get_null(), false);
        ASSERT_EQ(a.get_number(55), 0.0);
    }

    TEST(ReactiveJson, Strings) {
        reader a(R"-("\u0060\u012a\u12AB")-");
        ASSERT_EQ(a.get_string(""), u8"\u0060\u012a\u12AB");
    }

    TEST(ReactiveJson, Objects) {
        reader a(R"-({"asd":"sdf", "dfg":"fgh"})-");
        int i = 0;
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(name, i == 0 ? "asd" : "dfg");
            ASSERT_EQ(a.get_string(""), i == 0 ? "sdf" : "fgh");
            i++;
        }));
        ASSERT_EQ(i, 2);
    }

    TEST(ReactiveJson, UnusedFieldsInObjects) {
        reader a(R"-({"asd":"sdf", "dfg":"fgh"})-");
        int i = 0;
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(name, i == 0 ? "asd" : "dfg");
            i++;
        }));
    }

    TEST(ReactiveJson, ObjectMinMax) {
        reader a(R"-({ "min": -1.0e+28, "max": 1.0e+28 })-");
        a.get_object([&](auto name) {
            ASSERT_DOUBLE_EQ(a.get_number(0), name == "min" ? -1.0e+28 : 1.0e+28);
        });
    }

    TEST(ReactiveJson, IncompleteData) {
        reader a("-1.0e+28a");
        ASSERT_FALSE(a.try_number().has_value()) << "garbage after number";

        a.reset("[");
        a.get_array([] {});
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete array";

        a.reset("{");
        a.get_object([] (auto field){});
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete object";

        a.reset(R"-( {12})-");
        a.get_object([](auto field) {});
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "absent field name";

        a.reset(R"-( {"a"})-");
        a.get_object([](auto field) {});
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "absent ':'";

        a.reset(R"-( {"a":1,})-");
        a.get_object([](auto field) {});
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "dangling ','";

        a.reset(R"-( {"a":1; "x":1})-");
        a.get_object([&](auto field) { a.get_number(0); });
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "bad delimiter";

        a.reset(R"-( {"a":1 "x":1})-");
        a.get_object([](auto field) {});
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "no delimiters in object";

        a.reset(R"-( ")-");
        auto str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete string";

        a.reset(R"-( "\)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete string escape";

        a.reset(R"-( "\x)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "bad string escape";

        a.reset(R"-( "\u)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete \\u sequence";

        a.reset(R"-( "\u0)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete \\uX sequence";

        a.reset(R"-( "\u12)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete \\uXX sequence";

        a.reset(R"-( "\u123)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete \\uXXX sequence";

        a.reset(R"-( "\udd01)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete first surrogate";

        a.reset(R"-( "\udd01\)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete \\ after first surrogate";

        a.reset(R"-( "\udd01\u)-");
        str = a.get_string("");
        ASSERT_TRUE(a.get_error_pos() != nullptr) << "incomplete \\u after first surrogate";
    }

    TEST(ReactiveJson, Skipping) {
        reader a(R"-({"field":[1,2,3, "text with\rescapes\"\u2200\"", [{},[-1.34.e-11]]], "f1":false})-");
        a.get_bool(false);
        ASSERT_TRUE(a.success());
    }

    TEST(ReactiveJson, LimitedString) {
        reader a(R"-("long string")-");
        ASSERT_EQ(a.get_string("", 4), "long");
        ASSERT_TRUE(a.success());

        a.reset(R"-("lon\n string")-");
        ASSERT_EQ(a.get_string("", 4), "lon\n");
        ASSERT_TRUE(a.success());

        a.reset(R"-("lon\u1234 string")-");
        ASSERT_EQ(a.get_string("", 4), "lon");
        ASSERT_TRUE(a.success());
    }

    TEST(ReactiveJson, Alternatives) {
        reader a(R"-("yes")-");
        bool v = false;
        if (auto b = a.try_bool())
            v = *b;
        else if (auto i = a.try_number())
            v = *i == 1 || *i == -1;
        else if (auto s = a.try_string(5))
            v = *s == "true" || *s == "yes" || *s == "1";
        a.get_bool(false);
        ASSERT_TRUE(v);
        ASSERT_TRUE(a.success());
    }

    struct point {
        int x, y;
    };
    struct polygon {
        std::string name;
        std::vector<point> points;
        bool is_active;
    };

    std::vector<polygon> parse_json(const char* data) {
        reactive_json::reader json(data);
        std::vector<polygon> result;
        json.get_array([&] {
            result.emplace_back();
            auto& poly = result.back();
            json.get_object([&](auto name) {
                if (name == "active")
                    poly.is_active = json.get_bool(false);
                else if (name == "name")
                    poly.name = json.get_string("");
                else if (name == "points")
                    json.get_array([&] {
                        poly.points.emplace_back();
                        auto& p = poly.points.back();
                        json.get_object([&](auto name) {
                            if (name == "x")
                                p.x = (int)json.get_number(0);
                            else if (name == "y")
                                p.y = (int)json.get_number(0);
                        });
                    });
            });
        });
        ASSERT_TRUE(json.success());
        return result;
    }

    TEST(ReactiveJson, RealLifeExample) {
        auto r = parse_json(R"-(
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
                        {"x": 0, "y": 10},
                        {"y": 0, "x": 0}
                    ],
                    "active": true,
                    "name": "Corner"
                }
            ]
        )-");
        ASSERT_EQ(r.size(), 2);
        ASSERT_EQ(r[0].points.size(), 3);
        ASSERT_EQ(r[1].points.size(), 3);
        ASSERT_EQ(r[1].name, "Corner");
        ASSERT_TRUE(r[1].is_active);
        ASSERT_EQ(r[1].points[1].y, 10);
    }
}
