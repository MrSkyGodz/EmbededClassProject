#include "ImuReferenceController.h"

#include "ImuSensorApplication.h"
#include "MotorApplication.h"
#include "board.h"
#include "main.h"

namespace
{
constexpr float kDefaultKp = 0.2F;
constexpr float kDeadbandDeg = 0.5F;
constexpr float kServoMinDeg = 0.0F;
constexpr float kAzimuthServoMaxDeg = MOTOR1_SERVO_MAX_ANGLE_DEG;
constexpr float kElevationServoMaxDeg = MOTOR2_SERVO_MAX_ANGLE_DEG;
constexpr float kAzimuthServoCenterDeg = kAzimuthServoMaxDeg * 0.5F;
constexpr float kElevationServoCenterDeg = kElevationServoMaxDeg * 0.5F;
constexpr float kFullTurnDeg = 360.0F;
constexpr float kHalfTurnDeg = 180.0F;
constexpr float kGainMin = 0.0F;
constexpr float kGainMax = 100.0F;
constexpr float kMaxAzimuthStepDeg = 4.0F;
constexpr float kMaxElevationStepDeg = 4.0F;
constexpr uint8_t kDefaultFrameMode = 1U;

struct ControllerState
{
	float targetAzimuthDeg;
	float targetElevationDeg;
	uint8_t enable;
	uint8_t frameMode;
	float lastLogicalAzimuthDeg;
	float lastLogicalElevationDeg;
	float motor1AngleDeg;
	float motor2AngleDeg;
	float motor1TargetAngleDeg;
	float motor2TargetAngleDeg;
	float azimuthKp;
	float elevationKp;
	float azimuthOutputDeg;
	float elevationOutputDeg;
};

ControllerState g_state;

float clampFloat(float value, float minValue, float maxValue)
{
	if (value < minValue)
	{
		return minValue;
	}

	if (value > maxValue)
	{
		return maxValue;
	}

	return value;
}

float normalizeAzimuth360(float value)
{
	while (value < 0.0F)
	{
		value += kFullTurnDeg;
	}

	while (value >= kFullTurnDeg)
	{
		value -= kFullTurnDeg;
	}

	return value;
}

float wrapDeg180(float value)
{
	float wrapped = normalizeAzimuth360(value);
	if (wrapped > kHalfTurnDeg)
	{
		wrapped -= kFullTurnDeg;
	}
	return wrapped;
}

float logicalToPhysicalAzimuth(float logicalAzimuthDeg)
{
	return kAzimuthServoMaxDeg - clampFloat(logicalAzimuthDeg, kServoMinDeg, kAzimuthServoMaxDeg);
}

float physicalToLogicalAzimuth(float physicalAzimuthDeg)
{
	return kAzimuthServoMaxDeg - clampFloat(physicalAzimuthDeg, kServoMinDeg, kAzimuthServoMaxDeg);
}

float logicalToPhysicalElevation(float logicalElevationDeg)
{
	return clampFloat(logicalElevationDeg, kServoMinDeg, kElevationServoMaxDeg);
}

float physicalToLogicalElevation(float physicalElevationDeg)
{
	return clampFloat(physicalElevationDeg, kServoMinDeg, kElevationServoMaxDeg);
}

float absFloat(float value)
{
	return (value < 0.0F) ? -value : value;
}

float calculateOutput(float errorDeg, float gain, float maxStepDeg)
{
	if (absFloat(errorDeg) < kDeadbandDeg)
	{
		return 0.0F;
	}
	return clampFloat(gain * errorDeg, -maxStepDeg, maxStepDeg);
}

void clearOutputs()
{
	g_state.azimuthOutputDeg = 0.0F;
	g_state.elevationOutputDeg = 0.0F;
}

void fillStatusMessage(IcdMessage_t* message,
                       float currentAzimuthDeg,
                       float currentElevationDeg,
                       float azimuthErrorDeg,
                       float elevationErrorDeg)
{
	if (message == nullptr)
	{
		return;
	}

	static uint8_t statusCounter = 0U;
	message->Header.TimetagMs = HAL_GetTick();
	message->Header.Counter = statusCounter++;
	message->Header.IcdType = IcdType_ImuReferenceStatus;
	message->Payload.ImuReferenceStatus.Enable = g_state.enable;
	message->Payload.ImuReferenceStatus.FrameMode = g_state.frameMode;
	message->Payload.ImuReferenceStatus.TargetAzimuthDeg = g_state.targetAzimuthDeg;
	message->Payload.ImuReferenceStatus.TargetElevationDeg = g_state.targetElevationDeg;
	message->Payload.ImuReferenceStatus.CurrentAzimuthDeg = currentAzimuthDeg;
	message->Payload.ImuReferenceStatus.CurrentElevationDeg = currentElevationDeg;
	message->Payload.ImuReferenceStatus.AzimuthErrorDeg = azimuthErrorDeg;
	message->Payload.ImuReferenceStatus.ElevationErrorDeg = elevationErrorDeg;
	message->Payload.ImuReferenceStatus.AzimuthPiOutputDeg = g_state.azimuthOutputDeg;
	message->Payload.ImuReferenceStatus.ElevationPiOutputDeg = g_state.elevationOutputDeg;
	message->Payload.ImuReferenceStatus.Motor1AngleDeg = g_state.motor1AngleDeg;
	message->Payload.ImuReferenceStatus.Motor2AngleDeg = g_state.motor2AngleDeg;
	message->Payload.ImuReferenceStatus.Motor1TargetAngleDeg = g_state.motor1TargetAngleDeg;
	message->Payload.ImuReferenceStatus.Motor2TargetAngleDeg = g_state.motor2TargetAngleDeg;
	message->Payload.ImuReferenceStatus.ReverseBranch = 0U;
}
}

void ImuReferenceController_Init(void)
{
	g_state.targetAzimuthDeg = 0.0F;
	g_state.targetElevationDeg = 0.0F;
	g_state.enable = 0U;
	g_state.frameMode = kDefaultFrameMode;
	g_state.lastLogicalAzimuthDeg = kAzimuthServoCenterDeg;
	g_state.lastLogicalElevationDeg = kElevationServoCenterDeg;
	g_state.motor1AngleDeg = logicalToPhysicalAzimuth(g_state.lastLogicalAzimuthDeg);
	g_state.motor2AngleDeg = logicalToPhysicalElevation(g_state.lastLogicalElevationDeg);
	g_state.motor1TargetAngleDeg = g_state.motor1AngleDeg;
	g_state.motor2TargetAngleDeg = g_state.motor2AngleDeg;
	g_state.azimuthKp = kDefaultKp;
	g_state.elevationKp = kDefaultKp;
	clearOutputs();
}

void ImuReferenceController_ApplyControlCommand(const ImuReferenceControl_t* command)
{
	if (command == nullptr)
	{
		return;
	}

	const uint8_t previousFrameMode = g_state.frameMode;
	g_state.targetAzimuthDeg = normalizeAzimuth360(command->TargetAzimuthDeg);
	g_state.targetElevationDeg = clampFloat(command->TargetElevationDeg,
	                                        kServoMinDeg,
	                                        kElevationServoMaxDeg);
	g_state.enable = (command->Enable == 0U) ? 0U : 1U;
	g_state.frameMode = (command->FrameMode == 0U) ? 0U : 1U;

	if (g_state.enable == 0U)
	{
		clearOutputs();
	}

	if (g_state.frameMode != previousFrameMode)
	{
		(void) SetImuReferenceFrameMode(g_state.frameMode);
	}
}

void ImuReferenceController_ApplyTuningCommand(const ImuReferenceTuning_t* command)
{
	if (command == nullptr)
	{
		return;
	}

	g_state.azimuthKp = clampFloat(command->AzimuthKp, kGainMin, kGainMax);
	g_state.elevationKp = clampFloat(command->ElevationKp, kGainMin, kGainMax);
	clearOutputs();
}

void ImuReferenceController_NotifyManualMotorCommand(float motor1Deg, float motor2Deg)
{
	g_state.motor1AngleDeg = clampFloat(motor1Deg, kServoMinDeg, kAzimuthServoMaxDeg);
	g_state.motor2AngleDeg = clampFloat(motor2Deg, kServoMinDeg, kElevationServoMaxDeg);
	g_state.lastLogicalAzimuthDeg = physicalToLogicalAzimuth(g_state.motor1AngleDeg);
	g_state.lastLogicalElevationDeg = physicalToLogicalElevation(g_state.motor2AngleDeg);
	g_state.motor1TargetAngleDeg = g_state.motor1AngleDeg;
	g_state.motor2TargetAngleDeg = g_state.motor2AngleDeg;
	g_state.enable = 0U;
	clearOutputs();
}

bool ImuReferenceController_Update(const BNO055_Sensors_t* sample, IcdMessage_t* statusMessage)
{
	if (sample == nullptr)
	{
		return false;
	}

	const float currentAzimuthDeg = normalizeAzimuth360(sample->Euler.X);
	const float currentElevationDeg = sample->Euler.Z;
	const float azimuthErrorDeg = wrapDeg180(g_state.targetAzimuthDeg - currentAzimuthDeg);
	const float elevationErrorDeg = g_state.targetElevationDeg - currentElevationDeg;

	if (g_state.enable != 0U)
	{
		g_state.azimuthOutputDeg = calculateOutput(azimuthErrorDeg,
		                                           g_state.azimuthKp,
		                                           kMaxAzimuthStepDeg);
		g_state.elevationOutputDeg = calculateOutput(elevationErrorDeg,
		                                             g_state.elevationKp,
		                                             kMaxElevationStepDeg);

		g_state.lastLogicalAzimuthDeg = clampFloat(g_state.lastLogicalAzimuthDeg + g_state.azimuthOutputDeg,
		                                           kServoMinDeg,
		                                           kAzimuthServoMaxDeg);
		g_state.lastLogicalElevationDeg = clampFloat(g_state.lastLogicalElevationDeg + g_state.elevationOutputDeg,
		                                             kServoMinDeg,
		                                             kElevationServoMaxDeg);
		g_state.motor1AngleDeg = logicalToPhysicalAzimuth(g_state.lastLogicalAzimuthDeg);
		g_state.motor2AngleDeg = logicalToPhysicalElevation(g_state.lastLogicalElevationDeg);
		g_state.motor1TargetAngleDeg = g_state.motor1AngleDeg;
		g_state.motor2TargetAngleDeg = g_state.motor2AngleDeg;
		SetMotorPositionReference(g_state.motor1AngleDeg, g_state.motor2AngleDeg);
	}
	else
	{
		g_state.motor1TargetAngleDeg = g_state.motor1AngleDeg;
		g_state.motor2TargetAngleDeg = g_state.motor2AngleDeg;
		clearOutputs();
	}

	fillStatusMessage(statusMessage,
	                  currentAzimuthDeg,
	                  currentElevationDeg,
	                  azimuthErrorDeg,
	                  elevationErrorDeg);
	return true;
}
