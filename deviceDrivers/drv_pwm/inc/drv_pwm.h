#ifndef DRV_PWM_H_
#define DRV_PWM_H_

#include <stdint.h>
#include "drv_gpio/inc/drv_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_PWM_MAX_INSTANCES 8U

typedef struct
{
	TIM_TypeDef* Instance;
	uint32_t Prescaler;
	uint32_t CounterMode;
	uint32_t Period;
	uint32_t ClockDivision;
	uint32_t AutoReloadPreload;
} DrvPwm_PeripheralInit_t;

typedef struct
{
	uint32_t OCMode;
	uint32_t Pulse;
	uint32_t OCPolarity;
	uint32_t OCFastMode;
} DrvPwm_ChannelInit_t;

typedef struct
{
	uint32_t PwmIndex;
	DrvPwm_PeripheralInit_t PWM_Init;
	DrvPwm_ChannelInit_t ChannelInit;
	DrvGpio_OpenParams_t GPIO_Init;
	uint32_t Channel;
	TIM_HandleTypeDef* TIMHandle;
	uint8_t InitGpio;
} DrvPwm_OpenParams_t;

typedef struct
{
	uint32_t PwmIndex;
} DrvPwm_DeviceParams_t;

typedef struct
{
	uint32_t PwmIndex;
	uint32_t Compare;
} DrvPwm_CompareRequest_t;

typedef struct
{
	uint32_t PwmIndex;
	uint32_t Duty;
	uint32_t DutyMax;
} DrvPwm_DutyRequest_t;

typedef enum
{
	DRV_PWM_IOCTL_START = 0,
	DRV_PWM_IOCTL_STOP,
	DRV_PWM_IOCTL_SET_COMPARE,
	DRV_PWM_IOCTL_SET_DUTY
} DrvPwm_IoctlCmd_t;

HAL_StatusTypeDef DrvPwm_Open(void* vpParam);
HAL_StatusTypeDef DrvPwm_Close(void* vpParam);
TIM_HandleTypeDef* DrvPwm_GetHandle(uint32_t index);
HAL_StatusTypeDef DrvPwm_Start(uint32_t index);
HAL_StatusTypeDef DrvPwm_Stop(uint32_t index);
HAL_StatusTypeDef DrvPwm_SetCompare(void* vpParam);
HAL_StatusTypeDef DrvPwm_SetDuty(void* vpParam);
HAL_StatusTypeDef DrvPwm_Ioctl(const uint32_t command, void* vpParam);

#ifdef __cplusplus
}
#endif

#endif
