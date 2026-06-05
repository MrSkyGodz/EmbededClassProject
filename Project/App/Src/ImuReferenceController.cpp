#include "ImuReferenceController.h"

#include "ImuSensorApplication.h"
#include "MotorApplication.h"
#include "board.h"
#include "main.h"

namespace
{
constexpr float kDefaultKp = 1.0F;
constexpr float kDefaultKi = 0.0F;
constexpr float kControlPeriodSeconds = 0.1F;
constexpr float kServoMinDeg = 0.0F;
constexpr float kAzimuthServoMaxDeg = MOTOR1_SERVO_MAX_ANGLE_DEG;
constexpr float kElevationServoMaxDeg = MOTOR2_SERVO_MAX_ANGLE_DEG;
constexpr float kAzimuthServoCenterDeg = kAzimuthServoMaxDeg * 0.5F;
constexpr float kElevationServoCenterDeg = kElevationServoMaxDeg * 0.5F;
constexpr float kFullTurnDeg = 360.0F;
constexpr float kHalfTurnDeg = 180.0F;
constexpr float kElevationMinDeg = kServoMinDeg;
constexpr float kElevationMaxDeg = kElevationServoMaxDeg;
constexpr float kGainMin = 0.0F;
constexpr float kGainMax = 100.0F;
constexpr float kMaxStepAzDeg = 4.0F;
constexpr float kMaxStepElDeg = 4.0F;
constexpr float kMaxRateAzDegPerSecond = 90.0F;
constexpr float kMaxRateElDegPerSecond = 90.0F;
constexpr float kAzimuthFeedbackDirection = 1.0F;
constexpr float kElevationFeedbackDirection = 1.0F;
constexpr float kFeedbackDeadbandDeg = 0.75F;
constexpr float kFeedbackFullGyroDps = 30.0F;
constexpr float kFeedbackHoldGyroDps = 120.0F;
constexpr uint8_t kDefaultFrameMode = 1U;

struct TuningState
{
	float azimuthKp;
	float azimuthKi;
	float elevationKp;
	float elevationKi;
};

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
	float azimuthOutputDeg;
	float elevationOutputDeg;
	TuningState tuning;
};

struct AzEl
{
	float azimuthDeg;
	float elevationDeg;
};

struct LogicalCommand
{
	float azimuthDeg;
	float elevationDeg;
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

float absoluteFloat(float value)
{
	return (value < 0.0F) ? -value : value;
}

float signFloat(float value)
{
	return (value < 0.0F) ? -1.0F : 1.0F;
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

float rateLimit(float desired, float last, float maxRateDegPerSecond)
{
	const float maxStep = maxRateDegPerSecond * kControlPeriodSeconds;
	const float diff = clampFloat(desired - last, -maxStep, maxStep);
	return last + diff;
}

AzEl readCurrentAzEl(const BNO055_Sensors_t* sample)
{
	const AzEl current = {
		normalizeAzimuth360(sample->Euler.X),
		sample->Euler.Z,
	};
	return current;
}

float feedbackMotionScale(const BNO055_Sensors_t* sample)
{
	const float gyroMagnitudeDps = absoluteFloat(sample->Gyro.X) +
	                               absoluteFloat(sample->Gyro.Y) +
	                               absoluteFloat(sample->Gyro.Z);

	if (gyroMagnitudeDps <= kFeedbackFullGyroDps)
	{
		return 1.0F;
	}

	if (gyroMagnitudeDps >= kFeedbackHoldGyroDps)
	{
		return 0.0F;
	}

	return (kFeedbackHoldGyroDps - gyroMagnitudeDps) /
	       (kFeedbackHoldGyroDps - kFeedbackFullGyroDps);
}

float applyFeedbackDeadband(float errorDeg)
{
	if (absoluteFloat(errorDeg) <= kFeedbackDeadbandDeg)
	{
		return 0.0F;
	}

	return errorDeg - (signFloat(errorDeg) * kFeedbackDeadbandDeg);
}

float feedbackDelta(float errorDeg, float gain, float maxStepDeg, float motionScale)
{
	const float activeErrorDeg = applyFeedbackDeadband(errorDeg);
	const float deltaDeg = gain * activeErrorDeg * motionScale;
	return clampFloat(deltaDeg, -maxStepDeg, maxStepDeg);
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

LogicalCommand clampLogicalCommand(const LogicalCommand& command)
{
	const LogicalCommand clamped = {
		clampFloat(command.azimuthDeg, kServoMinDeg, kAzimuthServoMaxDeg),
		clampFloat(command.elevationDeg, kServoMinDeg, kElevationServoMaxDeg),
	};
	return clamped;
}

LogicalCommand rateLimitCommand(const LogicalCommand& command)
{
	LogicalCommand limited = command;
	limited.azimuthDeg = rateLimit(command.azimuthDeg,
	                               g_state.lastLogicalAzimuthDeg,
	                               kMaxRateAzDegPerSecond);
	limited.elevationDeg = rateLimit(command.elevationDeg,
	                                 g_state.lastLogicalElevationDeg,
	                                 kMaxRateElDegPerSecond);
	return clampLogicalCommand(limited);
}

void clearOutputs()
{
	g_state.azimuthOutputDeg = 0.0F;
	g_state.elevationOutputDeg = 0.0F;
}

void fillStatusMessage(IcdMessage_t* message,
                       const AzEl& currentAzEl,
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
	message->Payload.ImuReferenceStatus.CurrentAzimuthDeg = currentAzEl.azimuthDeg;
	message->Payload.ImuReferenceStatus.CurrentElevationDeg = currentAzEl.elevationDeg;
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
	g_state.tuning.azimuthKp = kDefaultKp;
	g_state.tuning.azimuthKi = kDefaultKi;
	g_state.tuning.elevationKp = kDefaultKp;
	g_state.tuning.elevationKi = kDefaultKi;
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
	g_state.targetElevationDeg = clampFloat(command->TargetElevationDeg, kElevationMinDeg, kElevationMaxDeg);
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

	g_state.tuning.azimuthKp = clampFloat(command->AzimuthKp, kGainMin, kGainMax);
	g_state.tuning.azimuthKi = clampFloat(command->AzimuthKi, kGainMin, kGainMax);
	g_state.tuning.elevationKp = clampFloat(command->ElevationKp, kGainMin, kGainMax);
	g_state.tuning.elevationKi = clampFloat(command->ElevationKi, kGainMin, kGainMax);

	if (command->ResetIntegrator != 0U)
	{
		clearOutputs();
	}
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

	const AzEl currentAzEl = readCurrentAzEl(sample);
	const float azimuthErrorDeg = wrapDeg180(g_state.targetAzimuthDeg - currentAzEl.azimuthDeg);
	const float elevationErrorDeg = g_state.targetElevationDeg - currentAzEl.elevationDeg;
	const float motionScale = feedbackMotionScale(sample);
	const float deltaAzimuthDeg = feedbackDelta(azimuthErrorDeg,
	                                            g_state.tuning.azimuthKp,
	                                            kMaxStepAzDeg,
	                                            motionScale);
	const float deltaElevationDeg = feedbackDelta(elevationErrorDeg,
	                                              g_state.tuning.elevationKp,
	                                              kMaxStepElDeg,
	                                              motionScale);

	if (g_state.enable != 0U)
	{
		const LogicalCommand desired = {
			g_state.lastLogicalAzimuthDeg + (kAzimuthFeedbackDirection * deltaAzimuthDeg),
			g_state.lastLogicalElevationDeg + (kElevationFeedbackDirection * deltaElevationDeg),
		};
		const LogicalCommand corrected = clampLogicalCommand(desired);
		const LogicalCommand limited = rateLimitCommand(corrected);

		g_state.azimuthOutputDeg = deltaAzimuthDeg;
		g_state.elevationOutputDeg = deltaElevationDeg;
		g_state.lastLogicalAzimuthDeg = limited.azimuthDeg;
		g_state.lastLogicalElevationDeg = limited.elevationDeg;
		g_state.motor1TargetAngleDeg = logicalToPhysicalAzimuth(corrected.azimuthDeg);
		g_state.motor2TargetAngleDeg = logicalToPhysicalElevation(corrected.elevationDeg);
		g_state.motor1AngleDeg = logicalToPhysicalAzimuth(limited.azimuthDeg);
		g_state.motor2AngleDeg = logicalToPhysicalElevation(limited.elevationDeg);
		SetMotorPositionReference(g_state.motor1AngleDeg, g_state.motor2AngleDeg);
	}
	else
	{
		g_state.motor1TargetAngleDeg = g_state.motor1AngleDeg;
		g_state.motor2TargetAngleDeg = g_state.motor2AngleDeg;
		clearOutputs();
	}

	fillStatusMessage(statusMessage, currentAzEl, azimuthErrorDeg, elevationErrorDeg);
	return true;
}
