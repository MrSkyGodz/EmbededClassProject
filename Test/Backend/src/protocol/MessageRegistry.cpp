#include "protocol/MessageRegistry.h"

#include <cstring>

namespace
{
constexpr uint8_t IcdType_PWMControl = 0U;
constexpr uint8_t IcdType_MotorControl = 1U;
constexpr uint8_t IcdType_Bno055Telemetry = 2U;
constexpr uint8_t IcdType_ImuReferenceControl = 3U;
constexpr uint8_t IcdType_ImuReferenceTuning = 4U;
constexpr uint8_t IcdType_ImuReferenceStatus = 5U;
constexpr size_t IcdHeaderSize = 6U;

void appendUint32Le(std::vector<uint8_t>& payload, uint32_t value)
{
    payload.push_back(static_cast<uint8_t>(value & 0xFFU));
    payload.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    payload.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    payload.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
}

uint32_t readUint32Le(const uint8_t* payload)
{
    return static_cast<uint32_t>(payload[0]) |
           (static_cast<uint32_t>(payload[1]) << 8U) |
           (static_cast<uint32_t>(payload[2]) << 16U) |
           (static_cast<uint32_t>(payload[3]) << 24U);
}

void appendFloat(std::vector<uint8_t>& payload, float value)
{
    uint8_t raw[sizeof(float)] = {};
    std::memcpy(raw, &value, sizeof(float));
    payload.insert(payload.end(), raw, raw + sizeof(float));
}

float readFloat(const uint8_t* payload)
{
    float value = 0.0F;
    std::memcpy(&value, payload, sizeof(float));
    return value;
}

bool findValue(const std::map<std::string, double>& values, const std::string& name, double& value)
{
    const auto it = values.find(name);
    if (it == values.end())
    {
        return false;
    }

    value = it->second;
    return true;
}
}

namespace protocol
{
MessageRegistry::MessageRegistry()
    : descriptions_{
          {"pwm", IcdType_PWMControl, "command", {{"pwm", "uint8", 0.0, 255.0}}},
          {"motor", IcdType_MotorControl, "command", {{"motor1AngleDeg", "float", 0.0, 180.0}, {"motor2AngleDeg", "float", 0.0, 180.0}}},
          {"bno055Telemetry",
           IcdType_Bno055Telemetry,
           "telemetry",
           {{"eulerX", "float", -360.0, 360.0},
            {"eulerY", "float", -360.0, 360.0},
            {"eulerZ", "float", -360.0, 360.0},
            {"gyroX", "float", -2000.0, 2000.0},
            {"gyroY", "float", -2000.0, 2000.0},
            {"gyroZ", "float", -2000.0, 2000.0}}},
          {"imuReferenceControl",
           IcdType_ImuReferenceControl,
           "command",
           {{"targetAzimuthDeg", "float", 0.0, 360.0},
            {"targetElevationDeg", "float", -180.0, 180.0},
            {"enable", "uint8", 0.0, 1.0},
            {"frameMode", "uint8", 0.0, 1.0}}},
          {"imuReferenceTuning",
           IcdType_ImuReferenceTuning,
           "command",
           {{"azimuthKp", "float", 0.0, 100.0},
            {"azimuthKi", "float", 0.0, 100.0},
            {"elevationKp", "float", 0.0, 100.0},
            {"elevationKi", "float", 0.0, 100.0},
            {"resetIntegrator", "uint8", 0.0, 1.0}}},
          {"imuReferenceStatus",
           IcdType_ImuReferenceStatus,
           "telemetry",
           {{"enable", "uint8", 0.0, 1.0},
            {"frameMode", "uint8", 0.0, 1.0},
            {"targetAzimuthDeg", "float", 0.0, 360.0},
            {"targetElevationDeg", "float", -180.0, 180.0},
            {"currentAzimuthDeg", "float", -360.0, 360.0},
            {"currentElevationDeg", "float", -360.0, 360.0},
            {"azimuthErrorDeg", "float", -360.0, 360.0},
            {"elevationErrorDeg", "float", -360.0, 360.0},
            {"azimuthPiOutputDeg", "float", -360.0, 360.0},
            {"elevationPiOutputDeg", "float", -360.0, 360.0},
            {"motor1AngleDeg", "float", 0.0, 180.0},
            {"motor2AngleDeg", "float", 0.0, 180.0},
            {"motor1TargetAngleDeg", "float", 0.0, 180.0},
            {"motor2TargetAngleDeg", "float", 0.0, 180.0},
            {"reverseBranch", "uint8", 0.0, 1.0}}},
      }
{
}

const std::vector<MessageDescription>& MessageRegistry::descriptions() const
{
    return descriptions_;
}

bool MessageRegistry::buildPayload(const std::string& type,
                                   const std::map<std::string, double>& values,
                                   uint32_t timetagMs,
                                   uint8_t counter,
                                   std::vector<uint8_t>& payload,
                                   std::string& error) const
{
    payload.clear();
    const MessageDescription* description = findByType(type);
    if (description == nullptr)
    {
        error = "unknown message type";
        return false;
    }

    appendUint32Le(payload, timetagMs);
    payload.push_back(counter);
    payload.push_back(description->icdType);

    if (type == "pwm")
    {
        double pwm = 0.0;
        if (!findValue(values, "pwm", pwm) || pwm < 0.0 || pwm > 255.0)
        {
            error = "pwm must be between 0 and 255";
            return false;
        }

        payload.push_back(static_cast<uint8_t>(pwm));
        return true;
    }

    if (type == "motor")
    {
        double motor1 = 0.0;
        double motor2 = 0.0;
        if (!findValue(values, "motor1AngleDeg", motor1))
        {
            error = "motor1AngleDeg is required";
            return false;
        }

        if (!findValue(values, "motor2AngleDeg", motor2))
        {
            motor2 = motor1;
        }

        if (motor1 < 0.0 || motor1 > 180.0 || motor2 < 0.0 || motor2 > 180.0)
        {
            error = "motor angles must be between 0 and 180";
            return false;
        }

        appendFloat(payload, static_cast<float>(motor1));
        appendFloat(payload, static_cast<float>(motor2));
        return true;
    }

    if (type == "imuReferenceControl")
    {
        double targetAzimuth = 0.0;
        double targetElevation = 0.0;
        double enable = 0.0;
        double frameMode = 0.0;
        if (!findValue(values, "targetAzimuthDeg", targetAzimuth) ||
            !findValue(values, "targetElevationDeg", targetElevation) ||
            !findValue(values, "enable", enable) ||
            !findValue(values, "frameMode", frameMode))
        {
            error = "targetAzimuthDeg, targetElevationDeg, enable, and frameMode are required";
            return false;
        }

        if (targetAzimuth < 0.0 || targetAzimuth > 360.0 ||
            targetElevation < -180.0 || targetElevation > 180.0 ||
            enable < 0.0 || enable > 1.0 ||
            frameMode < 0.0 || frameMode > 1.0)
        {
            error = "imuReferenceControl values are out of range";
            return false;
        }

        appendFloat(payload, static_cast<float>(targetAzimuth));
        appendFloat(payload, static_cast<float>(targetElevation));
        payload.push_back(static_cast<uint8_t>(enable));
        payload.push_back(static_cast<uint8_t>(frameMode));
        return true;
    }

    if (type == "imuReferenceTuning")
    {
        double azimuthKp = 0.0;
        double azimuthKi = 0.0;
        double elevationKp = 0.0;
        double elevationKi = 0.0;
        double resetIntegrator = 0.0;
        if (!findValue(values, "azimuthKp", azimuthKp) ||
            !findValue(values, "azimuthKi", azimuthKi) ||
            !findValue(values, "elevationKp", elevationKp) ||
            !findValue(values, "elevationKi", elevationKi) ||
            !findValue(values, "resetIntegrator", resetIntegrator))
        {
            error = "azimuth/elevation PI gains and resetIntegrator are required";
            return false;
        }

        if (azimuthKp < 0.0 || azimuthKp > 100.0 ||
            azimuthKi < 0.0 || azimuthKi > 100.0 ||
            elevationKp < 0.0 || elevationKp > 100.0 ||
            elevationKi < 0.0 || elevationKi > 100.0 ||
            resetIntegrator < 0.0 || resetIntegrator > 1.0)
        {
            error = "imuReferenceTuning values are out of range";
            return false;
        }

        appendFloat(payload, static_cast<float>(azimuthKp));
        appendFloat(payload, static_cast<float>(azimuthKi));
        appendFloat(payload, static_cast<float>(elevationKp));
        appendFloat(payload, static_cast<float>(elevationKi));
        payload.push_back(static_cast<uint8_t>(resetIntegrator));
        return true;
    }

    error = "unknown message type";
    return false;
}

bool MessageRegistry::decodePayload(const uint8_t* payload,
                                    size_t payloadLength,
                                    DecodedMessage& message,
                                    std::string& error) const
{
    if (payloadLength < IcdHeaderSize)
    {
        error = "payload is shorter than ICD header";
        return false;
    }

    message = {};
    message.timetagMs = readUint32Le(payload);
    message.counter = payload[4U];
    message.icdType = payload[5U];

    const MessageDescription* description = findByIcdType(message.icdType);
    if (description == nullptr)
    {
        error = "unknown ICD type";
        return false;
    }

    message.type = description->type;
    const uint8_t* body = payload + IcdHeaderSize;
    const size_t bodyLength = payloadLength - IcdHeaderSize;

    if (message.icdType == IcdType_PWMControl)
    {
        if (bodyLength != 1U)
        {
            error = "invalid pwm body length";
            return false;
        }
        message.values["pwm"] = static_cast<double>(body[0U]);
        return true;
    }

    if (message.icdType == IcdType_MotorControl)
    {
        if (bodyLength != sizeof(float) * 2U)
        {
            error = "invalid motor body length";
            return false;
        }
        message.values["motor1AngleDeg"] = static_cast<double>(readFloat(body));
        message.values["motor2AngleDeg"] = static_cast<double>(readFloat(body + sizeof(float)));
        return true;
    }

    if (message.icdType == IcdType_Bno055Telemetry)
    {
        if (bodyLength != sizeof(float) * 6U)
        {
            error = "invalid bno055 telemetry body length";
            return false;
        }
        message.values["eulerX"] = static_cast<double>(readFloat(body));
        message.values["eulerY"] = static_cast<double>(readFloat(body + sizeof(float)));
        message.values["eulerZ"] = static_cast<double>(readFloat(body + (sizeof(float) * 2U)));
        message.values["gyroX"] = static_cast<double>(readFloat(body + (sizeof(float) * 3U)));
        message.values["gyroY"] = static_cast<double>(readFloat(body + (sizeof(float) * 4U)));
        message.values["gyroZ"] = static_cast<double>(readFloat(body + (sizeof(float) * 5U)));
        return true;
    }

    if (message.icdType == IcdType_ImuReferenceControl)
    {
        if (bodyLength != (sizeof(float) * 2U) + 2U)
        {
            error = "invalid imu reference control body length";
            return false;
        }
        message.values["targetAzimuthDeg"] = static_cast<double>(readFloat(body));
        message.values["targetElevationDeg"] = static_cast<double>(readFloat(body + sizeof(float)));
        message.values["enable"] = static_cast<double>(body[sizeof(float) * 2U]);
        message.values["frameMode"] = static_cast<double>(body[(sizeof(float) * 2U) + 1U]);
        return true;
    }

    if (message.icdType == IcdType_ImuReferenceTuning)
    {
        if (bodyLength != (sizeof(float) * 4U) + 1U)
        {
            error = "invalid imu reference tuning body length";
            return false;
        }
        message.values["azimuthKp"] = static_cast<double>(readFloat(body));
        message.values["azimuthKi"] = static_cast<double>(readFloat(body + sizeof(float)));
        message.values["elevationKp"] = static_cast<double>(readFloat(body + (sizeof(float) * 2U)));
        message.values["elevationKi"] = static_cast<double>(readFloat(body + (sizeof(float) * 3U)));
        message.values["resetIntegrator"] = static_cast<double>(body[sizeof(float) * 4U]);
        return true;
    }

    if (message.icdType == IcdType_ImuReferenceStatus)
    {
        if (bodyLength != (sizeof(float) * 12U) + 3U)
        {
            error = "invalid imu reference status body length";
            return false;
        }
        message.values["enable"] = static_cast<double>(body[0U]);
        message.values["frameMode"] = static_cast<double>(body[1U]);
        message.values["targetAzimuthDeg"] = static_cast<double>(readFloat(body + 2U));
        message.values["targetElevationDeg"] = static_cast<double>(readFloat(body + 2U + sizeof(float)));
        message.values["currentAzimuthDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 2U)));
        message.values["currentElevationDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 3U)));
        message.values["azimuthErrorDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 4U)));
        message.values["elevationErrorDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 5U)));
        message.values["azimuthPiOutputDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 6U)));
        message.values["elevationPiOutputDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 7U)));
        message.values["motor1AngleDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 8U)));
        message.values["motor2AngleDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 9U)));
        message.values["motor1TargetAngleDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 10U)));
        message.values["motor2TargetAngleDeg"] = static_cast<double>(readFloat(body + 2U + (sizeof(float) * 11U)));
        message.values["reverseBranch"] = static_cast<double>(body[2U + (sizeof(float) * 12U)]);
        return true;
    }

    error = "unsupported ICD type";
    return false;
}

const MessageDescription* MessageRegistry::findByType(const std::string& type) const
{
    for (const auto& description : descriptions_)
    {
        if (description.type == type)
        {
            return &description;
        }
    }
    return nullptr;
}

const MessageDescription* MessageRegistry::findByIcdType(uint8_t icdType) const
{
    for (const auto& description : descriptions_)
    {
        if (description.icdType == icdType)
        {
            return &description;
        }
    }
    return nullptr;
}
}
