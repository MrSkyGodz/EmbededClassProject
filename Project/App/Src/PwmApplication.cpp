/*
 * PwmApplication.cpp
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */


#include "PwmApplication.h"
#include "pwm.h"

void SetRedLedDimmer(uint8_t Dimmer)
{
	PWM_SetLedRedDuty(Dimmer);
}




