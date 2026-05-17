#include "protocol/MessageRegistry.h"

#include <cstring>

namespace
{
constexpr uint8_t IcdType_PWMControl = 0U;
constexpr uint8_t IcdType_MotorControl = 1U;
constexpr uint8_t IcdType_Bno055Telemetry = 2U;
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
