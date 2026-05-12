/*
 * CommunicationType.h
 *
 *  Created on: 20 Mar 2026
 *      Author: akdal
 */

#ifndef TYPES_COMMUNICATIONTYPE_H_
#define TYPES_COMMUNICATIONTYPE_H_

#include <stdint.h>

typedef enum : uint8_t
{
	Header_PWMControl,
	Header_MotorControl,
	Header_MAX
}Header_e;
typedef struct
{
	uint8_t Pwm;
}PWMControl_t;

typedef struct
{
	float Motor1AngleDeg;
	float Motor2AngleDeg;
}MotorControl_t;

typedef union
{
	PWMControl_t PWMControl;
	MotorControl_t MotorControl;
}CommunicationProtocol_u;
typedef struct
{
	Header_e Header;
	CommunicationProtocol_u Cp;
}CommunicationProtocol_t;



#endif /* TYPES_COMMUNICATIONTYPE_H_ */
