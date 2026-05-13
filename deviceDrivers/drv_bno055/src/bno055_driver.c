#include "drv_bno055/inc/bno055_driver.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
	uint32_t BusIndex;
	uint8_t CurrentPage;
	uint8_t CurrentMode;
	uint8_t IsOpen;
} BNO055_Device_t;

static BNO055_Device_t g_bnoDevices[BNO055_MAX_INSTANCES];

static BNO055_Error_t Bno055_ValidateDevice(const BNO055_Device_t* device);

static BNO055_Device_t* Bno055_GetDevice(uint32_t index)
{
	if (index >= BNO055_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_bnoDevices[index];
}

static BNO055_Error_t Bno055_GetOpenDevice(uint32_t index, BNO055_Device_t** device)
{
	if (device == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	*device = Bno055_GetDevice(index);
	if (*device == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	return Bno055_ValidateDevice(*device);
}

static BNO055_Error_t Bno055_FromHalStatus(HAL_StatusTypeDef status)
{
	switch (status)
	{
	case HAL_OK:
		return BNO055_ERROR_NONE;
	case HAL_BUSY:
		return BNO055_ERROR_BUSY;
	case HAL_TIMEOUT:
		return BNO055_ERROR_TIMEOUT;
	case HAL_ERROR:
	default:
		return BNO055_ERROR_HAL;
	}
}

static BNO055_Error_t Bno055_ValidateDevice(const BNO055_Device_t* device)
{
	if (device == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	if (device->IsOpen == 0U)
	{
		return BNO055_ERROR_INVALID_STATE;
	}

	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_ReadRegisters(const BNO055_Device_t* device,
	                                      uint8_t reg,
	                                      uint8_t* data,
	                                      uint16_t length)
{
	BNO055_Error_t status = Bno055_ValidateDevice(device);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	if ((data == NULL) || (length == 0U))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	return Bno055_FromHalStatus(Bno055_ReadData(device->BusIndex, reg, data, length));
}

static BNO055_Error_t Bno055_WriteRegisters(const BNO055_Device_t* device,
	                                       uint8_t reg,
	                                       const uint8_t* data,
	                                       uint16_t length)
{
	BNO055_Error_t status = Bno055_ValidateDevice(device);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	if ((data == NULL) || (length == 0U))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	return Bno055_FromHalStatus(Bno055_WriteData(device->BusIndex, reg, data, length));
}

static BNO055_Error_t Bno055_SelectPageInternal(BNO055_Device_t* device, uint8_t page)
{
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (page > PAGE_1)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_WriteRegisters(device, PAGE_ID_ADDR, &page, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	device->CurrentPage = page;
	HAL_Delay(50U);
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_SetOperationModeInternal(BNO055_Device_t* device, Op_Modes_t mode)
{
	uint8_t rawMode = (uint8_t) mode;
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_0);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_WriteRegisters(device, OPR_MODE_ADDR, &rawMode, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	device->CurrentMode = rawMode;
	HAL_Delay((mode == CONFIG_MODE) ? 19U : 9U);
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_GetCurrentModeInternal(BNO055_Device_t* device, Op_Modes_t* mode)
{
	uint8_t rawMode = 0U;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (mode == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_SelectPageInternal(device, PAGE_0);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_ReadRegisters(device, OPR_MODE_ADDR, &rawMode, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	rawMode &= 0x0FU;
	device->CurrentMode = rawMode;
	*mode = (Op_Modes_t) rawMode;
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_SetPowerModeInternal(BNO055_Device_t* device, uint8_t powerMode)
{
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_0);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_WriteRegisters(device, PWR_MODE_ADDR, &powerMode, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(50U);
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_SetClockSourceInternal(BNO055_Device_t* device, uint8_t source)
{
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_0);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_WriteRegisters(device, SYS_TRIGGER_ADDR, &source, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(100U);
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_SetAxisInternal(BNO055_Device_t* device, uint8_t remap, uint8_t sign)
{
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_0);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_WriteRegisters(device, AXIS_MAP_CONFIG_ADDR, &remap, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(20U);
	status = Bno055_WriteRegisters(device, AXIS_MAP_SIGN_ADDR, &sign, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(100U);
	return BNO055_ERROR_NONE;
}

static bool Bno055_IsFusionMode(Op_Modes_t mode)
{
	return (mode == IMU) ||
	       (mode == COMPASS) ||
	       (mode == M4G) ||
	       (mode == NDOF_FMC_OFF) ||
	       (mode == NDOF);
}

static BNO055_Error_t Bno055_SetAccelRangeInternal(BNO055_Device_t* device, uint8_t range)
{
	uint8_t accConfig = 0U;
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_1);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_ReadRegisters(device, ACC_CONFIG_ADDR, &accConfig, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	accConfig = (uint8_t) ((accConfig & 0xFCU) | (range & 0x03U));
	status = Bno055_WriteRegisters(device, ACC_CONFIG_ADDR, &accConfig, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(100U);
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_SetUnitSelectionInternal(BNO055_Device_t* device, uint8_t unitSelection)
{
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_0);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_WriteRegisters(device, UNIT_SEL_ADDR, &unitSelection, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(100U);
	return BNO055_ERROR_NONE;
}


static BNO055_Error_t Bno055_EnterConfigMode(BNO055_Device_t* device, Op_Modes_t* previousMode)
{
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (previousMode == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_GetCurrentModeInternal(device, previousMode);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	if (*previousMode != CONFIG_MODE)
	{
		status = Bno055_SetOperationModeInternal(device, CONFIG_MODE);
	}

	return status;
}

static BNO055_Error_t Bno055_RestoreMode(BNO055_Device_t* device, Op_Modes_t previousMode)
{
	if (previousMode == CONFIG_MODE)
	{
		return BNO055_ERROR_NONE;
	}

	return Bno055_SetOperationModeInternal(device, previousMode);
}


static BNO055_Error_t Bno055_ConfigureInterruptsInternal(
    BNO055_Device_t* device,
    const BNO055_InterruptConfig_t* config)
{
    uint8_t previousPage = PAGE_0;
    Op_Modes_t previousMode = CONFIG_MODE;
    BNO055_Error_t status = BNO055_ERROR_NONE;

    if ((device == NULL) || (config == NULL))
    {
        return BNO055_ERROR_INVALID_PARAM;
    }

    previousPage = device->CurrentPage;

    status = Bno055_EnterConfigMode(device, &previousMode);
    if (status != BNO055_ERROR_NONE)
    {
        return status;
    }

    status = Bno055_SelectPageInternal(device, PAGE_1);
    if (status == BNO055_ERROR_NONE)
    {
        status = Bno055_WriteRegisters(device, INT_EN_ADDR, &config->EnableMask, 1U);
    }

    if (status == BNO055_ERROR_NONE)
    {
        status = Bno055_WriteRegisters(device, INT_MSK_ADDR, &config->RouteMask, 1U);
    }

    if (status == BNO055_ERROR_NONE)
    {
        status = Bno055_SelectPageInternal(device, previousPage);
    }

    {
        BNO055_Error_t restoreStatus = Bno055_RestoreMode(device, previousMode);
        if ((status == BNO055_ERROR_NONE) && (restoreStatus != BNO055_ERROR_NONE))
        {
            status = restoreStatus;
        }
    }

    return status;
}

static BNO055_Error_t Bno055_ResetInternal(BNO055_Device_t* device)
{
	uint8_t resetCommand = 0x20U;
	uint8_t chipId = 0U;
	uint32_t attempt = 0U;
	BNO055_Error_t status = Bno055_SelectPageInternal(device, PAGE_0);

	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_WriteRegisters(device, SYS_TRIGGER_ADDR, &resetCommand, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	HAL_Delay(500U);

	for (attempt = 0U; attempt < 10U; ++attempt)
	{
		status = Bno055_ReadRegisters(device, CHIP_ID_ADDR, &chipId, 1U);
		if ((status == BNO055_ERROR_NONE) && (chipId == BNO055_ID))
		{
			device->CurrentPage = PAGE_0;
			device->CurrentMode = CONFIG_MODE;
			return BNO055_ERROR_NONE;
		}

		HAL_Delay(50U);
	}

	return BNO055_ERROR_DEVICE_ID;
}

static BNO055_Error_t Bno055_ReadStatusInternal(BNO055_Device_t* device, BNO_Status_t* statusInfo)
{
	BNO055_Error_t status = BNO055_ERROR_NONE;
	uint8_t value = 0U;

	if (statusInfo == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_ReadRegisters(device, ST_RESULT_ADDR, &value, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}
	statusInfo->STresult = value;

	status = Bno055_ReadRegisters(device, SYS_STATUS_ADDR, &value, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}
	statusInfo->SYSStatus = value;

	status = Bno055_ReadRegisters(device, SYS_ERR_ADDR, &value, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}
	statusInfo->SYSError = value;

	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_ReadCalibrationInternal(BNO055_Device_t* device, Calib_status_t* calibration)
{
	uint8_t calData = 0U;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (calibration == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_ReadRegisters(device, CALIB_STAT_ADDR, &calData, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	calibration->System = (calData >> 6) & 0x03U;
	calibration->Gyro = (calData >> 4) & 0x03U;
	calibration->Acc = (calData >> 2) & 0x03U;
	calibration->MAG = calData & 0x03U;
	return BNO055_ERROR_NONE;
}

static bool Bno055_IsCalibrationSatisfied(Op_Modes_t mode, const Calib_status_t* calibration)
{
	if (calibration == NULL)
	{
		return false;
	}

	switch (mode)
	{
	case ACC_ONLY:
		return calibration->Acc == 3U;
	case MAG_ONLY:
		return calibration->MAG == 3U;
	case GYRO_ONLY:
	case M4G:
		return calibration->Gyro == 3U;
	case ACC_MAG:
	case COMPASS:
		return (calibration->Acc == 3U) && (calibration->MAG == 3U);
	case ACC_GYRO:
	case IMU:
		return (calibration->Acc == 3U) && (calibration->Gyro == 3U);
	case MAG_GYRO:
		return (calibration->MAG == 3U) && (calibration->Gyro == 3U);
	case AMG:
	case NDOF_FMC_OFF:
	case NDOF:
		return (calibration->System == 3U) &&
		       (calibration->Gyro == 3U) &&
		       (calibration->Acc == 3U) &&
		       (calibration->MAG == 3U);
	case CONFIG_MODE:
	default:
		return false;
	}
}

static BNO055_Error_t Bno055_ReadOffsetsInternal(BNO055_Device_t* device, uint8_t* data, uint16_t length)
{
	Op_Modes_t previousMode = CONFIG_MODE;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if ((data == NULL) || (length != BNO055_OFFSET_DATA_LENGTH))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_EnterConfigMode(device, &previousMode);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_SelectPageInternal(device, PAGE_0);
	if (status == BNO055_ERROR_NONE)
	{
		status = Bno055_ReadRegisters(device, ACC_OFFSET_X_LSB_ADDR, data, length);
	}

	(void) Bno055_RestoreMode(device, previousMode);
	return status;
}

static BNO055_Error_t Bno055_WriteOffsetsInternal(BNO055_Device_t* device, const uint8_t* data, uint16_t length)
{
	Op_Modes_t previousMode = CONFIG_MODE;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if ((data == NULL) || (length != BNO055_OFFSET_DATA_LENGTH))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_EnterConfigMode(device, &previousMode);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_SelectPageInternal(device, PAGE_0);
	if (status == BNO055_ERROR_NONE)
	{
		status = Bno055_WriteRegisters(device, ACC_OFFSET_X_LSB_ADDR, data, length);
	}

	(void) Bno055_RestoreMode(device, previousMode);
	return status;
}

static BNO055_Error_t Bno055_IsFullyCalibratedInternal(BNO055_Device_t* device, bool* result)
{
	Calib_status_t calibration = {0};
	Op_Modes_t mode = CONFIG_MODE;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (result == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_ReadCalibrationInternal(device, &calibration);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_GetCurrentModeInternal(device, &mode);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	*result = Bno055_IsCalibrationSatisfied(mode, &calibration);

	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_ReadVector(BNO055_Device_t* device,
	                                   uint8_t baseAddress,
	                                   uint8_t rawLength,
	                                   float scale,
	                                   BNO055_Data_XYZ_t* destination)
{
	uint8_t buffer[6] = {0U};
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if ((destination == NULL) || (rawLength != 6U))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_ReadRegisters(device, baseAddress, buffer, rawLength);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	destination->X = (float) ((int16_t) ((buffer[1] << 8) | buffer[0])) / scale;
	destination->Y = (float) ((int16_t) ((buffer[3] << 8) | buffer[2])) / scale;
	destination->Z = (float) ((int16_t) ((buffer[5] << 8) | buffer[4])) / scale;
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_ReadQuaternion(BNO055_Device_t* device, BNO055_QuaData_WXYZ_t* destination)
{
	uint8_t buffer[8] = {0U};
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (destination == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_ReadRegisters(device, BNO_QUATERNION, buffer, sizeof(buffer));
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	destination->W = (float) ((int16_t) ((buffer[1] << 8) | buffer[0])) / (float) (1U << 14);
	destination->X = (float) ((int16_t) ((buffer[3] << 8) | buffer[2])) / (float) (1U << 14);
	destination->Y = (float) ((int16_t) ((buffer[5] << 8) | buffer[4])) / (float) (1U << 14);
	destination->Z = (float) ((int16_t) ((buffer[7] << 8) | buffer[6])) / (float) (1U << 14);
	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_ReadSensorsInternal(BNO055_Device_t* device,
	                                             BNO055_Sensors_t* sensorData,
	                                             BNO055_Sensor_Type sensors)
{
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (sensorData == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	if ((sensors & SENSOR_GRAVITY) != 0U)
	{
		status = Bno055_ReadVector(device, BNO_GRAVITY, 6U, 100.0f, &sensorData->Gravity);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	if ((sensors & SENSOR_QUATERNION) != 0U)
	{
		status = Bno055_ReadQuaternion(device, &sensorData->Quaternion);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	if ((sensors & SENSOR_LINACC) != 0U)
	{
		status = Bno055_ReadVector(device, BNO_LINACC, 6U, 100.0f, &sensorData->LineerAcc);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	if ((sensors & SENSOR_GYRO) != 0U)
	{
		status = Bno055_ReadVector(device, BNO_GYRO, 6U, 16.0f, &sensorData->Gyro);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	if ((sensors & SENSOR_ACCEL) != 0U)
	{
		status = Bno055_ReadVector(device, BNO_ACCEL, 6U, 100.0f, &sensorData->Accel);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	if ((sensors & SENSOR_MAG) != 0U)
	{
		status = Bno055_ReadVector(device, BNO_MAG, 6U, 16.0f, &sensorData->Magneto);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	if ((sensors & SENSOR_EULER) != 0U)
	{
		status = Bno055_ReadVector(device, BNO_EULER, 6U, 16.0f, &sensorData->Euler);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	return BNO055_ERROR_NONE;
}

static BNO055_Error_t Bno055_ApplyInit(BNO055_Device_t* device, const BNO055_Init_t* init)
{
	uint8_t clockStatus = 0U;
	Op_Modes_t previousMode = CONFIG_MODE;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if (init == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_EnterConfigMode(device, &previousMode);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	if (!Bno055_IsFusionMode((Op_Modes_t) init->OP_Modes))
	{
		status = Bno055_SetAccelRangeInternal(device, init->ACC_Range);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	status = Bno055_SelectPageInternal(device, PAGE_0);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_ReadRegisters(device, SYS_CLK_STATUS_ADDR, &clockStatus, 1U);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	if (clockStatus == 0U)
	{
		status = Bno055_SetClockSourceInternal(device, init->Clock_Source);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
	}

	status = Bno055_SetAxisInternal(device, init->Axis, init->Axis_sign);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_SetUnitSelectionInternal(device, init->Unit_Sel);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_SetPowerModeInternal(device, init->Mode);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	status = Bno055_SetOperationModeInternal(device, (Op_Modes_t) init->OP_Modes);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	if (init->ConfigureInterruptsOnOpen)
	{
		return Bno055_ConfigureInterruptsInternal(device, &init->InterruptConfig);
	}

	return BNO055_ERROR_NONE;
}

BNO055_Error_t Bno055_Open(void* vpParam)
{
	BNO055_OpenParams_t* openParams = (BNO055_OpenParams_t*) vpParam;
	BNO055_Device_t* device = NULL;
	BNO055_Error_t status = BNO055_ERROR_NONE;
	uint8_t chip_id = 0U;

	if (openParams == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	device = Bno055_GetDevice(openParams->DeviceIndex);
	if (device == NULL)
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	memset(device, 0, sizeof(*device));
	device->BusIndex = openParams->Hardware.I2C.I2CIndex;

	status = Bno055_FromHalStatus(Bno055_HardwareInit(&openParams->Hardware));
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	device->CurrentPage = PAGE_0;
	device->CurrentMode = CONFIG_MODE;
	device->IsOpen = 1U;

	status = Bno055_ResetInternal(device);
	if (status != BNO055_ERROR_NONE)
	{
		device->IsOpen = 0U;
		return status;
	}

	status = Bno055_ApplyInit(device, &openParams->Init);
	if (status != BNO055_ERROR_NONE)
	{
		device->IsOpen = 0U;
		return status;
	}

	status = Bno055_ReadRegisters(device, CHIP_ID_ADDR, &chip_id, 1U);
	if ((status != BNO055_ERROR_NONE) || (chip_id != BNO055_ID))
	{
		device->IsOpen = 0U;
	}
	return status;
}

BNO055_Error_t Bno055_Ioctl(BNO055_IOCTL_COMMANDS_T command, void* vpParam)
{
	switch (command)
	{
	case BNO055_IOCTL_GET_STATUS:
	{
		BNO055_StatusRequest_t* request = (BNO055_StatusRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if ((request == NULL) || (request->Status == NULL))
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_ReadStatusInternal(device, request->Status);
	}
	case BNO055_IOCTL_READ_REGISTER:
	{
		BNO055_RegisterRequest_t* request = (BNO055_RegisterRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_ReadRegisters(device,
		                            request->RegisterAddress,
		                            request->Data,
		                            request->Length);
	}
	case BNO055_IOCTL_SELECT_PAGE:
	{
		BNO055_PageRequest_t* request = (BNO055_PageRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_SelectPageInternal(device, request->Page);
	}
	case BNO055_IOCTL_RESET:
	{
		BNO055_DeviceRequest_t* request = (BNO055_DeviceRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_ResetInternal(device);
	}
	case BNO055_IOCTL_SET_OPERATION_MODE:
	{
		BNO055_OperationModeRequest_t* request = (BNO055_OperationModeRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_SetOperationModeInternal(device, request->Mode);
	}
	case BNO055_IOCTL_GET_OPERATION_MODE:
	{
		BNO055_OperationModeRequest_t* request = (BNO055_OperationModeRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_GetCurrentModeInternal(device, &request->Mode);
	}
	case BNO055_IOCTL_SET_POWER_MODE:
	{
		BNO055_PowerModeRequest_t* request = (BNO055_PowerModeRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_SetPowerModeInternal(device, request->Mode);
	}
	case BNO055_IOCTL_SET_CLOCK_SOURCE:
	{
		BNO055_ClockSourceRequest_t* request = (BNO055_ClockSourceRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_SetClockSourceInternal(device, request->Source);
	}
	case BNO055_IOCTL_SET_AXIS:
	{
		BNO055_AxisRequest_t* request = (BNO055_AxisRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		Op_Modes_t previousMode = CONFIG_MODE;
		BNO055_Error_t status = BNO055_ERROR_NONE;

		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}

		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}

		status = Bno055_EnterConfigMode(device, &previousMode);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}

		status = Bno055_SetAxisInternal(device, request->Remap, request->Sign);
		(void) Bno055_RestoreMode(device, previousMode);
		return status;
	}
	case BNO055_IOCTL_SET_ACCEL_RANGE:
	{
		BNO055_AccelRangeRequest_t* request = (BNO055_AccelRangeRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		Op_Modes_t previousMode = CONFIG_MODE;
		BNO055_Error_t status = BNO055_ERROR_NONE;

		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}

		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}

		status = Bno055_EnterConfigMode(device, &previousMode);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}

		status = Bno055_SetAccelRangeInternal(device, request->Range);
		(void) Bno055_SelectPageInternal(device, PAGE_0);
		(void) Bno055_RestoreMode(device, previousMode);
		return status;
	}
	case BNO055_IOCTL_SET_UNIT_SELECTION:
	{
		BNO055_UnitSelectionRequest_t* request = (BNO055_UnitSelectionRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		Op_Modes_t previousMode = CONFIG_MODE;
		BNO055_Error_t status = BNO055_ERROR_NONE;

		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}

		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}

		status = Bno055_EnterConfigMode(device, &previousMode);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}

		status = Bno055_SetUnitSelectionInternal(device, request->UnitSelection);
		(void) Bno055_RestoreMode(device, previousMode);
		return status;
	}
	case BNO055_IOCTL_GET_CALIBRATION:
	{
		BNO055_CalibrationRequest_t* request = (BNO055_CalibrationRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if ((request == NULL) || (request->Calibration == NULL))
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_ReadCalibrationInternal(device, request->Calibration);
	}
	case BNO055_IOCTL_GET_SENSOR_OFFSETS:
	{
		BNO055_OffsetsRequest_t* request = (BNO055_OffsetsRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_ReadOffsetsInternal(device, request->Data, request->Length);
	}
	case BNO055_IOCTL_SET_SENSOR_OFFSETS:
	{
		BNO055_OffsetsRequest_t* request = (BNO055_OffsetsRequest_t*) vpParam;
		BNO055_Device_t* device = NULL;
		BNO055_Error_t status = BNO055_ERROR_NONE;
		if (request == NULL)
		{
			return BNO055_ERROR_INVALID_PARAM;
		}
		status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
		if (status != BNO055_ERROR_NONE)
		{
			return status;
		}
		return Bno055_WriteOffsetsInternal(device, request->Data, request->Length);
	}
		case BNO055_IOCTL_IS_FULLY_CALIBRATED:
		{
			BNO055_BoolRequest_t* request = (BNO055_BoolRequest_t*) vpParam;
			BNO055_Device_t* device = NULL;
			BNO055_Error_t status = BNO055_ERROR_NONE;
			if ((request == NULL) || (request->Result == NULL))
			{
				return BNO055_ERROR_INVALID_PARAM;
			}
			status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
			if (status != BNO055_ERROR_NONE)
			{
				return status;
			}
			return Bno055_IsFullyCalibratedInternal(device, request->Result);
		}
		case BNO055_IOCTL_CONFIGURE_INTERRUPTS:
		{
			BNO055_InterruptConfigRequest_t* request = (BNO055_InterruptConfigRequest_t*) vpParam;
			BNO055_Device_t* device = NULL;
			BNO055_Error_t status = BNO055_ERROR_NONE;
			if (request == NULL)
			{
				return BNO055_ERROR_INVALID_PARAM;
			}
			status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
			if (status != BNO055_ERROR_NONE)
			{
				return status;
			}
			return Bno055_ConfigureInterruptsInternal(device, &request->Config);
		}

		default:
			return BNO055_ERROR_UNSUPPORTED;
	}
}

BNO055_Error_t Bno055_Write(void* vpParam, uint32_t size)
{
	BNO055_WriteRequest_t* request = (BNO055_WriteRequest_t*) vpParam;
	BNO055_Device_t* device = NULL;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if ((request == NULL) || (size != sizeof(BNO055_WriteRequest_t)))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	return Bno055_WriteRegisters(device,
	                             request->RegisterAddress,
	                             request->Data,
	                             request->Length);
}

BNO055_Error_t Bno055_Read(void* vpParam, uint32_t size)
{
	BNO055_ReadRequest_t* request = (BNO055_ReadRequest_t*) vpParam;
	BNO055_Device_t* device = NULL;
	BNO055_Error_t status = BNO055_ERROR_NONE;

	if ((request == NULL) || (size != sizeof(BNO055_ReadRequest_t)))
	{
		return BNO055_ERROR_INVALID_PARAM;
	}

	status = Bno055_GetOpenDevice(request->DeviceIndex, &device);
	if (status != BNO055_ERROR_NONE)
	{
		return status;
	}

	return Bno055_ReadSensorsInternal(device, request->SensorData, request->Sensors);
}
