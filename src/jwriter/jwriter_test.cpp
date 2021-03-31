#include<vector>
#include <sstream>
#include "jwriter.h"
#include "gunit.h"

namespace
{
    using std::string;
    using std::vector;
    using std::stringstream;

    struct point { double x, y; };
    struct polygon {
        string name;
        bool is_active;
        vector<point> points;
    };

    TEST(JsonWriter, Simple)
    {
        vector<polygon> root = {
            {
                "First",
                true,
                {
                    { 0, 0 },
                    { 10, -10.5 },
                    { 1e+11, .5 },
                }
            },
            {
                "Second\r",
                false,
                {
                    { -20, 30 },
                    { 10, -10.5 },
                    { 333, .5555e-10 },
                }
            }
        };
        stringstream s;
        jwriter(s).write_array(root.size(), [&](auto& s, size_t i) {
            s.write_object([&poly = root[i]](auto& s) {
                s("name", poly.name);
                s("active", poly.is_active);
                s.write_array("points", poly.points.size(), [&](auto& s, size_t i) {
                    s.write_object([&pt = poly.points[i]](auto& s){
                        s("x", pt.x)("y", pt.y);
                    });
                });
            });
        });
        ASSERT_EQ(s.str(), R"-([{"name":"First","active":true,"points":[{"x":0,"y":0},{"x":10,"y":-10.5},{"x":1e+11,"y":0.5}]},{"name":"Second\r","active":false,"points":[{"x":-20,"y":30},{"x":10,"y":-10.5},{"x":333,"y":5.555e-11}]}])-");
    }
}