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

