#ifndef __MOTOR_H__
#define __MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "board.h"

void MOTOR_Init(void);
HAL_StatusTypeDef MOTOR_SetMotor1Duty(uint32_t duty);
HAL_StatusTypeDef MOTOR_SetMotor2Duty(uint32_t duty);
HAL_StatusTypeDef MOTOR_SetMotor1PulseUs(uint32_t pulseUs);
HAL_StatusTypeDef MOTOR_SetMotor2PulseUs(uint32_t pulseUs);
HAL_StatusTypeDef MOTOR_SetMotor1Angle(float angleDeg);
HAL_StatusTypeDef MOTOR_SetMotor2Angle(float angleDeg);
HAL_StatusTypeDef MOTOR_SetMotor1Speed(int32_t speed);
HAL_StatusTypeDef MOTOR_SetMotor2Speed(int32_t speed);

#ifdef __cplusplus
}
#endif

#endif
