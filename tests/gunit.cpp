#include "gunit.h"
#include <vector>

namespace testing {
    TestRegRecord* tests = nullptr;
    int failed;

    TestRegRecord::TestRegRecord(const char* name, Test* (*fn)())
        : name(name), fn(fn), next(tests) {
        tests = this;
    }

}  // namespace testing

int RUN_ALL_TESTS() {
    ::testing::failed = 0;
    try
    {
        for (testing::TestRegRecord* trr = testing::tests; trr; trr = trr->next)
        {
            std::cout << "Test:" << trr->name << std::endl;
            ::testing::Test* t = trr->fn();
            t->SetUp();
            t->Run();
            t->TearDown();
            delete t;
        }
    }
    catch (int) {
        ::testing::failed++;
    }
    std::cout << (::testing::failed ? "failed" : "passed") << std::endl;
    return ::testing::failed;
}

int main()
{
    return RUN_ALL_TESTS();
}
