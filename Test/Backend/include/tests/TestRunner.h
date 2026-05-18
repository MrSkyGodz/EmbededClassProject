#pragma once

#include "channel/ChannelManager.h"
#include "tests/TestDefinition.h"
#include "tests/TestRegistry.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tests
{
class TestRunner
{
public:
    explicit TestRunner(channel::ChannelManager& channels);
    ~TestRunner();

    std::vector<TestInfo> listTests() const;
    TestStatus run(const std::string& testId,
                   const std::string& source,
                   const TestParams& params,
                   std::function<void(const std::string&)> logger);
    TestStatus status() const;
    TestStatus stop();

private:
    bool isAllowedSource(const ITestCase& test, const std::string& source) const;
    void joinWorker();
    void setStatus(const TestStatus& status);

    channel::ChannelManager& channels_;
    TestRegistry testRegistry_;
    mutable std::mutex mutex_;
    TestStatus status_;
    std::thread workerThread_;
    std::atomic<bool> stopRequested_{false};
};
}
