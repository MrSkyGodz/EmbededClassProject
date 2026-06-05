#include "tests/BuiltinTests.h"

#include "tests/TestContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
struct MotorPosition
{
    double motor1;
    double motor2;
};

struct OrientationTarget
{
    double azimuthDeg;
    double elevationDeg;
};

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

double wrap180(double angleDeg)
{
    while (angleDeg > 180.0)
    {
        angleDeg -= 360.0;
    }
    while (angleDeg < -180.0)
    {
        angleDeg += 360.0;
    }
    return angleDeg;
}

uint32_t remainingMsUntil(const std::chrono::steady_clock::time_point& deadline)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline)
    {
        return 0U;
    }

    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<uint32_t>(std::max<int64_t>(remaining, 1));
}

bool sleepWithStop(tests::TestContext& context, uint32_t durationMs)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
    while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    return !context.stopRequested();
}

std::string motorChannelName(tests::TestContext& context)
{
    return context.channel("motor").exists() ? "motor" : channel::DefaultChannelName;
}

std::string controlChannelName(tests::TestContext& context)
{
    return context.channel("control").exists() ? "control" : channel::DefaultChannelName;
}

std::string feedbackChannelName(tests::TestContext& context)
{
    return context.channel("imu").exists() ? "imu" : channel::DefaultChannelName;
}

bool sendMotorCommand(tests::TestChannel& channel, double motor1, double motor2, std::string& error)
{
    return channel.sendMessage("motor", {{"motor1AngleDeg", motor1}, {"motor2AngleDeg", motor2}}, error);
}

bool sendImuReferenceControl(tests::TestChannel& channel,
                             double targetAzimuthDeg,
                             double targetElevationDeg,
                             double enable,
                             double frameMode,
                             std::string& error)
{
    return channel.sendMessage("imuReferenceControl",
                               {{"targetAzimuthDeg", targetAzimuthDeg},
                                {"targetElevationDeg", targetElevationDeg},
                                {"enable", enable},
                                {"frameMode", frameMode}},
                               error);
}

bool sendImuReferenceTuning(tests::TestChannel& channel,
                            double azimuthKp,
                            double elevationKp,
                            std::string& error)
{
    return channel.sendMessage("imuReferenceTuning",
                               {{"azimuthKp", azimuthKp},
                                {"elevationKp", elevationKp}},
                               error);
}

bool valueOf(const recorder::ParsedMessage& message, const std::string& fieldName, double& value, std::string& error)
{
    const auto it = message.values.find(fieldName);
    if (it == message.values.end())
    {
        error = "Expected field was not found: " + fieldName;
        return false;
    }

    value = it->second;
    return true;
}

bool calibrationComplete(const recorder::ParsedMessage& message)
{
    const auto system = message.values.find("system");
    const auto gyro = message.values.find("gyro");
    const auto acc = message.values.find("acc");
    const auto mag = message.values.find("mag");
    const auto fullyCalibrated = message.values.find("fullyCalibrated");

    return (system != message.values.end()) &&
           (gyro != message.values.end()) &&
           (acc != message.values.end()) &&
           (mag != message.values.end()) &&
           (fullyCalibrated != message.values.end()) &&
           (system->second >= 3.0) &&
           (gyro->second >= 3.0) &&
           (acc->second >= 3.0) &&
           (mag->second >= 3.0) &&
           (fullyCalibrated->second >= 1.0);
}

bool calibrationFieldComplete(const recorder::ParsedMessage& message, const std::string& fieldName)
{
    if (fieldName == "fullyCalibrated")
    {
        return calibrationComplete(message);
    }

    const auto it = message.values.find(fieldName);
    return (it != message.values.end()) && (it->second >= 3.0);
}

bool calibrationFieldsComplete(const recorder::ParsedMessage& message, const std::vector<std::string>& fieldNames)
{
    for (const auto& fieldName : fieldNames)
    {
        if (!calibrationFieldComplete(message, fieldName))
        {
            return false;
        }
    }

    return true;
}

std::string calibrationSummary(const recorder::ParsedMessage& message)
{
    std::ostringstream out;
    out << "system=" << message.values.at("system")
        << " gyro=" << message.values.at("gyro")
        << " acc=" << message.values.at("acc")
        << " mag=" << message.values.at("mag")
        << " fullyCalibrated=" << message.values.at("fullyCalibrated");
    return out.str();
}

bool waitForCalibrationFieldDuring(tests::TestContext& context,
                                   tests::TestChannel& feedbackChannel,
                                   uint64_t& nextIndex,
                                   uint32_t durationMs,
                                   const std::string& fieldName,
                                   recorder::ParsedMessage& lastCalibration,
                                   bool& sawCalibration)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
    while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
    {
        const uint32_t waitMs = std::min<uint32_t>(remainingMsUntil(deadline), 100U);
        recorder::ParsedMessage message{};
        if (!feedbackChannel.waitForParsed("bno055CalibrationStatus", nextIndex, waitMs, message))
        {
            continue;
        }

        nextIndex = message.index + 1U;
        lastCalibration = message;
        sawCalibration = true;
        if (calibrationFieldComplete(message, fieldName))
        {
            return true;
        }
    }

    return false;
}

bool waitForStableCalibrationFields(tests::TestContext& context,
                                    tests::TestChannel& feedbackChannel,
                                    uint64_t& nextIndex,
                                    uint32_t durationMs,
                                    uint32_t requiredStableMs,
                                    const std::vector<std::string>& fieldNames,
                                    recorder::ParsedMessage& lastCalibration,
                                    bool& sawCalibration)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
    auto stableStart = std::chrono::steady_clock::time_point{};

    while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
    {
        const uint32_t waitMs = std::min<uint32_t>(remainingMsUntil(deadline), 100U);
        recorder::ParsedMessage message{};
        if (!feedbackChannel.waitForParsed("bno055CalibrationStatus", nextIndex, waitMs, message))
        {
            continue;
        }

        nextIndex = message.index + 1U;
        lastCalibration = message;
        sawCalibration = true;

        if (!calibrationFieldsComplete(message, fieldNames))
        {
            stableStart = std::chrono::steady_clock::time_point{};
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        if (stableStart == std::chrono::steady_clock::time_point{})
        {
            stableStart = now;
        }

        const auto stableMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - stableStart).count();
        if (stableMs >= static_cast<int64_t>(requiredStableMs))
        {
            return true;
        }
    }

    return false;
}

bool orientationTargetReached(const recorder::ParsedMessage& message,
                              double targetAzimuthDeg,
                              double targetElevationDeg,
                              double frameMode,
                              double toleranceDeg)
{
    const auto targetAzimuth = message.values.find("targetAzimuthDeg");
    const auto targetElevation = message.values.find("targetElevationDeg");
    const auto enable = message.values.find("enable");
    const auto statusFrameMode = message.values.find("frameMode");
    const auto azimuthError = message.values.find("azimuthErrorDeg");
    const auto elevationError = message.values.find("elevationErrorDeg");

    if (targetAzimuth == message.values.end() ||
        targetElevation == message.values.end() ||
        enable == message.values.end() ||
        statusFrameMode == message.values.end() ||
        azimuthError == message.values.end() ||
        elevationError == message.values.end())
    {
        return false;
    }

    return (enable->second >= 1.0) &&
           (std::fabs(statusFrameMode->second - frameMode) <= 0.5) &&
           (std::fabs(wrap180(targetAzimuth->second - targetAzimuthDeg)) <= 0.5) &&
           (std::fabs(targetElevation->second - targetElevationDeg) <= 0.5) &&
           (std::fabs(azimuthError->second) <= toleranceDeg) &&
           (std::fabs(elevationError->second) <= toleranceDeg);
}

std::string orientationSummary(const recorder::ParsedMessage& message)
{
    std::ostringstream out;
    const auto append = [&](const char* name) {
        const auto it = message.values.find(name);
        if (it != message.values.end())
        {
            out << " " << name << "=" << it->second;
        }
    };

    append("targetAzimuthDeg");
    append("targetElevationDeg");
    append("currentAzimuthDeg");
    append("currentElevationDeg");
    append("azimuthErrorDeg");
    append("elevationErrorDeg");
    append("motor1AngleDeg");
    append("motor2AngleDeg");
    return out.str();
}

bool waitForStableOrientationTarget(tests::TestContext& context,
                                    tests::TestChannel& feedbackChannel,
                                    uint64_t& nextIndex,
                                    double targetAzimuthDeg,
                                    double targetElevationDeg,
                                    double frameMode,
                                    double toleranceDeg,
                                    uint32_t timeoutMs,
                                    uint32_t requiredStableMs,
                                    recorder::ParsedMessage& lastStatus,
                                    bool& sawStatus)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    auto stableStart = std::chrono::steady_clock::time_point{};

    while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
    {
        const uint32_t waitMs = std::min<uint32_t>(remainingMsUntil(deadline), 100U);
        recorder::ParsedMessage message{};
        if (!feedbackChannel.waitForParsed("imuReferenceStatus", nextIndex, waitMs, message))
        {
            continue;
        }

        nextIndex = message.index + 1U;
        lastStatus = message;
        sawStatus = true;

        if (!orientationTargetReached(message, targetAzimuthDeg, targetElevationDeg, frameMode, toleranceDeg))
        {
            stableStart = std::chrono::steady_clock::time_point{};
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        if (stableStart == std::chrono::steady_clock::time_point{})
        {
            stableStart = now;
        }

        const auto stableMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - stableStart).count();
        if (stableMs >= static_cast<int64_t>(requiredStableMs))
        {
            return true;
        }
    }

    return false;
}

bool waitForLatestParsed(tests::TestContext& context,
                         tests::TestChannel& feedbackChannel,
                         const std::string& type,
                         uint64_t& nextIndex,
                         uint32_t windowMs,
                         recorder::ParsedMessage& latest,
                         std::string& error)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(windowMs);
    bool found = false;

    while (!context.stopRequested() && std::chrono::steady_clock::now() < deadline)
    {
        const uint32_t waitMs = std::min<uint32_t>(remainingMsUntil(deadline), 100U);
        recorder::ParsedMessage message{};
        if (!feedbackChannel.waitForParsed(type, nextIndex, waitMs, message))
        {
            continue;
        }

        nextIndex = message.index + 1U;
        latest = message;
        found = true;
    }

    if (!found)
    {
        error = "No " + type + " telemetry received during sample window.";
    }

    return found;
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

class ImuCalibrationSweepTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "imu_calibration_sweep"; }
    std::string name() const override { return "IMU gyro/mag calibration profile"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {
            {"holdMs", "Static hold ms", "number", 4000.0, 500.0, 20000.0},
            {"moveSettleMs", "Move settle ms", "number", 800.0, 100.0, 10000.0},
            {"timeoutMs", "Timeout ms", "number", 120000.0, 10000.0, 300000.0},
            {"repeatCount", "Repeat count", "number", 4.0, 1.0, 20.0},
            {"stableGyroMagMs", "Stable gyro/mag ms", "number", 2000.0, 500.0, 20000.0},
        };
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        constexpr std::array<MotorPosition, 9U> gyroPositions = {{
            {90.0, 90.0},
            {90.0, 0.0},
            {90.0, 180.0},
            {0.0, 90.0},
            {180.0, 90.0},
            {0.0, 0.0},
            {180.0, 180.0},
            {0.0, 180.0},
            {180.0, 0.0},
        }};
        constexpr std::array<MotorPosition, 33U> magPositions = {{
            {90.0, 90.0},
            {75.0, 104.0},
            {60.0, 115.0},
            {45.0, 124.0},
            {30.0, 130.0},
            {15.0, 124.0},
            {0.0, 115.0},
            {15.0, 102.0},
            {30.0, 90.0},
            {45.0, 76.0},
            {60.0, 65.0},
            {75.0, 56.0},
            {90.0, 50.0},
            {105.0, 56.0},
            {120.0, 65.0},
            {135.0, 76.0},
            {150.0, 90.0},
            {165.0, 102.0},
            {180.0, 115.0},
            {165.0, 124.0},
            {150.0, 130.0},
            {135.0, 124.0},
            {120.0, 115.0},
            {105.0, 102.0},
            {90.0, 90.0},
            {75.0, 76.0},
            {60.0, 65.0},
            {45.0, 56.0},
            {30.0, 50.0},
            {45.0, 62.0},
            {60.0, 75.0},
            {75.0, 84.0},
            {90.0, 90.0},
        }};

        const uint32_t holdMs = static_cast<uint32_t>(context.paramNumber("holdMs", 4000.0));
        const uint32_t moveSettleMs = static_cast<uint32_t>(context.paramNumber("moveSettleMs", 800.0));
        const uint32_t timeoutMs = static_cast<uint32_t>(context.paramNumber("timeoutMs", 120000.0));
        const uint32_t repeatCount = static_cast<uint32_t>(context.paramNumber("repeatCount", 4.0));
        const uint32_t stableGyroMagMs = static_cast<uint32_t>(context.paramNumber("stableGyroMagMs", 2000.0));

        auto commandChannel = context.channel(motorChannelName(context));
        auto feedbackChannel = context.channel(feedbackChannelName(context));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        uint64_t nextIndex = feedbackChannel.parsedTotal();
        recorder::ParsedMessage lastCalibration{};
        bool sawCalibration = false;
        std::string stageError;

        const auto runStage = [&](const char* label,
                                  const std::string& fieldName,
                                  const auto& positions,
                                  uint32_t stageRepeatCount,
                                  uint32_t perPositionHoldMs) -> bool {
            context.log(std::string("Calibration stage started: ") + label + ".");
            for (uint32_t repeat = 0U; repeat < stageRepeatCount && std::chrono::steady_clock::now() < deadline; ++repeat)
            {
                for (const auto& position : positions)
                {
                    if (context.stopRequested() || std::chrono::steady_clock::now() >= deadline)
                    {
                        return false;
                    }

                    std::string error;
                    if (!sendMotorCommand(commandChannel, position.motor1, position.motor2, error))
                    {
                        stageError = error;
                        return false;
                    }

                    std::ostringstream log;
                    log << "Calibration " << label << " motor=(" << position.motor1 << ", " << position.motor2 << ").";
                    context.log(log.str());

                    if (!sleepWithStop(context, std::min<uint32_t>(moveSettleMs, remainingMsUntil(deadline))))
                    {
                        return false;
                    }

                    const uint32_t waitMs = std::min<uint32_t>(perPositionHoldMs, remainingMsUntil(deadline));
                    if (waitForCalibrationFieldDuring(context,
                                                      feedbackChannel,
                                                      nextIndex,
                                                      waitMs,
                                                      fieldName,
                                                      lastCalibration,
                                                      sawCalibration))
                    {
                        context.log(std::string("Calibration stage completed: ") + label + " " +
                                    calibrationSummary(lastCalibration));
                        return true;
                    }
                }
            }
            return false;
        };

        (void) runStage("gyro static", "gyro", gyroPositions, 1U, holdMs);
        if (!stageError.empty())
        {
            return {false, stageError};
        }
        if (context.stopRequested())
        {
            return {false, "Calibration profile stopped."};
        }

        (void) runStage("mag figure-eight sweep", "mag", magPositions, repeatCount, std::max<uint32_t>(holdMs / 3U, 700U));
        if (!stageError.empty())
        {
            return {false, stageError};
        }
        if (context.stopRequested())
        {
            return {false, "Calibration profile stopped."};
        }

        for (uint32_t repeat = 0U; repeat < repeatCount && std::chrono::steady_clock::now() < deadline; ++repeat)
        {
            for (const auto& position : magPositions)
            {
                if (context.stopRequested() || std::chrono::steady_clock::now() >= deadline)
                {
                    break;
                }

                std::string error;
                if (!sendMotorCommand(commandChannel, position.motor1, position.motor2, error))
                {
                    return {false, error};
                }

                if (!sleepWithStop(context, std::min<uint32_t>(moveSettleMs, remainingMsUntil(deadline))))
                {
                    return {false, "Calibration profile stopped."};
                }

                if (waitForStableCalibrationFields(context,
                                                   feedbackChannel,
                                                   nextIndex,
                                                   std::min<uint32_t>(holdMs, remainingMsUntil(deadline)),
                                                   stableGyroMagMs,
                                                   {"gyro", "mag"},
                                                   lastCalibration,
                                                   sawCalibration))
                {
                    return {true, "BNO055 gyro/mag calibration completed and stable: " +
                                  calibrationSummary(lastCalibration)};
                }
            }
        }

        if (context.stopRequested())
        {
            return {false, "Calibration profile stopped."};
        }

        if (!sawCalibration)
        {
            return {false, "No bno055CalibrationStatus telemetry received during calibration sweep."};
        }

        return {false, "Calibration profile did not reach stable gyro/mag calibration. Last status: " +
                       calibrationSummary(lastCalibration)};
    }
};

class ImuManualAccSystemCalibrationTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "imu_manual_acc_system_calibration"; }
    std::string name() const override { return "Manual IMU acc/system calibration"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {
            {"instructionHoldMs", "Instruction hold ms", "number", 8000.0, 1000.0, 30000.0},
            {"timeoutMs", "Timeout ms", "number", 180000.0, 10000.0, 300000.0},
            {"stableSystemMs", "Stable system ms", "number", 2000.0, 500.0, 20000.0},
        };
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        constexpr std::array<const char*, 6U> accInstructions = {{
            "ACC: Place the IMU level and keep it still.",
            "ACC: Rotate the whole unit so the nose/front points up; keep it still.",
            "ACC: Rotate the whole unit so the nose/front points down; keep it still.",
            "ACC: Put the unit on its left side; keep it still.",
            "ACC: Put the unit on its right side; keep it still.",
            "ACC: Turn the unit upside down; keep it still.",
        }};
        constexpr std::array<const char*, 4U> systemInstructions = {{
            "SYSTEM: Slowly rotate the whole unit around yaw while keeping clear of magnetic objects.",
            "SYSTEM: Tilt the whole unit forward and backward slowly.",
            "SYSTEM: Tilt the whole unit left and right slowly.",
            "SYSTEM: Return to normal mounting orientation and keep it still.",
        }};

        const uint32_t instructionHoldMs = static_cast<uint32_t>(context.paramNumber("instructionHoldMs", 8000.0));
        const uint32_t timeoutMs = static_cast<uint32_t>(context.paramNumber("timeoutMs", 180000.0));
        const uint32_t stableSystemMs = static_cast<uint32_t>(context.paramNumber("stableSystemMs", 2000.0));

        auto feedbackChannel = context.channel(feedbackChannelName(context));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        uint64_t nextIndex = feedbackChannel.parsedTotal();
        recorder::ParsedMessage lastCalibration{};
        bool sawCalibration = false;

        for (const char* instruction : accInstructions)
        {
            if (context.stopRequested() || std::chrono::steady_clock::now() >= deadline)
            {
                break;
            }

            context.log(instruction);
            if (waitForCalibrationFieldDuring(context,
                                              feedbackChannel,
                                              nextIndex,
                                              std::min<uint32_t>(instructionHoldMs, remainingMsUntil(deadline)),
                                              "acc",
                                              lastCalibration,
                                              sawCalibration))
            {
                context.log("ACC calibration completed: " + calibrationSummary(lastCalibration));
                break;
            }
        }

        if (context.stopRequested())
        {
            return {false, "Manual calibration stopped."};
        }

        if (!calibrationFieldComplete(lastCalibration, "acc"))
        {
            if (!sawCalibration)
            {
                return {false, "No bno055CalibrationStatus telemetry received during manual calibration."};
            }
            return {false, "ACC calibration did not reach 3. Last status: " + calibrationSummary(lastCalibration)};
        }

        for (const char* instruction : systemInstructions)
        {
            if (context.stopRequested() || std::chrono::steady_clock::now() >= deadline)
            {
                break;
            }

            context.log(instruction);
            if (waitForStableCalibrationFields(context,
                                               feedbackChannel,
                                               nextIndex,
                                               std::min<uint32_t>(instructionHoldMs, remainingMsUntil(deadline)),
                                               stableSystemMs,
                                               {"system"},
                                               lastCalibration,
                                               sawCalibration))
            {
                return {true, "SYSTEM calibration reached 3 and stayed stable: " +
                              calibrationSummary(lastCalibration)};
            }
        }

        if (context.stopRequested())
        {
            return {false, "Manual calibration stopped."};
        }

        return {false, "SYSTEM calibration did not reach stable 3. Last status: " +
                       calibrationSummary(lastCalibration)};
    }
};

class ImuReferenceOrientationTargetsTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "imu_reference_orientation_targets"; }
    std::string name() const override { return "IMU reference orientation targets"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {
            {"frameMode", "Frame mode", "number", 1.0, 0.0, 1.0},
            {"azimuthKp", "Azimuth Kp", "number", 0.2, 0.0, 100.0},
            {"elevationKp", "Elevation Kp", "number", 0.2, 0.0, 100.0},
            {"toleranceDeg", "Tolerance deg", "number", 8.0, 0.5, 45.0},
            {"targetTimeoutMs", "Target timeout ms", "number", 20000.0, 1000.0, 120000.0},
            {"stableMs", "Stable ms", "number", 1000.0, 100.0, 10000.0},
            {"target1AzimuthDeg", "Target 1 azimuth", "number", 0.0, 0.0, 360.0},
            {"target1ElevationDeg", "Target 1 elevation", "number", 0.0, -180.0, 180.0},
            {"target2AzimuthDeg", "Target 2 azimuth", "number", 50.0, 0.0, 360.0},
            {"target2ElevationDeg", "Target 2 elevation", "number", 30.0, -180.0, 180.0},
            {"target3AzimuthDeg", "Target 3 azimuth", "number", 120.0, 0.0, 360.0},
            {"target3ElevationDeg", "Target 3 elevation", "number", 60.0, -180.0, 180.0},
            {"disableAfter", "Disable after", "number", 1.0, 0.0, 1.0},
        };
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        const double frameMode = std::round(context.paramNumber("frameMode", 1.0));
        const double azimuthKp = context.paramNumber("azimuthKp", 0.2);
        const double elevationKp = context.paramNumber("elevationKp", 0.2);
        const double toleranceDeg = context.paramNumber("toleranceDeg", 8.0);
        const uint32_t targetTimeoutMs = static_cast<uint32_t>(context.paramNumber("targetTimeoutMs", 20000.0));
        const uint32_t stableMs = static_cast<uint32_t>(context.paramNumber("stableMs", 1000.0));
        const bool disableAfter = context.paramNumber("disableAfter", 1.0) >= 0.5;
        const std::array<OrientationTarget, 3U> targets = {{
            {context.paramNumber("target1AzimuthDeg", 0.0), context.paramNumber("target1ElevationDeg", 0.0)},
            {context.paramNumber("target2AzimuthDeg", 50.0), context.paramNumber("target2ElevationDeg", 30.0)},
            {context.paramNumber("target3AzimuthDeg", 120.0), context.paramNumber("target3ElevationDeg", 60.0)},
        }};

        auto commandChannel = context.channel(controlChannelName(context));
        auto feedbackChannel = context.channel(feedbackChannelName(context));
        uint64_t nextIndex = feedbackChannel.parsedTotal();
        std::string error;

        if (!sendImuReferenceTuning(commandChannel, azimuthKp, elevationKp, error))
        {
            return {false, error};
        }

        recorder::ParsedMessage lastStatus{};
        bool sawStatus = false;
        size_t targetIndex = 0U;

        for (const auto& target : targets)
        {
            ++targetIndex;
            if (context.stopRequested())
            {
                disableReferenceIfRequested(commandChannel, disableAfter, frameMode);
                return {false, "Orientation target test stopped."};
            }

            if (!sendImuReferenceControl(commandChannel,
                                         target.azimuthDeg,
                                         target.elevationDeg,
                                         1.0,
                                         frameMode,
                                         error))
            {
                disableReferenceIfRequested(commandChannel, disableAfter, frameMode);
                return {false, error};
            }

            std::ostringstream log;
            log << "IMU reference target " << targetIndex << " az=" << target.azimuthDeg
                << " el=" << target.elevationDeg << ".";
            context.log(log.str());

            if (!waitForStableOrientationTarget(context,
                                                feedbackChannel,
                                                nextIndex,
                                                target.azimuthDeg,
                                                target.elevationDeg,
                                                frameMode,
                                                toleranceDeg,
                                                targetTimeoutMs,
                                                stableMs,
                                                lastStatus,
                                                sawStatus))
            {
                disableReferenceIfRequested(commandChannel, disableAfter, frameMode);
                if (!sawStatus)
                {
                    return {false, "No imuReferenceStatus telemetry received during orientation target test."};
                }

                std::ostringstream message;
                message << "Target " << targetIndex << " did not settle within tolerance=" << toleranceDeg
                        << "." << orientationSummary(lastStatus);
                return {false, message.str()};
            }
        }

        disableReferenceIfRequested(commandChannel, disableAfter, frameMode);

        std::ostringstream message;
        message << targets.size() << " IMU reference targets reached within tolerance=" << toleranceDeg
                << "." << orientationSummary(lastStatus);
        return {true, message.str()};
    }

private:
    static void disableReferenceIfRequested(tests::TestChannel& commandChannel, bool disableAfter, double frameMode)
    {
        if (!disableAfter)
        {
            return;
        }

        std::string ignoredError;
        (void) sendImuReferenceControl(commandChannel, 0.0, 0.0, 0.0, frameMode, ignoredError);
    }
};

class ImuAxisLimitRotationTest final : public tests::ITestCase
{
public:
    std::string id() const override { return "imu_axis_limit_rotation"; }
    std::string name() const override { return "IMU axis limit rotation check"; }
    std::string defaultSource() const override { return "received_live"; }
    std::vector<std::string> allowedSources() const override { return {"received_live"}; }

    std::vector<tests::TestParameter> parameters() const override
    {
        return {
            {"axis", "Axis 0 az 1 el 2 both", "number", 2.0, 0.0, 2.0},
            {"minDeg", "Min command deg", "number", 0.0, 0.0, 180.0},
            {"maxDeg", "Max command deg", "number", 180.0, 0.0, 180.0},
            {"holdOtherMotorDeg", "Hold other motor deg", "number", 90.0, 0.0, 180.0},
            {"settleMs", "Settle ms", "number", 1500.0, 100.0, 10000.0},
            {"sampleWindowMs", "Sample window ms", "number", 1500.0, 100.0, 10000.0},
            {"toleranceDeg", "Tolerance deg", "number", 20.0, 0.0, 90.0},
        };
    }

    tests::TestResult run(tests::TestContext& context) override
    {
        const int axis = static_cast<int>(std::round(context.paramNumber("axis", 2.0)));
        if (axis != 0 && axis != 1 && axis != 2)
        {
            return {false, "axis must be 0 for azimuth, 1 for elevation, or 2 for both."};
        }

        const double minDeg = context.paramNumber("minDeg", 0.0);
        const double maxDeg = context.paramNumber("maxDeg", 180.0);
        const double holdOtherMotorDeg = context.paramNumber("holdOtherMotorDeg", 90.0);
        const uint32_t settleMs = static_cast<uint32_t>(context.paramNumber("settleMs", 1500.0));
        const uint32_t sampleWindowMs = static_cast<uint32_t>(context.paramNumber("sampleWindowMs", 1500.0));
        const double toleranceDeg = context.paramNumber("toleranceDeg", 20.0);

        auto commandChannel = context.channel(motorChannelName(context));
        auto feedbackChannel = context.channel(feedbackChannelName(context));
        uint64_t nextIndex = feedbackChannel.parsedTotal();

        if (axis == 0 || axis == 1)
        {
            return runAxisCheck(context,
                                commandChannel,
                                feedbackChannel,
                                nextIndex,
                                axis,
                                minDeg,
                                maxDeg,
                                holdOtherMotorDeg,
                                settleMs,
                                sampleWindowMs,
                                toleranceDeg);
        }

        const auto azimuthResult = runAxisCheck(context,
                                               commandChannel,
                                               feedbackChannel,
                                               nextIndex,
                                               0,
                                               minDeg,
                                               maxDeg,
                                               holdOtherMotorDeg,
                                               settleMs,
                                               sampleWindowMs,
                                               toleranceDeg);
        if (!azimuthResult.passed)
        {
            return {false, "Azimuth failed: " + azimuthResult.message};
        }
        if (context.stopRequested())
        {
            return {false, "Limit test stopped."};
        }

        const auto elevationResult = runAxisCheck(context,
                                                 commandChannel,
                                                 feedbackChannel,
                                                 nextIndex,
                                                 1,
                                                 minDeg,
                                                 maxDeg,
                                                 holdOtherMotorDeg,
                                                 settleMs,
                                                 sampleWindowMs,
                                                 toleranceDeg);
        if (!elevationResult.passed)
        {
            return {false, "Elevation failed: " + elevationResult.message};
        }

        return {true, azimuthResult.message + " | " + elevationResult.message};
    }

private:
    static tests::TestResult runAxisCheck(tests::TestContext& context,
                                          tests::TestChannel& commandChannel,
                                          tests::TestChannel& feedbackChannel,
                                          uint64_t& nextIndex,
                                          int axis,
                                          double minDeg,
                                          double maxDeg,
                                          double holdOtherMotorDeg,
                                          uint32_t settleMs,
                                          uint32_t sampleWindowMs,
                                          double toleranceDeg)
    {
        recorder::ParsedMessage startMessage{};
        recorder::ParsedMessage endMessage{};
        std::string error;

        if (!moveAndSample(context,
                           commandChannel,
                           feedbackChannel,
                           nextIndex,
                           axis,
                           minDeg,
                           holdOtherMotorDeg,
                           settleMs,
                           sampleWindowMs,
                           startMessage,
                           error))
        {
            return {false, error};
        }

        if (!moveAndSample(context,
                           commandChannel,
                           feedbackChannel,
                           nextIndex,
                           axis,
                           maxDeg,
                           holdOtherMotorDeg,
                           settleMs,
                           sampleWindowMs,
                           endMessage,
                           error))
        {
            return {false, error};
        }

        const std::string fieldName = axis == 0 ? "eulerX" : "eulerZ";
        double startValue = 0.0;
        double endValue = 0.0;
        if (!valueOf(startMessage, fieldName, startValue, error) || !valueOf(endMessage, fieldName, endValue, error))
        {
            return {false, error};
        }

        const double expectedRotation = std::fabs(maxDeg - minDeg);
        const double measuredRotation = std::fabs(wrap180(endValue - startValue));
        const double mismatch = std::fabs(measuredRotation - expectedRotation);

        std::ostringstream result;
        result << (axis == 0 ? "Azimuth" : "Elevation")
               << " limit rotation start=" << startValue
               << " end=" << endValue
               << " measured=" << measuredRotation
               << " expected=" << expectedRotation
               << " tolerance=" << toleranceDeg;

        return {mismatch <= toleranceDeg, result.str()};
    }

    static bool moveAndSample(tests::TestContext& context,
                              tests::TestChannel& commandChannel,
                              tests::TestChannel& feedbackChannel,
                              uint64_t& nextIndex,
                              int axis,
                              double drivenMotorDeg,
                              double holdOtherMotorDeg,
                              uint32_t settleMs,
                              uint32_t sampleWindowMs,
                              recorder::ParsedMessage& sample,
                              std::string& error)
    {
        const double motor1 = axis == 0 ? drivenMotorDeg : holdOtherMotorDeg;
        const double motor2 = axis == 0 ? holdOtherMotorDeg : drivenMotorDeg;

        if (!sendMotorCommand(commandChannel, motor1, motor2, error))
        {
            return false;
        }

        std::ostringstream log;
        log << "Limit test motor=(" << motor1 << ", " << motor2 << ").";
        context.log(log.str());

        if (!sleepWithStop(context, settleMs))
        {
            error = "Limit test stopped.";
            return false;
        }

        return waitForLatestParsed(context,
                                   feedbackChannel,
                                   "bno055Telemetry",
                                   nextIndex,
                                   sampleWindowMs,
                                   sample,
                                   error);
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
}

namespace tests
{
std::vector<std::unique_ptr<ITestCase>> createBuiltinTests()
{
    std::vector<std::unique_ptr<ITestCase>> tests;
    tests.push_back(std::make_unique<PwmFrameWriteTest>());
    tests.push_back(std::make_unique<MotorFrameWriteTest>());
    tests.push_back(std::make_unique<ImuCalibrationSweepTest>());
    tests.push_back(std::make_unique<ImuManualAccSystemCalibrationTest>());
    tests.push_back(std::make_unique<ImuReferenceOrientationTargetsTest>());
    tests.push_back(std::make_unique<ImuAxisLimitRotationTest>());
    tests.push_back(std::make_unique<ReceivedHasDataTest>());
    tests.push_back(std::make_unique<ReceivedContainsFrameHeaderTest>());
    tests.push_back(std::make_unique<LiveReceivesDataTest>());
    return tests;
}
}
