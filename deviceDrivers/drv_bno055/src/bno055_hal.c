#include "drv_bno055/inc/bno055_hal.h"
#include "drv_bno055/inc/bno055_const.h"

HAL_StatusTypeDef Bno055_HardwareInit(const Bno055_HardwareInit_t* hardwareInit)
{
	HAL_StatusTypeDef status = HAL_OK;
	DrvI2c_DeviceParams_t i2cDeviceParams = {0};

	if (hardwareInit == NULL)
	{
		return HAL_ERROR;
	}

	status = DrvI2c_Open((void*) &hardwareInit->I2C);
	if (status != HAL_OK)
	{
		return status;
	}

	i2cDeviceParams.I2CIndex = hardwareInit->I2C.I2CIndex;

	if ((hardwareInit->IntPin.Port != NULL) && (hardwareInit->IntPin.Pin != 0U))
	{
		status = DrvGpio_Open((void*) &hardwareInit->IntPin);
		if (status != HAL_OK)
		{
			(void) DrvI2c_Close(&i2cDeviceParams);
			return status;
		}
	}

	return HAL_OK;
}

HAL_StatusTypeDef Bno055_ReadData(uint32_t index, uint8_t reg, uint8_t *pData, uint16_t len)
{
	DrvI2c_MemTransferParams_t transferParams = {
		.I2CIndex = index,
		.DevAddress = P_BNO055,
		.MemAddress = reg,
		.MemAddressSize = I2C_MEMADD_SIZE_8BIT,
		.Timeout = 1000U
	};

	return DrvI2c_MemRead(&transferParams, pData, len);
}

HAL_StatusTypeDef Bno055_WriteData(uint32_t index, uint8_t reg, const uint8_t *pData, uint16_t len)
{
	DrvI2c_MemTransferParams_t transferParams = {
		.I2CIndex = index,
		.DevAddress = P_BNO055,
		.MemAddress = reg,
		.MemAddressSize = I2C_MEMADD_SIZE_8BIT,
		.Timeout = 1000U
	};

	return DrvI2c_MemWrite(&transferParams, pData, len);
}

