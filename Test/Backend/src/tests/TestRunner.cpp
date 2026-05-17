#include "tests/TestRunner.h"

#include "tests/TestContext.h"

#include <algorithm>

namespace tests
{
TestRunner::TestRunner(protocol::MessageRegistry& registry,
                       recorder::ReceiveRecorder& recorder,
                       recorder::ParsedMessageRecorder& parsedRecorder)
    : registry_(registry),
      recorder_(recorder),
      parsedRecorder_(parsedRecorder)
{
}

TestRunner::~TestRunner()
{
    stop();
}

std::vector<TestInfo> TestRunner::listTests() const
{
    return testRegistry_.listTests();
}

TestStatus TestRunner::run(const std::string& testId,
                           const std::string& source,
                           const TestParams& params,
                           transport::ITransport* transport,
                           std::function<void(const std::string&)> logger)
{
    stop();

    ITestCase* test = testRegistry_.findById(testId);
    if (test == nullptr)
    {
        const TestStatus result{testId, "finished", false, "Unknown test id."};
        setStatus(result);
        return result;
    }

    const std::string selectedSource = source.empty() ? test->defaultSource() : source;
    if (!isAllowedSource(*test, selectedSource))
    {
        const TestStatus result{test->id(), "finished", false, "Source is not allowed for this test."};
        setStatus(result);
        return result;
    }

    stopRequested_ = false;
    setStatus({test->id(), "running", false, "Test is running."});

    workerThread_ = std::thread([this, test, selectedSource, params, transport, logger]() {
        TestContext context(selectedSource, params, registry_, recorder_, parsedRecorder_, transport, stopRequested_, logger);
        const auto result = test->run(context);
        const bool stopped = stopRequested_ && !result.passed;
        setStatus({test->id(), stopped ? "stopped" : "finished", result.passed, result.message});
    });

    return status();
}

TestStatus TestRunner::status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

TestStatus TestRunner::stop()
{
    stopRequested_ = true;
    joinWorker();

    auto current = status();
    if (current.state == "running")
    {
        current.state = "stopped";
        current.passed = false;
        current.message = "Test stopped.";
        setStatus(current);
    }

    return status();
}

bool TestRunner::isAllowedSource(const ITestCase& test, const std::string& source) const
{
    const auto sources = test.allowedSources();
    return std::find(sources.begin(), sources.end(), source) != sources.end();
}

void TestRunner::joinWorker()
{
    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}

void TestRunner::setStatus(const TestStatus& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}
}
