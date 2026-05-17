#pragma once

#include "protocol/MessageRegistry.h"
#include "recorder/ParsedMessageRecorder.h"
#include "recorder/ReceiveRecorder.h"
#include "tests/TestDefinition.h"
#include "tests/TestRegistry.h"
#include "transport/ITransport.h"

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
    TestRunner(protocol::MessageRegistry& registry,
               recorder::ReceiveRecorder& recorder,
               recorder::ParsedMessageRecorder& parsedRecorder);
    ~TestRunner();

    std::vector<TestInfo> listTests() const;
    TestStatus run(const std::string& testId,
                   const std::string& source,
                   const TestParams& params,
                   transport::ITransport* transport,
                   std::function<void(const std::string&)> logger);
    TestStatus status() const;
    TestStatus stop();

private:
    bool isAllowedSource(const ITestCase& test, const std::string& source) const;
    void joinWorker();
    void setStatus(const TestStatus& status);

    protocol::MessageRegistry& registry_;
    recorder::ReceiveRecorder& recorder_;
    recorder::ParsedMessageRecorder& parsedRecorder_;
    TestRegistry testRegistry_;
    mutable std::mutex mutex_;
    TestStatus status_;
    std::thread workerThread_;
    std::atomic<bool> stopRequested_{false};
};
}
