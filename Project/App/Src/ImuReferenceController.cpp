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
constexpr float kLimitMarginDeg = 0.0F;
constexpr float kGainMin = 0.0F;
constexpr float kGainMax = 100.0F;
constexpr float kMaxStepAzDeg = 10.0F;
constexpr float kMaxStepElDeg = 10.0F;
constexpr float kMaxRateAzDegPerSecond = 90.0F;
constexpr float kMaxRateElDegPerSecond = 90.0F;

enum ControlMode : uint8_t
{
	ControlMode_Normal = 0U,
	ControlMode_Flipped = 1U,
	ControlMode_Saturated = 2U,
};

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
	uint8_t reverseBranch;
	ControlMode mode;
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
	uint8_t reverseBranch;
	ControlMode mode;
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
	LogicalCommand clamped = command;
	clamped.azimuthDeg = clampFloat(command.azimuthDeg, kServoMinDeg, kAzimuthServoMaxDeg);
	clamped.elevationDeg = clampFloat(command.elevationDeg, kServoMinDeg, kElevationServoMaxDeg);
	return clamped;
}

bool commandInsideLimits(const LogicalCommand& command)
{
	return (command.azimuthDeg >= (kServoMinDeg + kLimitMarginDeg)) &&
	       (command.azimuthDeg <= (kAzimuthServoMaxDeg - kLimitMarginDeg)) &&
	       (command.elevationDeg >= (kServoMinDeg + kLimitMarginDeg)) &&
	       (command.elevationDeg <= (kElevationServoMaxDeg - kLimitMarginDeg));
}

float commandDistanceFromLast(const LogicalCommand& command)
{
	return absoluteFloat(command.azimuthDeg - g_state.lastLogicalAzimuthDeg) +
	       absoluteFloat(command.elevationDeg - g_state.lastLogicalElevationDeg);
}

LogicalCommand closestToLimits(const LogicalCommand& first, const LogicalCommand& second)
{
	LogicalCommand firstClamped = clampLogicalCommand(first);
	LogicalCommand secondClamped = clampLogicalCommand(second);
	firstClamped.mode = ControlMode_Saturated;
	secondClamped.mode = ControlMode_Saturated;

	return (commandDistanceFromLast(secondClamped) < commandDistanceFromLast(firstClamped)) ?
		secondClamped :
		firstClamped;
}

LogicalCommand selectBaseCommand(const LogicalCommand& normal, const LogicalCommand& flipped)
{
	if (commandInsideLimits(normal))
	{
		return normal;
	}

	if (commandInsideLimits(flipped))
	{
		return flipped;
	}

	if (commandInsideLimits(normal))
	{
		return normal;
	}

	const bool preferFlipped = (g_state.mode == ControlMode_Flipped) ||
	                           ((g_state.mode == ControlMode_Saturated) && (g_state.reverseBranch != 0U));
	return preferFlipped ? closestToLimits(flipped, normal) : closestToLimits(normal, flipped);
}

LogicalCommand applyFeedbackCorrection(const LogicalCommand& selectedBase,
                                       float azimuthErrorDeg,
                                       float elevationErrorDeg)
{
	LogicalCommand command = selectedBase;
	const float deltaAzimuthDeg = clampFloat(g_state.tuning.azimuthKp * azimuthErrorDeg,
	                                         -kMaxStepAzDeg,
	                                         kMaxStepAzDeg);
	const float deltaElevationDeg = clampFloat(g_state.tuning.elevationKp * elevationErrorDeg,
	                                           -kMaxStepElDeg,
	                                           kMaxStepElDeg);

	command.azimuthDeg += deltaAzimuthDeg;
	command.elevationDeg += deltaElevationDeg;
	g_state.azimuthOutputDeg = deltaAzimuthDeg;
	g_state.elevationOutputDeg = deltaElevationDeg;
	return clampLogicalCommand(command);
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
	message->Payload.ImuReferenceStatus.ReverseBranch = g_state.reverseBranch;
}
}

void ImuReferenceController_Init(void)
{
	g_state.targetAzimuthDeg = 0.0F;
	g_state.targetElevationDeg = 0.0F;
	g_state.enable = 0U;
	g_state.frameMode = 0U;
	g_state.lastLogicalAzimuthDeg = kAzimuthServoCenterDeg;
	g_state.lastLogicalElevationDeg = kElevationServoCenterDeg;
	g_state.motor1AngleDeg = logicalToPhysicalAzimuth(g_state.lastLogicalAzimuthDeg);
	g_state.motor2AngleDeg = logicalToPhysicalElevation(g_state.lastLogicalElevationDeg);
	g_state.motor1TargetAngleDeg = g_state.motor1AngleDeg;
	g_state.motor2TargetAngleDeg = g_state.motor2AngleDeg;
	g_state.reverseBranch = 0U;
	g_state.mode = ControlMode_Normal;
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
	g_state.reverseBranch = 0U;
	g_state.mode = ControlMode_Normal;
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

	if (g_state.enable != 0U)
	{
		const LogicalCommand normalBase = {
			g_state.targetAzimuthDeg,
			g_state.targetElevationDeg,
			0U,
			ControlMode_Normal,
		};
		const LogicalCommand flippedBase = {
			normalizeAzimuth360(g_state.targetAzimuthDeg + kHalfTurnDeg),
			kElevationServoMaxDeg - g_state.targetElevationDeg,
			1U,
			ControlMode_Flipped,
		};
		const LogicalCommand selectedBase = selectBaseCommand(normalBase, flippedBase);
		const LogicalCommand corrected = applyFeedbackCorrection(selectedBase,
		                                                         azimuthErrorDeg,
		                                                         elevationErrorDeg);
		const LogicalCommand limited = rateLimitCommand(corrected);

		g_state.mode = selectedBase.mode;
		g_state.reverseBranch = selectedBase.reverseBranch;
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
		g_state.reverseBranch = (g_state.mode == ControlMode_Flipped) ? 1U : 0U;
		clearOutputs();
	}

	fillStatusMessage(statusMessage, currentAzEl, azimuthErrorDeg, elevationErrorDeg);
	return true;
}
