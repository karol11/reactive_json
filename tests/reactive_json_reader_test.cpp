#include "reactive_json_reader.h"
#include "gunit.h"

namespace
{
    using reactive_json::reader;

    TEST(ReactiveJson, Postive)
    {
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

    TEST(ReactiveJson, Strings)
    {
        reader a(R"-("\u0060\u012a\u12AB")-");
        ASSERT_EQ(a.get_string(""), u8"\u0060\u012a\u12AB");
    }

    TEST(ReactiveJson, Objects)
    {
        reader a(R"-({"asd":"sdf", "dfg":"fgh"})-");
        int i = 0;
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(name, i == 0 ? "asd" : "dfg");
            ASSERT_EQ(a.get_string(""), i == 0 ? "sdf" : "fgh");
            i++;
        }));
        ASSERT_EQ(i, 2);
    }

    TEST(ReactiveJson, UnusedFieldsInObjects)
    {
        reader a(R"-({"asd":"sdf", "dfg":"fgh"})-");
        int i = 0;
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(name, i == 0 ? "asd" : "dfg");
            i++;
        }));
    }

    TEST(ReactiveJson, ObjectMinMax)
    {
        reader a(R"-({ "min": -1.0e+28, "max": 1.0e+28 })-");
        a.get_object([&](auto name) {
            ASSERT_DOUBLE_EQ(a.get_number(0), name == "min" ? -1.0e+28 : 1.0e+28);
        });
    }

    TEST(ReactiveJson, IncompleteData)
    {
        double d;
        reader a("-1.0e+28a");
        ASSERT_FALSE(a.try_number(d)) << "garbage after number";

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
}
