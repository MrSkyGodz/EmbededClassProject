#pragma once

#include "tests/TestDefinition.h"

#include <memory>
#include <vector>

namespace tests
{
std::vector<std::unique_ptr<ITestCase>> createBuiltinTests();
}
