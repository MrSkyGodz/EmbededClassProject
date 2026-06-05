/*
 * ImuSensorProcess.h
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */

#ifndef INC_IMUSENSORAPPLICATION_H_
#define INC_IMUSENSORAPPLICATION_H_

#include <stdbool.h>

#include "bno055.h"

void StartImu();
bool ReadOneSensorSample(BNO055_Sensors_t* sample);
bool ReadImuCalibrationStatus(Calib_status_t* calibration, bool* fullyCalibrated);
bool SetImuReferenceFrameMode(uint8_t frameMode);


#endif /* INC_IMUSENSORAPPLICATION_H_ */
