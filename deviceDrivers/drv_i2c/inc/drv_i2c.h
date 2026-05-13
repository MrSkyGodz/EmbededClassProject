#ifndef DRV_I2C_H_
#define DRV_I2C_H_

#include <stdint.h>
#include "drv_gpio/inc/drv_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_I2C_MAX_INSTANCES 3U

typedef struct
{
	I2C_TypeDef* Instance;
	uint32_t ClockSpeed;
} DrvI2c_PeripheralInit_t;

typedef struct
{
	uint32_t I2CIndex;
	DrvI2c_PeripheralInit_t I2C_Init;
	DrvGpio_OpenParams_t GPIO_SDAInit;
	DrvGpio_OpenParams_t GPIO_SCLInit;
	I2C_HandleTypeDef* I2CHandle;
} DrvI2c_OpenParams_t;

typedef struct
{
	uint32_t I2CIndex;
} DrvI2c_DeviceParams_t;

typedef struct
{
	uint32_t I2CIndex;
	uint16_t DevAddress;
	uint16_t MemAddress;
	uint16_t MemAddressSize;
	uint32_t Timeout;
} DrvI2c_MemTransferParams_t;

HAL_StatusTypeDef DrvI2c_Open(void* vpParam);
HAL_StatusTypeDef DrvI2c_Close(void* vpParam);

I2C_HandleTypeDef* DrvI2c_GetHandle(uint32_t index);

HAL_StatusTypeDef DrvI2c_MemRead(void* vpParam,
                                 uint8_t* data,
                                 uint16_t length);

HAL_StatusTypeDef DrvI2c_MemWrite(void* vpParam,
                                  const uint8_t* data,
                                  uint16_t length);

#ifdef __cplusplus
}
#endif

#endif
