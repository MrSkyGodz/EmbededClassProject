#pragma once

#include "drv_bno055/inc/bno055_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void Initialize();
void Loop();
bool ReadOneSensorSample(BNO055_Sensors_t* sample);
void PublishSensorSample(const BNO055_Sensors_t* sample);

void ToggleLed();

#ifdef __cplusplus
}
#endif
