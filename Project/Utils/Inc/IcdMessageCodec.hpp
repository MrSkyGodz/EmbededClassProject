#pragma once

#include "IcdFrameBuilder.hpp"

namespace IcdMessageCodec
{
inline uint8_t BuildPwmControlPayload(uint32_t timetagMs,
                                      uint8_t counter,
                                      const PWMControl_t& message,
                                      uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_PWMControl);
	payload[offset++] = message.Pwm;
	return offset;
}

inline uint8_t BuildMotorControlPayload(uint32_t timetagMs,
                                        uint8_t counter,
                                        const MotorControl_t& message,
                                        uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_MotorControl);
	IcdProtocol::AppendFloat(payload, &offset, message.Motor1AngleDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.Motor2AngleDeg);
	return offset;
}

inline uint8_t BuildBno055TelemetryPayload(uint32_t timetagMs,
                                           uint8_t counter,
                                           const Bno055Telemetry_t& message,
                                           uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_Bno055Telemetry);
	IcdProtocol::AppendFloat(payload, &offset, message.EulerX);
	IcdProtocol::AppendFloat(payload, &offset, message.EulerY);
	IcdProtocol::AppendFloat(payload, &offset, message.EulerZ);
	IcdProtocol::AppendFloat(payload, &offset, message.GyroX);
	IcdProtocol::AppendFloat(payload, &offset, message.GyroY);
	IcdProtocol::AppendFloat(payload, &offset, message.GyroZ);
	return offset;
}

inline uint8_t BuildBno055CalibrationStatusPayload(uint32_t timetagMs,
                                                   uint8_t counter,
                                                   const Bno055CalibrationStatus_t& message,
                                                   uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_Bno055CalibrationStatus);
	payload[offset++] = message.System;
	payload[offset++] = message.Gyro;
	payload[offset++] = message.Acc;
	payload[offset++] = message.Mag;
	payload[offset++] = message.FullyCalibrated;
	return offset;
}

inline uint8_t BuildImuReferenceControlPayload(uint32_t timetagMs,
                                               uint8_t counter,
                                               const ImuReferenceControl_t& message,
                                               uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_ImuReferenceControl);
	IcdProtocol::AppendFloat(payload, &offset, message.TargetAzimuthDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.TargetElevationDeg);
	payload[offset++] = message.Enable;
	payload[offset++] = message.FrameMode;
	return offset;
}

inline uint8_t BuildImuReferenceTuningPayload(uint32_t timetagMs,
                                              uint8_t counter,
                                              const ImuReferenceTuning_t& message,
                                              uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_ImuReferenceTuning);
	IcdProtocol::AppendFloat(payload, &offset, message.AzimuthKp);
	IcdProtocol::AppendFloat(payload, &offset, message.AzimuthKi);
	IcdProtocol::AppendFloat(payload, &offset, message.ElevationKp);
	IcdProtocol::AppendFloat(payload, &offset, message.ElevationKi);
	payload[offset++] = message.ResetIntegrator;
	return offset;
}

inline uint8_t BuildImuReferenceStatusPayload(uint32_t timetagMs,
                                              uint8_t counter,
                                              const ImuReferenceStatus_t& message,
                                              uint8_t* payload)
{
	uint8_t offset = IcdProtocol::AppendHeader(payload, timetagMs, counter, IcdType_ImuReferenceStatus);
	payload[offset++] = message.Enable;
	payload[offset++] = message.FrameMode;
	IcdProtocol::AppendFloat(payload, &offset, message.TargetAzimuthDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.TargetElevationDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.CurrentAzimuthDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.CurrentElevationDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.AzimuthErrorDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.ElevationErrorDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.AzimuthPiOutputDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.ElevationPiOutputDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.Motor1AngleDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.Motor2AngleDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.Motor1TargetAngleDeg);
	IcdProtocol::AppendFloat(payload, &offset, message.Motor2TargetAngleDeg);
	payload[offset++] = message.ReverseBranch;
	return offset;
}

inline uint8_t BuildPayload(const IcdMessage_t& message, uint8_t* payload)
{
	if (message.Header.IcdType == IcdType_PWMControl)
	{
		return BuildPwmControlPayload(message.Header.TimetagMs,
		                              message.Header.Counter,
		                              message.Payload.PWMControl,
		                              payload);
	}

	if (message.Header.IcdType == IcdType_MotorControl)
	{
		return BuildMotorControlPayload(message.Header.TimetagMs,
		                                message.Header.Counter,
		                                message.Payload.MotorControl,
		                                payload);
	}

	if (message.Header.IcdType == IcdType_Bno055Telemetry)
	{
		return BuildBno055TelemetryPayload(message.Header.TimetagMs,
		                                   message.Header.Counter,
		                                   message.Payload.Bno055Telemetry,
		                                   payload);
	}

	if (message.Header.IcdType == IcdType_Bno055CalibrationStatus)
	{
		return BuildBno055CalibrationStatusPayload(message.Header.TimetagMs,
		                                           message.Header.Counter,
		                                           message.Payload.Bno055CalibrationStatus,
		                                           payload);
	}

	if (message.Header.IcdType == IcdType_ImuReferenceControl)
	{
		return BuildImuReferenceControlPayload(message.Header.TimetagMs,
		                                       message.Header.Counter,
		                                       message.Payload.ImuReferenceControl,
		                                       payload);
	}

	if (message.Header.IcdType == IcdType_ImuReferenceTuning)
	{
		return BuildImuReferenceTuningPayload(message.Header.TimetagMs,
		                                      message.Header.Counter,
		                                      message.Payload.ImuReferenceTuning,
		                                      payload);
	}

	if (message.Header.IcdType == IcdType_ImuReferenceStatus)
	{
		return BuildImuReferenceStatusPayload(message.Header.TimetagMs,
		                                      message.Header.Counter,
		                                      message.Payload.ImuReferenceStatus,
		                                      payload);
	}

	return 0U;
}

inline uint8_t BuildFrame(const IcdMessage_t& message, uint8_t* frame)
{
	uint8_t payload[ICD_HEADER_SIZE_BYTES + sizeof(IcdPayload_u)] = {0U};
	const uint8_t payloadLength = BuildPayload(message, payload);
	if (payloadLength == 0U)
	{
		return 0U;
	}

	return IcdProtocol::BuildFrame(payload, payloadLength, frame);
}
}
