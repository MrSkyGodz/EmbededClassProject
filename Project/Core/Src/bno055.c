/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bno055.c
  * @brief   BNO055 glue layer backed by drv_bno055 / drv_i2c.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "bno055.h"
#include "i2c.h"
#include "drv_bno055/inc/bno055_hal.h"

#include "board.h"

BNO_Status_t BnoStatus = {0};
BNO055_Sensors_t BnoSensors = {0};
BNO055_StatusRequest_t BnoStatusRequest = {.DeviceIndex = BNO055_DEVICE_INDEX,
											.Status = &BnoStatus};
BNO055_ReadRequest_t BnoReadRequest = {.DeviceIndex = BNO055_DEVICE_INDEX,
                                       .Sensors = (BNO055_Sensor_Type) (SENSOR_EULER | SENSOR_GYRO)};

static Bno055_HardwareInit_t g_bnoHardware = {
    .I2C = {
        .I2CIndex = 2U,
        .I2C_Init = {
            .Instance = I2C2,
            .ClockSpeed = 0x20303E5DU
        },
        .GPIO_SDAInit = {
            .GPIOIndex = BNO055_I2C_SDA_GPIO_INDEX,
            .Port = GPIOF,
            .Pin = GPIO_PIN_0,
            .Function = GPIO_AF4_I2C2
        },
        .GPIO_SCLInit = {
            .GPIOIndex = BNO055_I2C_SCL_GPIO_INDEX,
            .Port = GPIOF,
            .Pin = GPIO_PIN_1,
            .Function = GPIO_AF4_I2C2
        },
        .I2CHandle = NULL
    },
	.IntPin = {
		.GPIOIndex = BNO055_INT_GPIO_INDEX,
		.Port = BNO055_INT_GPIO_Port,
		.Pin = BNO055_INT_Pin,
		.Mode = GPIO_MODE_IT_RISING,
		.Pull = GPIO_NOPULL,
		.InitInterrupt = {
				.InterruptEnabled = 1,
				.InterruptIRQn = BNO055_INT_GPIO_EXTI_IRQn,
				.InterruptPriority = 6,
				.InterruptSubPriority = 0,
		},
	}
};

static BNO055_OpenParams_t g_bnoOpenParams = {
    .DeviceIndex = BNO055_DEVICE_INDEX,
    .Init = {
        .Unit_Sel = UNIT_ORI_WINDOWS | UNIT_TEMP_CELCIUS | UNIT_EUL_DEG |
                    UNIT_GYRO_DPS | UNIT_ACC_MS2,
        .Axis = DEFAULT_AXIS_REMAP,
        .Axis_sign = DEFAULT_AXIS_SIGN,
        .Mode = BNO055_NORMAL_MODE,
        .OP_Modes = IMU,
        .Clock_Source = CLOCK_INTERNAL,
        .ACC_Range = Range_4G,
        .ConfigureInterruptsOnOpen = true,
        .InterruptConfig = {
            .EnableMask = BNO055_INT_ACC_BSX_DRDY,
            .RouteMask = BNO055_INT_ACC_BSX_DRDY
        }
    }
};

BNO055_Error_t BNO055_Init(void)
{
    BNO055_Error_t status = BNO055_ERROR_NONE;

    g_bnoOpenParams.Hardware = g_bnoHardware;

    status = Bno055_Open(&g_bnoOpenParams);
    if (status != BNO055_ERROR_NONE)
    {
        return status;
    }

    return Bno055_Ioctl(BNO055_IOCTL_GET_STATUS, &BnoStatusRequest);
}
