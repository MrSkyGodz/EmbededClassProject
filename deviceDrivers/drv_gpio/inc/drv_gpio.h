#ifndef DRV_GPIO_H_
#define DRV_GPIO_H_

#include <stdint.h>
#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_GPIO_MAX_INSTANCES 16U


typedef struct
{
	uint32_t GPIOIndex;
	uint8_t InterruptEnabled;
	IRQn_Type InterruptIRQn;
	uint32_t InterruptPriority;
	uint32_t InterruptSubPriority;
} DrvGpio_InterruptInit_t;

typedef struct
{
	uint32_t GPIOIndex;
	GPIO_TypeDef* Port;
	uint16_t Pin;
	uint32_t Function;
	uint32_t Mode;
	uint32_t Pull;
	uint32_t Speed;
	DrvGpio_InterruptInit_t InitInterrupt;
} DrvGpio_OpenParams_t;

typedef void (*DrvGpio_InterruptCallback_t)(uint32_t gpioIndex, void* userData);

typedef struct
{
	uint32_t GPIOIndex;
	DrvGpio_InterruptCallback_t Callback;
	void* UserData;
} DrvGpio_AttachInterruptRequest_t ;

typedef enum
{
	DRV_GPIO_IOCTL_INIT_INTERRUPT = 0,
	DRV_GPIO_IOCTL_ATTACH_INTERRUPT = 1
} DrvGpio_IoctlCommand_t;

HAL_StatusTypeDef DrvGpio_Open(void* vpParam);
HAL_StatusTypeDef DrvGpio_Close(void* vpParam);
GPIO_PinState DrvGpio_Read(uint16_t gpioIndex);
HAL_StatusTypeDef DrvGpio_Write(uint16_t gpioIndex, GPIO_PinState state);
HAL_StatusTypeDef DrvGpio_Toggle(uint16_t gpioIndex);
HAL_StatusTypeDef DrvGpio_Ioctl(DrvGpio_IoctlCommand_t command, void* vpParam);
HAL_StatusTypeDef DrvGpio_IrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif
