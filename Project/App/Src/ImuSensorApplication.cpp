#include <ImuSensorApplication.h>
#include "bno055.h"

namespace
{
constexpr uint8_t kReadFailureRecoveryThreshold = 3U;
constexpr uint8_t kRecoveryCooldownReadCycles = 10U;

uint8_t g_consecutiveReadFailures = 0U;
uint8_t g_recoveryCooldownReadCycles = 0U;
uint8_t g_requestedFrameMode = 1U;

bool ApplyRequestedFrameMode()
{
	BNO055_OperationModeRequest_t request = {};
	request.DeviceIndex = BNO055_DEVICE_INDEX;
	request.Mode = (g_requestedFrameMode == 0U) ? IMU : NDOF;

	return Bno055_Ioctl(BNO055_IOCTL_SET_OPERATION_MODE, &request) == BNO055_ERROR_NONE;
}

bool TryRecoverImu()
{
	if (g_recoveryCooldownReadCycles > 0U)
	{
		g_recoveryCooldownReadCycles--;
		return false;
	}

	if (g_consecutiveReadFailures < kReadFailureRecoveryThreshold)
	{
		return false;
	}

	g_consecutiveReadFailures = 0U;
	g_recoveryCooldownReadCycles = kRecoveryCooldownReadCycles;

	if (BNO055_Recover() != BNO055_ERROR_NONE)
	{
		return false;
	}

	if (!ApplyRequestedFrameMode())
	{
		return false;
	}

	g_recoveryCooldownReadCycles = 0U;
	return true;
}

void RegisterReadFailure()
{
	if (g_consecutiveReadFailures < kReadFailureRecoveryThreshold)
	{
		g_consecutiveReadFailures++;
	}
}
}

bool ReadOneSensorSample(BNO055_Sensors_t* sample)
{
	if (sample == nullptr)
	{
		return false;
	}
	BnoReadRequest.SensorData = sample;
	if (Bno055_Read(&BnoReadRequest, sizeof(BnoReadRequest)) == BNO055_ERROR_NONE)
	{
		g_consecutiveReadFailures = 0U;
		g_recoveryCooldownReadCycles = 0U;
		return true;
	}

	RegisterReadFailure();
	if (TryRecoverImu())
	{
		BnoReadRequest.SensorData = sample;
		if (Bno055_Read(&BnoReadRequest, sizeof(BnoReadRequest)) == BNO055_ERROR_NONE)
		{
			g_consecutiveReadFailures = 0U;
			return true;
		}

		RegisterReadFailure();
	}

	return false;
}

bool ReadImuCalibrationStatus(Calib_status_t* calibration, bool* fullyCalibrated)
{
	if (calibration == nullptr)
	{
		return false;
	}

	BNO055_CalibrationRequest_t request = {};
	request.DeviceIndex = BNO055_DEVICE_INDEX;
	request.Calibration = calibration;

	if (Bno055_Ioctl(BNO055_IOCTL_GET_CALIBRATION, &request) != BNO055_ERROR_NONE)
	{
		return false;
	}

	if (fullyCalibrated != nullptr)
	{
		*fullyCalibrated = (calibration->System == 3U) &&
		                   (calibration->Gyro == 3U) &&
		                   (calibration->Acc == 3U) &&
		                   (calibration->MAG == 3U);
	}

	return true;
}

void StartImu()
{
	if (BNO055_Init() == BNO055_ERROR_NONE)
	{
		(void) ApplyRequestedFrameMode();
		g_consecutiveReadFailures = 0U;
		g_recoveryCooldownReadCycles = 0U;
	}
	else
	{
		g_consecutiveReadFailures = kReadFailureRecoveryThreshold;
	}
}

bool SetImuReferenceFrameMode(uint8_t frameMode)
{
	g_requestedFrameMode = (frameMode == 0U) ? 0U : 1U;

	return ApplyRequestedFrameMode();
}
