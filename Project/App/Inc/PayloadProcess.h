/*
 * PayloadProcess.h
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */

#ifndef INC_PAYLOADPROCESS_H_
#define INC_PAYLOADPROCESS_H_


#include "CommunicationType.h"
#include "drv_bno055/inc/bno055_types.h"

inline IcdMessage_t BuildBno055TelemetryMessage(const BNO055_Sensors_t* sample)
{
	IcdMessage_t message = {};
	static uint16_t TelemetryCounter = 0;
	message.Header.TimetagMs = HAL_GetTick();
	message.Header.Counter = TelemetryCounter++;
	message.Header.IcdType = IcdType_Bno055Telemetry;
	message.Payload.Bno055Telemetry.EulerX = sample->Euler.X;
	message.Payload.Bno055Telemetry.EulerY = sample->Euler.Y;
	message.Payload.Bno055Telemetry.EulerZ = sample->Euler.Z;
	message.Payload.Bno055Telemetry.GyroX = sample->Gyro.X;
	message.Payload.Bno055Telemetry.GyroY = sample->Gyro.Y;
	message.Payload.Bno055Telemetry.GyroZ = sample->Gyro.Z;
	return message;
}



#endif /* INC_PAYLOADPROCESS_H_ */
