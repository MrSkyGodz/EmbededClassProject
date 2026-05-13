#ifndef BNO055_DRIVER_H_
#define BNO055_DRIVER_H_

#include "bno055_const.h"
#include "bno055_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint8_t EnableMask;
	uint8_t RouteMask;
} BNO055_InterruptConfig_t;

typedef struct
{
	uint8_t Unit_Sel;
	uint8_t Axis;
	uint8_t Axis_sign;
	uint8_t Mode;
	uint8_t OP_Modes;
	uint8_t Clock_Source;
	uint8_t ACC_Range;
	bool ConfigureInterruptsOnOpen;
	BNO055_InterruptConfig_t InterruptConfig;
} BNO055_Init_t;

#define BNO055_NORMAL_MODE  0U
#define BNO055_LOWPOWER_MODE 1U
#define BNO055_SUSPEND_MODE 2U

#define DEFAULT_AXIS_REMAP 0x24U
#define DEFAULT_AXIS_SIGN  0x00U

#define CLOCK_EXTERNAL (1U << 7)
#define CLOCK_INTERNAL (0U << 7)

#define UNIT_ORI_ANDROID    (1U << 7)
#define UNIT_ORI_WINDOWS    (0U << 7)
#define UNIT_TEMP_CELCIUS   (0U << 4)
#define UNIT_TEMP_FAHRENHEIT (1U << 4)
#define UNIT_EUL_DEG        (0U << 2)
#define UNIT_EUL_RAD        (1U << 2)
#define UNIT_GYRO_DPS       (0U << 1)
#define UNIT_GYRO_RPS       (1U << 1)
#define UNIT_ACC_MS2        (0U << 0)
#define UNIT_ACC_MG         (1U << 0)

#define Range_2G  0x00U
#define Range_4G  0x01U
#define Range_8G  0x02U
#define Range_16G 0x03U

#define BNO055_INT_ACC_BSX_DRDY (1U << 0)
#define BNO055_INT_MAG_DRDY     (1U << 1)
#define BNO055_INT_GYRO_AM      (1U << 2)
#define BNO055_INT_GYR_HIGH_RATE (1U << 3)
#define BNO055_INT_GYR_DRDY     (1U << 4)
#define BNO055_INT_ACC_HIGH_G   (1U << 5)
#define BNO055_INT_ACC_AM       (1U << 6)
#define BNO055_INT_ACC_NM       (1U << 7)

#define BNO055_OFFSET_DATA_LENGTH 22U
#define BNO055_CALIBRATION_DEFAULT_TIMEOUT_MS 30000U
#define BNO055_CALIBRATION_DEFAULT_POLL_MS 500U

typedef enum
{
	BNO055_ERROR_NONE = 0,
	BNO055_ERROR_INVALID_PARAM,
	BNO055_ERROR_INVALID_STATE,
	BNO055_ERROR_HAL,
	BNO055_ERROR_BUSY,
	BNO055_ERROR_TIMEOUT,
	BNO055_ERROR_DEVICE_ID,
	BNO055_ERROR_UNSUPPORTED
} BNO055_Error_t;

#define BNO055_MAX_INSTANCES MAX_BO055

typedef struct
{
	uint32_t DeviceIndex;
	Bno055_HardwareInit_t Hardware;
	BNO055_Init_t Init;
} BNO055_OpenParams_t;

typedef struct
{
	uint32_t DeviceIndex;
	BNO055_Sensors_t* SensorData;
	BNO055_Sensor_Type Sensors;
} BNO055_ReadRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
} BNO055_DeviceRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t RegisterAddress;
	const uint8_t* Data;
	uint16_t Length;
} BNO055_WriteRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t RegisterAddress;
	uint8_t* Data;
	uint16_t Length;
} BNO055_RegisterRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	BNO_Status_t* Status;
} BNO055_StatusRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t Page;
} BNO055_PageRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	Op_Modes_t Mode;
} BNO055_OperationModeRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t Mode;
} BNO055_PowerModeRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t Source;
} BNO055_ClockSourceRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t Remap;
	uint8_t Sign;
} BNO055_AxisRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t Range;
} BNO055_AccelRangeRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t UnitSelection;
} BNO055_UnitSelectionRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	Calib_status_t* Calibration;
} BNO055_CalibrationRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	uint8_t* Data;
	uint16_t Length;
} BNO055_OffsetsRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	bool* Result;
} BNO055_BoolRequest_t;

typedef struct
{
	uint32_t DeviceIndex;
	BNO055_InterruptConfig_t Config;
} BNO055_InterruptConfigRequest_t;

typedef enum
{
	BNO055_IOCTL_GET_STATUS = 0,
	BNO055_IOCTL_READ_REGISTER,
	BNO055_IOCTL_SELECT_PAGE,
	BNO055_IOCTL_RESET,
	BNO055_IOCTL_SET_OPERATION_MODE,
	BNO055_IOCTL_GET_OPERATION_MODE,
	BNO055_IOCTL_SET_POWER_MODE,
	BNO055_IOCTL_SET_CLOCK_SOURCE,
	BNO055_IOCTL_SET_AXIS,
	BNO055_IOCTL_SET_ACCEL_RANGE,
	BNO055_IOCTL_SET_UNIT_SELECTION,
	BNO055_IOCTL_GET_CALIBRATION,
	BNO055_IOCTL_GET_SENSOR_OFFSETS,
	BNO055_IOCTL_SET_SENSOR_OFFSETS,
	BNO055_IOCTL_IS_FULLY_CALIBRATED,
	BNO055_IOCTL_CONFIGURE_INTERRUPTS,
} BNO055_IOCTL_COMMANDS_T;

BNO055_Error_t Bno055_Open(void* vpParam);
BNO055_Error_t Bno055_Ioctl(BNO055_IOCTL_COMMANDS_T command, void* vpParam);
BNO055_Error_t Bno055_Write(void* vpParam, uint32_t size);
BNO055_Error_t Bno055_Read(void* vpParam, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
