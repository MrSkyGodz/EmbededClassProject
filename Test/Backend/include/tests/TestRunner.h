#pragma once

#include "protocol/MessageRegistry.h"
#include "recorder/ReceiveRecorder.h"
#include "transport/ITransport.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tests
{
struct TestInfo
{
    std::string id;
    std::string name;
    std::string source;
};

struct TestStatus
{
    std::string id = "none";
    std::string state = "idle";
    bool passed = false;
    std::string message = "No test has run yet.";
};

class TestRunner
{
public:
    TestRunner(protocol::MessageRegistry& registry, recorder::ReceiveRecorder& recorder);
    ~TestRunner();

    std::vector<TestInfo> listTests() const;
    TestStatus run(const std::string& testId, const std::string& source, transport::ITransport* transport);
    TestStatus status() const;
    TestStatus stop();

private:
    TestStatus runImmediate(const std::string& testId, const std::string& source, transport::ITransport* transport);
    void startLive(const std::string& testId);
    void setStatus(const TestStatus& status);

    protocol::MessageRegistry& registry_;
    recorder::ReceiveRecorder& recorder_;
    mutable std::mutex mutex_;
    TestStatus status_;
    std::thread liveThread_;
    std::atomic<bool> liveStop_{false};
};
}
