#include <vector>
#include "gunit.h"

namespace
{
    TEST(GROUP_NAME, Postive) {
        MK_READER(a, "[[]   ]");
        int outer = 0, inner = 0;
        ASSERT_TRUE(a.try_array([&] {
            outer++;
            ASSERT_TRUE(a.try_array([&] {
                inner++;
                }));
            }));
        ASSERT_EQ(outer, 1);
        ASSERT_EQ(inner, 0);

        RESET_READER(a, "-2.32e-11");
        ASSERT_DOUBLE_EQ(a.get_number(0), -2.32e-11);

        RESET_READER(a, "\"\"");
        ASSERT_EQ(a.get_number(0), 0.0);

        RESET_READER(a, "false");
        ASSERT_EQ(a.get_bool(true), false);

        RESET_READER(a, "true");
        ASSERT_EQ(a.get_bool(false), true);

        RESET_READER(a, "0");
        ASSERT_EQ(a.get_null(), false);
        ASSERT_EQ(a.get_number(55), 0.0);
    }

    TEST(GROUP_NAME, Strings) {
        MK_READER(a, R"-("\u0060\u012a\u12AB")-");
        ASSERT_EQ(a.get_string(""), u8"\u0060\u012a\u12AB");
    }

    TEST(GROUP_NAME, Objects) {
        MK_READER(a, R"-({"asd":"sdf", "dfg":"fgh"})-");
        int i = 0;
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(name, i == 0 ? "asd" : "dfg");
            ASSERT_EQ(a.get_string(""), i == 0 ? "sdf" : "fgh");
            i++;
            }));
        ASSERT_EQ(i, 2);
    }

    TEST(GROUP_NAME, UnusedFieldsInObjects) {
        MK_READER(a, R"-({"asd":"sdf", "dfg":"fgh"})-");
        int i = 0;
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(name, i == 0 ? "asd" : "dfg");
            i++;
            }));
    }

    TEST(GROUP_NAME, ObjectMinMax) {
        MK_READER(a, R"-({ "min": -1.0e+28, "max": 1.0e+28 })-");
        a.get_object([&](auto name) {
            ASSERT_DOUBLE_EQ(a.get_number(0), name == "min" ? -1.0e+28 : 1.0e+28);
            });
    }

    TEST(GROUP_NAME, IncompleteData) {
        MK_READER(a, "-1.0e+28a");
        a.try_number();
        ASSERT_FALSE(a.try_number().has_value()) << "garbage after number";

        RESET_READER(a, "[");
        a.get_array([] {});
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete array";

        RESET_READER(a, "{");
        a.get_object([](auto field) {});
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete object";

        RESET_READER(a, R"-( {12})-");
        a.get_object([](auto field) {});
        ASSERT_FALSE(a.get_error_message().empty()) << "absent field name";

        RESET_READER(a, R"-( {"a"})-");
        a.get_object([](auto field) {});
        ASSERT_FALSE(a.get_error_message().empty()) << "absent ':'";

        RESET_READER(a, R"-( {"a":1,})-");
        a.get_object([](auto field) {});
        ASSERT_FALSE(a.get_error_message().empty()) << "dangling ','";

        RESET_READER(a, R"-( {"a":1; "x":1})-");
        a.get_object([&](auto field) { a.get_number(0); });
        ASSERT_FALSE(a.get_error_message().empty()) << "bad delimiter";

        RESET_READER(a, R"-( {"a":1 "x":1})-");
        a.get_object([](auto field) {});
        ASSERT_FALSE(a.get_error_message().empty()) << "no delimiters in object";

        RESET_READER(a, R"-( ")-");
        auto str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete string";

        RESET_READER(a, R"-( "\)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete string escape";

        RESET_READER(a, R"-( "\x)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "bad string escape";

        RESET_READER(a, R"-( "\u)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete \\u sequence";

        RESET_READER(a, R"-( "\u0)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete \\uX sequence";

        RESET_READER(a, R"-( "\u12)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete \\uXX sequence";

        RESET_READER(a, R"-( "\u123)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete \\uXXX sequence";

        RESET_READER(a, R"-( "\udd01)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete first surrogate";

        RESET_READER(a, R"-( "\udd01\)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete \\ after first surrogate";

        RESET_READER(a, R"-( "\udd01\u)-");
        str = a.get_string("");
        ASSERT_FALSE(a.get_error_message().empty()) << "incomplete \\u after first surrogate";
    }

    TEST(GROUP_NAME, Skipping) {
        MK_READER(a, R"-({"field":[1,2,3, "text with\rescapes\"\u2200\"", [{},[-1.34.e-11]]], "f1":false})-");
        a.get_bool(false);
        ASSERT_TRUE(a.success());
    }

    TEST(GROUP_NAME, LimitedString) {
        MK_READER(a, R"-("long string")-");
        ASSERT_EQ(a.get_string("", 4), "long");
        ASSERT_TRUE(a.success());

        RESET_READER(a, R"-("lon\n string")-");
        ASSERT_EQ(a.get_string("", 4), "lon\n");
        ASSERT_TRUE(a.success());

        RESET_READER(a, R"-("lon\u1234 string")-");
        ASSERT_EQ(a.get_string("", 4), "lon");
        ASSERT_TRUE(a.success());
    }

    TEST(GROUP_NAME, Alternatives) {
        MK_READER(a, R"-("yes")-");
        bool v = false;
        if (auto i = a.try_number())
            v = *i != 0;
        else if (auto s = a.try_string(5))
            v = *s == "true" || *s == "yes" || *s == "1";
        else
            v = a.get_bool(false);
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
        MK_READER(json, data);
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

    TEST(GROUP_NAME, RealLifeExample) {
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
