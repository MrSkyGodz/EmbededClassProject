/*
 * MotorApplication.cpp
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */
#include "motor.h"


void SetMotorPositionReference(float angleDegMotor1,float angleDegMotor2)
{
	MOTOR_SetMotor1Angle(angleDegMotor1);
	MOTOR_SetMotor2Angle(angleDegMotor2);
}

