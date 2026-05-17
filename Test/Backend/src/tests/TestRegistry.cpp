#include "tests/TestRegistry.h"

#include "tests/BuiltinTests.h"

namespace tests
{
TestRegistry::TestRegistry()
    : tests_(createBuiltinTests())
{
}

ITestCase* TestRegistry::findById(const std::string& id)
{
    for (const auto& test : tests_)
    {
        if (test->id() == id)
        {
            return test.get();
        }
    }
    return nullptr;
}

std::vector<TestInfo> TestRegistry::listTests() const
{
    std::vector<TestInfo> infos;
    infos.reserve(tests_.size());

    for (const auto& test : tests_)
    {
        infos.push_back({test->id(), test->name(), test->defaultSource(), test->allowedSources(), test->parameters()});
    }

    return infos;
}
}
