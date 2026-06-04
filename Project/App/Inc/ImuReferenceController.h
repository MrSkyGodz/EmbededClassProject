#ifndef INC_IMUREFERENCECONTROLLER_H_
#define INC_IMUREFERENCECONTROLLER_H_

#include <stdbool.h>

#include "CommunicationType.h"
#include "drv_bno055/inc/bno055_types.h"

void ImuReferenceController_Init(void);
void ImuReferenceController_ApplyControlCommand(const ImuReferenceControl_t* command);
void ImuReferenceController_ApplyTuningCommand(const ImuReferenceTuning_t* command);
void ImuReferenceController_NotifyManualMotorCommand(float motor1Deg, float motor2Deg);
bool ImuReferenceController_Update(const BNO055_Sensors_t* sample, IcdMessage_t* statusMessage);

#endif /* INC_IMUREFERENCECONTROLLER_H_ */
