/*
 * CommunicationType.h
 *
 *  Created on: 20 Mar 2026
 *      Author: akdal
 */

#ifndef TYPES_COMMUNICATIONTYPE_H_
#define TYPES_COMMUNICATIONTYPE_H_

#include <stdint.h>

#define ICD_HEADER_SIZE_BYTES 6U

typedef enum : uint8_t
{
	IcdType_PWMControl,
	IcdType_MotorControl,
	IcdType_Bno055Telemetry,
	IcdType_MAX
}IcdType_e;

typedef struct
{
	uint32_t TimetagMs;
	uint8_t Counter;
	uint8_t IcdType;
}IcdHeader_t;

typedef struct
{
	uint8_t Pwm;
}PWMControl_t;

typedef struct
{
	float Motor1AngleDeg;
	float Motor2AngleDeg;
}MotorControl_t;

typedef struct
{
	float EulerX;
	float EulerY;
	float EulerZ;
	float GyroX;
	float GyroY;
	float GyroZ;
}Bno055Telemetry_t;

typedef union
{
	PWMControl_t PWMControl;
	MotorControl_t MotorControl;
	Bno055Telemetry_t Bno055Telemetry;
}IcdPayload_u;

typedef struct
{
	IcdHeader_t Header;
	IcdPayload_u Payload;
}IcdMessage_t;



#endif /* TYPES_COMMUNICATIONTYPE_H_ */
