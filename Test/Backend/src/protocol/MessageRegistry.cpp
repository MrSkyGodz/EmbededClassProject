#include "protocol/MessageRegistry.h"

#include <cstring>

namespace
{
constexpr uint8_t Header_PWMControl = 0U;
constexpr uint8_t Header_MotorControl = 1U;

void appendFloat(std::vector<uint8_t>& payload, float value)
{
    uint8_t raw[sizeof(float)] = {};
    std::memcpy(raw, &value, sizeof(float));
    payload.insert(payload.end(), raw, raw + sizeof(float));
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
          {"pwm", Header_PWMControl, {{"pwm", "uint8", 0.0, 255.0}}},
          {"motor", Header_MotorControl, {{"motor1AngleDeg", "float", 0.0, 180.0}, {"motor2AngleDeg", "float", 0.0, 180.0}}},
      }
{
}

const std::vector<MessageDescription>& MessageRegistry::descriptions() const
{
    return descriptions_;
}

bool MessageRegistry::buildPayload(const std::string& type,
                                   const std::map<std::string, double>& values,
                                   std::vector<uint8_t>& payload,
                                   std::string& error) const
{
    payload.clear();

    if (type == "pwm")
    {
        double pwm = 0.0;
        if (!findValue(values, "pwm", pwm) || pwm < 0.0 || pwm > 255.0)
        {
            error = "pwm must be between 0 and 255";
            return false;
        }

        payload.push_back(Header_PWMControl);
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

        payload.push_back(Header_MotorControl);
        appendFloat(payload, static_cast<float>(motor1));
        appendFloat(payload, static_cast<float>(motor2));
        return true;
    }

    error = "unknown message type";
    return false;
}
}
