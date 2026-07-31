#include "TestFramework.h"

int main()
{
    int testCount = 0;

    for (const auto& test : shifterfx::test::registry())
    {
        std::cout << "RUN  " << test.name << '\n';
        const int failuresBefore = shifterfx::test::failureCount;
        test.fn();
        if (shifterfx::test::failureCount == failuresBefore)
            std::cout << "PASS " << test.name << '\n';
        ++testCount;
    }

    std::cout << "----\n";
    std::cout << testCount << " test case(s) run, " << shifterfx::test::failureCount << " assertion failure(s).\n";

    return shifterfx::test::failureCount == 0 ? 0 : 1;
}
