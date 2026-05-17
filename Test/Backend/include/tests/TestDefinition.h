#pragma once

#include "recorder/ParsedMessageRecorder.h"
#include "recorder/ReceiveRecorder.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tests
{
struct TestParameter
{
    std::string name;
    std::string label;
    std::string type = "number";
    double defaultValue = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
};

struct TestInfo
{
    std::string id;
    std::string name;
    std::string source;
    std::vector<std::string> allowedSources;
    std::vector<TestParameter> parameters;
};

struct TestStatus
{
    std::string id = "none";
    std::string state = "idle";
    bool passed = false;
    std::string message = "No test has run yet.";
};

struct TestResult
{
    bool passed = false;
    std::string message;
};

class TestContext;

class ITestCase
{
public:
    virtual ~ITestCase() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string defaultSource() const = 0;
    virtual std::vector<std::string> allowedSources() const = 0;
    virtual std::vector<TestParameter> parameters() const { return {}; }
    virtual TestResult run(TestContext& context) = 0;
};

using TestParams = std::map<std::string, double>;
}
