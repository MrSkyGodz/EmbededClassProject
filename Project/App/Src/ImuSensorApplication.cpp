#include <ImuSensorApplication.h>
#include "bno055.h"

bool ReadOneSensorSample(BNO055_Sensors_t* sample)
{
	if (sample == nullptr)
	{
		return false;
	}
	BnoReadRequest.SensorData = sample;
	if (Bno055_Read(&BnoReadRequest, sizeof(BnoReadRequest)) == BNO055_ERROR_NONE)
	{
		return true;
	}

	return false;
}




void StartImu()
{
	// handle error
	BNO055_Init();
}

bool SetImuReferenceFrameMode(uint8_t frameMode)
{
	BNO055_OperationModeRequest_t request = {};
	request.DeviceIndex = BNO055_DEVICE_INDEX;
	request.Mode = (frameMode == 0U) ? IMU : NDOF;

	return Bno055_Ioctl(BNO055_IOCTL_SET_OPERATION_MODE, &request) == BNO055_ERROR_NONE;
}
