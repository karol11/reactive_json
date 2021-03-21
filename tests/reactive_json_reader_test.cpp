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
        ASSERT_TRUE(a.try_object([&](auto name) {
            ASSERT_EQ(a.get_number(0), name == "min" ? -1.0e+28 : 1.0e+28);
        }));
    }
}
