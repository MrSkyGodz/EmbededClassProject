/*
 * GpioApplication.cpp
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */

#include "drv_gpio/inc/drv_gpio.h"
#include "gpio.h"
#include "stdio.h"

extern "C" void ButtonInterruptCallback(uint32_t gpioIndex,
                                      void* userData)
{
	static uint32_t ledStatus = 0U;

	ledStatus = !ledStatus;
	DrvGpio_Toggle(LED_BLUE_INDEX);
	const char *message = ledStatus != 0U ? "On\n" : "Off\n";
	printf("%s",message);
}


DrvGpio_AttachInterruptRequest_t ButtonGpioAttachRequest =
{
	.GPIOIndex = BUTTON_GPIO_INDEX,
	.Callback = ButtonInterruptCallback,
	.UserData = 0,
};



void StartGpio()
{
	if(DrvGpio_Ioctl(DRV_GPIO_IOCTL_ATTACH_INTERRUPT,&ButtonGpioAttachRequest) != HAL_OK)
	{
		Error_Handler();
	}
}
