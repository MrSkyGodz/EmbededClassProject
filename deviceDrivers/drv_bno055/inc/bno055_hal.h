#ifndef BNO055_HAL_H_
#define BNO055_HAL_H_

#include <stdint.h>
#include "drv_gpio/inc/drv_gpio.h"
#include "drv_i2c/inc/drv_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BO055 DRV_I2C_MAX_INSTANCES ///< Can not exceed number of i2c

typedef struct
{
	DrvI2c_OpenParams_t I2C;
	DrvGpio_OpenParams_t IntPin;
} Bno055_HardwareInit_t;

HAL_StatusTypeDef Bno055_HardwareInit(const Bno055_HardwareInit_t* hardwareInit);
HAL_StatusTypeDef Bno055_ReadData(uint32_t index, uint8_t reg, uint8_t *data, uint16_t len);
HAL_StatusTypeDef Bno055_WriteData(uint32_t index, uint8_t reg, const uint8_t *pData, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
