#pragma once

#include "tests/TestDefinition.h"

#include <memory>
#include <vector>

namespace tests
{
class TestRegistry
{
public:
    TestRegistry();

    ITestCase* findById(const std::string& id);
    std::vector<TestInfo> listTests() const;

private:
    std::vector<std::unique_ptr<ITestCase>> tests_;
};
}
