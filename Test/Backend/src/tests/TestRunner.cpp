#include "tests/TestRunner.h"

#include "protocol/Frame.h"

#include <chrono>
#include <map>

namespace
{
bool containsHeader(const std::vector<uint8_t>& bytes)
{
    for (size_t i = 1U; i < bytes.size(); ++i)
    {
        if (bytes[i - 1U] == 0xAAU && bytes[i] == 0x55U)
        {
            return true;
        }
    }
    return false;
}
}

namespace tests
{
TestRunner::TestRunner(protocol::MessageRegistry& registry, recorder::ReceiveRecorder& recorder)
    : registry_(registry),
      recorder_(recorder)
{
}

TestRunner::~TestRunner()
{
    stop();
}

std::vector<TestInfo> TestRunner::listTests() const
{
    return {
        {"pwm_frame_mock", "Build and loop back PWM frame", "mock"},
        {"motor_frame_mock", "Build and loop back motor frame", "mock"},
        {"received_has_data", "Received buffer has data", "received_snapshot"},
        {"received_contains_frame_header", "Received data contains AA 55 frame header", "received_snapshot"},
        {"live_receives_data", "Live stream receives new bytes", "received_live"},
    };
}

TestStatus TestRunner::run(const std::string& testId, const std::string& source, transport::ITransport* transport)
{
    if (source == "received_live")
    {
        startLive(testId);
        return status();
    }

    const auto result = runImmediate(testId, source, transport);
    setStatus(result);
    return result;
}

TestStatus TestRunner::status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

TestStatus TestRunner::stop()
{
    liveStop_ = true;
    if (liveThread_.joinable())
    {
        liveThread_.join();
    }

    auto current = status();
    if (current.state == "running")
    {
        current.state = "stopped";
        current.passed = false;
        current.message = "Live test stopped.";
        setStatus(current);
    }

    return status();
}

TestStatus TestRunner::runImmediate(const std::string& testId, const std::string& source, transport::ITransport* transport)
{
    if (source == "mock" || source == "serial")
    {
        if (transport == nullptr || !transport->isOpen())
        {
            return {testId, "finished", false, "Transport is not open."};
        }

        std::map<std::string, double> values;
        if (testId == "pwm_frame_mock")
        {
            values["pwm"] = 128.0;
        }
        else if (testId == "motor_frame_mock")
        {
            values["motor1AngleDeg"] = 90.0;
            values["motor2AngleDeg"] = 45.0;
        }
        else
        {
            return {testId, "finished", false, "Unknown transport test."};
        }

        std::vector<uint8_t> payload;
        std::string error;
        const std::string messageType = testId == "pwm_frame_mock" ? "pwm" : "motor";
        if (!registry_.buildPayload(messageType, values, payload, error))
        {
            return {testId, "finished", false, error};
        }

        const auto frame = protocol::buildFrame(payload);
        if (!protocol::startsWithFrameHeader(frame.bytes))
        {
            return {testId, "finished", false, "Frame header is invalid."};
        }

        if (!transport->write(frame.bytes.data(), frame.bytes.size(), error))
        {
            return {testId, "finished", false, error};
        }

        return {testId, "finished", true, "Frame built and written successfully."};
    }

    if (source == "received_snapshot")
    {
        const auto bytes = recorder_.snapshotValues();
        if (testId == "received_has_data")
        {
            return {testId, "finished", !bytes.empty(), bytes.empty() ? "No received data yet." : "Received data exists."};
        }

        if (testId == "received_contains_frame_header")
        {
            const bool ok = containsHeader(bytes);
            return {testId, "finished", ok, ok ? "Frame header found in received data." : "AA 55 header was not found."};
        }
    }

    return {testId, "finished", false, "Unknown test/source combination."};
}

void TestRunner::startLive(const std::string& testId)
{
    stop();
    liveStop_ = false;
    setStatus({testId, "running", false, "Waiting for new received bytes."});

    liveThread_ = std::thread([this, testId]() {
        const uint64_t startCount = recorder_.totalReceived();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

        while (!liveStop_ && std::chrono::steady_clock::now() < deadline)
        {
            if (recorder_.totalReceived() > startCount)
            {
                setStatus({testId, "finished", true, "Live data received."});
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!liveStop_)
        {
            setStatus({testId, "finished", false, "Timed out waiting for live data."});
        }
    });
}

void TestRunner::setStatus(const TestStatus& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}
}
