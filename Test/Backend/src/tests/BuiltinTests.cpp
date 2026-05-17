#include "tests/BuiltinTests.h"

#include "tests/TestContext.h"

#include <cmath>
#include <chrono>
#include <memory>
#include <sstream>
#include <thread>

namespace
{
bool containsFrameHeader(const std::vector<uint8_t>& bytes)
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

bool fieldWithin(const recorder::ParsedMessage& message,
                 const std::string& fieldName,
                 double expected,
                 double tolerance,
                 std::string& mismatch)
{
    const auto it = message.values.find(fieldName);
    if (it == message.values.end())
    {
        mismatch = "Expected field was not found: " + fieldName;
        return false;
    }

    const double delta = std::fabs(it->second - expected);
    if (delta > tolerance)
    {
        std::ostringstream out;
        out << fieldName << " expected " << expected << " got " << it->second << " tolerance " << tolerance;
        mismatch = out.str();
        return false;
    }

    return true;
}

class PwmFrameWriteTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "pwm_frame_write"; }
    std::string name() const override { return "Build and write PWM frame"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {{"pwm", "PWM", "number", 128.0, 0.0, 255.0}};
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        const double pwm = context.paramNumber("pwm", 128.0);
        std::string error;
        if (!context.sendMessage("pwm", {{"pwm", pwm}}, error))
        {
            return {false, error};
        }
        return {true, "PWM frame built and written successfully."};
    }
};

class MotorFrameWriteTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "motor_frame_write"; }
    std::string name() const override { return "Build and write motor frame"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {
            {"motor1AngleDeg", "Motor 1 angle", "number", 90.0, 0.0, 180.0},
            {"motor2AngleDeg", "Motor 2 angle", "number", 45.0, 0.0, 180.0},
        };
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        const double motor1 = context.paramNumber("motor1AngleDeg", 90.0);
        const double motor2 = context.paramNumber("motor2AngleDeg", 45.0);
        std::string error;
        if (!context.sendMessage("motor", {{"motor1AngleDeg", motor1}, {"motor2AngleDeg", motor2}}, error))
        {
            return {false, error};
        }
        return {true, "Motor frame built and written successfully."};
    }
};

class ReceivedHasDataTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "received_has_data"; }
    std::string name() const override { return "Received buffer has data"; }
    std::string defaultSource() const override { return "received_snapshot"; }
    std::vector<std::string> allowedSources() const override { return {"received_snapshot"}; }

    tests::TestResult run(tests::TestContext& context) override
    {
        const auto bytes = context.snapshotRaw();
        return {!bytes.empty(), bytes.empty() ? "No received data yet." : "Received data exists."};
    }
};

class ReceivedContainsFrameHeaderTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "received_contains_frame_header"; }
    std::string name() const override { return "Received data contains AA 55 frame header"; }
    std::string defaultSource() const override { return "received_snapshot"; }
    std::vector<std::string> allowedSources() const override { return {"received_snapshot"}; }

    tests::TestResult run(tests::TestContext& context) override
    {
        const bool ok = containsFrameHeader(context.snapshotRaw());
        return {ok, ok ? "Frame header found in received data." : "AA 55 header was not found."};
    }
};

class LiveReceivesDataTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "live_receives_data"; }
    std::string name() const override { return "Live stream receives new bytes"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {{"timeoutMs", "Timeout ms", "number", 5000.0, 100.0, 60000.0}};
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        const uint64_t startCount = context.rawTotal();
        const uint32_t timeoutMs = static_cast<uint32_t>(context.paramNumber("timeoutMs", 5000.0));

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
        {
            if (context.rawTotal() > startCount)
            {
                return {true, "Live data received."};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        return {false, context.stopRequested() ? "Live test stopped." : "Timed out waiting for live data."};
    }
};

class MotorCommandImuFeedbackTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "motor_command_imu_feedback"; }
    std::string name() const override { return "Motor command with IMU feedback"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live", "received_snapshot"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {
            {"targetEulerZ", "Target Euler Z", "number", 90.0, -360.0, 360.0},
            {"tolerance", "Tolerance", "number", 45.0, 0.0, 360.0},
            {"timeoutMs", "Check window ms", "number", 2000.0, 100.0, 60000.0},
            {"motor1AngleDeg", "Motor 1 angle", "number", 90.0, 0.0, 180.0},
            {"motor2AngleDeg", "Motor 2 angle", "number", 45.0, 0.0, 180.0},
        };
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        const double target = context.paramNumber("targetEulerZ", 90.0);
        const double tolerance = context.paramNumber("tolerance", 45.0);
        const uint32_t timeoutMs = static_cast<uint32_t>(context.paramNumber("timeoutMs", 2000.0));

        if (context.source() == "received_snapshot")
        {
            return checkMessages(context.snapshotParsed("bno055Telemetry"), target, tolerance, "Received snapshot");
        }

        const uint64_t sinceIndex = context.parsedTotal();
        const double motor1 = context.paramNumber("motor1AngleDeg", 90.0);
        const double motor2 = context.paramNumber("motor2AngleDeg", 45.0);

        std::string error;
        if (!context.sendMessage("motor", {{"motor1AngleDeg", motor1}, {"motor2AngleDeg", motor2}}, error))
        {
            return {false, error};
        }
        context.log("Motor command sent, checking BNO055 telemetry window.");
        return checkLiveTelemetryWindow(context, sinceIndex, target, tolerance, timeoutMs);
    }

private:
    static tests::TestResult checkLiveTelemetryWindow(tests::TestContext& context,
                                                      uint64_t sinceIndex,
                                                      double target,
                                                      double tolerance,
                                                      uint32_t timeoutMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        uint64_t nextIndex = sinceIndex;
        std::string lastMismatch = "No BNO055 telemetry received during check window.";

        while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
        {
            const auto now = std::chrono::steady_clock::now();
            const auto remainingMs = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());

            recorder::ParsedMessage message{};
            if (!context.waitForParsed("bno055Telemetry", nextIndex, remainingMs, message))
            {
                break;
            }

            nextIndex = message.index + 1U;
            if (fieldWithin(message, "eulerZ", target, tolerance, lastMismatch))
            {
                return {true, "BNO055 telemetry reached target during check window."};
            }
        }

        return {false, context.stopRequested() ? "Feedback test stopped." : lastMismatch};
    }

    static tests::TestResult checkMessages(const std::vector<recorder::ParsedMessage>& messages,
                                           double target,
                                           double tolerance,
                                           const std::string& label)
    {
        std::string mismatch = "No BNO055 telemetry found.";
        for (const auto& message : messages)
        {
            if (fieldWithin(message, "eulerZ", target, tolerance, mismatch))
            {
                return {true, label + " contains matching BNO055 telemetry."};
            }
        }
        return {false, mismatch};
    }
};
}

namespace tests
{
std::vector<std::unique_ptr<ITestCase>> createBuiltinTests()
{
    std::vector<std::unique_ptr<ITestCase>> tests;
    tests.push_back(std::make_unique<PwmFrameWriteTest>());
    tests.push_back(std::make_unique<MotorFrameWriteTest>());
    tests.push_back(std::make_unique<MotorCommandImuFeedbackTest>());
    tests.push_back(std::make_unique<ReceivedHasDataTest>());
    tests.push_back(std::make_unique<ReceivedContainsFrameHeaderTest>());
    tests.push_back(std::make_unique<LiveReceivesDataTest>());
    return tests;
}
}
