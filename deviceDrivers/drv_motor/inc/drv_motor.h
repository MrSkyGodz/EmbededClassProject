#ifndef DRV_MOTOR_H_
#define DRV_MOTOR_H_

#include <stdint.h>
#include "drv_pwm/inc/drv_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_MOTOR_MAX_INSTANCES 4U
#define DRV_MOTOR_DEFAULT_PWM_FREQUENCY_HZ 50U
#define DRV_MOTOR_DEFAULT_TIMER_TICK_HZ 1000000U
#define DRV_MOTOR_DEFAULT_MIN_PULSE_US 1000U
#define DRV_MOTOR_DEFAULT_CENTER_PULSE_US 1500U
#define DRV_MOTOR_DEFAULT_MAX_PULSE_US 2000U
#define DRV_MOTOR_DEFAULT_MAX_ANGLE_DEG 180U

typedef enum
{
	DRV_MOTOR_TYPE_POSITION_SERVO = 0,
	DRV_MOTOR_TYPE_CONTINUOUS_SERVO
} DrvMotor_Type_t;

typedef struct
{
	uint32_t PwmIndex;
	TIM_TypeDef* Instance;
	uint32_t Channel;
	DrvGpio_OpenParams_t GPIO_Init;
	TIM_HandleTypeDef* TIMHandle;
	uint32_t TimerClockHz;
	uint32_t TimerTickHz;
	uint32_t PwmFrequencyHz;
} DrvMotor_HardwareInit_t;

typedef struct
{
	DrvMotor_Type_t Type;
	uint32_t MinPulseUs;
	uint32_t CenterPulseUs;
	uint32_t MaxPulseUs;
	float MaxAngleDeg;
} DrvMotor_Init_t;

typedef struct
{
	uint32_t MotorIndex;
	DrvMotor_HardwareInit_t Hardware;
	DrvMotor_Init_t Init;
} DrvMotor_OpenParams_t;

typedef struct
{
	uint32_t MotorIndex;
} DrvMotor_DeviceParams_t;

typedef struct
{
	uint32_t MotorIndex;
	float AngleDeg;
} DrvMotor_AngleRequest_t;

typedef struct
{
	uint32_t MotorIndex;
	uint32_t PulseUs;
} DrvMotor_PulseRequest_t;

typedef struct
{
	uint32_t MotorIndex;
	uint32_t Duty;
	uint32_t DutyMax;
} DrvMotor_DutyRequest_t;

typedef struct
{
	uint32_t MotorIndex;
	int32_t Speed;
	int32_t SpeedMax;
} DrvMotor_SpeedRequest_t;

HAL_StatusTypeDef DrvMotor_Open(void* vpParam);
HAL_StatusTypeDef DrvMotor_Close(void* vpParam);
HAL_StatusTypeDef DrvMotor_SetAngle(void* vpParam);
HAL_StatusTypeDef DrvMotor_SetPulseUs(void* vpParam);
HAL_StatusTypeDef DrvMotor_SetDuty(void* vpParam);
HAL_StatusTypeDef DrvMotor_SetSpeed(void* vpParam);

#ifdef __cplusplus
}
#endif

#endif
