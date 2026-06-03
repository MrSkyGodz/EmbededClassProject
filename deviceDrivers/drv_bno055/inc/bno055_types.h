/*
 * bno055_types.h
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */

#ifndef BNO055_TYPES_H_
#define BNO055_TYPES_H_


/** ----------------------------------------------------------------------------------------------------
  * 			 					  BNO055 status macros definition
  * ----------------------------------------------------------------------------------------------------
  */
typedef struct{
	uint8_t STresult;	//First 4 bit[0:3] indicates self test resulst 3:ST_MCU, 2:ST_GYRO, 1:ST_MAG, 0:ST_ACCEL
	uint8_t SYSError;	//Contains system error type If SYSStatus is System_Error(0x01)
	uint8_t SYSStatus;
}BNO_Status_t;

typedef struct{
	uint8_t System;
	uint8_t Gyro;
	uint8_t Acc;
	uint8_t MAG;
}Calib_status_t;

/** ----------------------------------------------------------------------------------------------------
  * 			 BNO055 Data output structures and  macros definition for ReadData function
  * ----------------------------------------------------------------------------------------------------
  */
typedef struct{ //SENSOR DATA AXIS X, Y and Z
	float X;
	float Y;
	float Z;
}BNO055_Data_XYZ_t;

typedef struct{ //SENSOR DATA AXIS W, X, Y and Z (Only for quaternion data)
	float W;
	float X;
	float Y;
	float Z;
}BNO055_QuaData_WXYZ_t;

typedef struct{ //SENSOR DATAS
	BNO055_Data_XYZ_t Accel;
	BNO055_Data_XYZ_t Gyro;
	BNO055_Data_XYZ_t Magneto;
	BNO055_Data_XYZ_t Euler;
	BNO055_Data_XYZ_t LineerAcc;
	BNO055_Data_XYZ_t Gravity;
	BNO055_QuaData_WXYZ_t Quaternion;
}BNO055_Sensors_t;

typedef enum {
    SENSOR_GRAVITY     = 0x01,
    SENSOR_QUATERNION  = 0x02,
    SENSOR_LINACC      = 0x04,
    SENSOR_GYRO        = 0x08,
    SENSOR_ACCEL       = 0x10,
    SENSOR_MAG         = 0x20,
    SENSOR_EULER       = 0x40,
} BNO055_Sensor_Type;



#endif /* DRV_BNO055_INC_BNO055_TYPES_H_ */
