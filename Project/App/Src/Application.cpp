#include "Application.h"
#include "ParserImp.h"
#include "ComImp/UartComImp.h"


#include <stdio.h>
#include <string.h>

extern "C"
{
#include "bno055.h"
#include "gpio.h"
#include "main.h"
#include "pwm.h"
#include "motor.h"
#include "i2c.h"
#include "usart.h"
#include "drv_gpio/inc/drv_gpio.h"
#include "drv_uart/inc/drv_uart.h"
#include "drv_bno055/inc/bno055_driver.h"
}


ParserImp parser;
extern "C" void DebugUartDataCallback(DrvUart_Event_t event,
                                      const uint8_t* rxBuffer,
                                      size_t rxLength,
                                      void* userData)
{
	(void) event;
	(void) userData;

	for (size_t i = 0U; i < rxLength; ++i)
	{
		parser.pushByte(rxBuffer[i]);
	}
}


extern "C" void ButtonInterruptCallback(uint32_t gpioIndex,
                                      void* userData)
{
	static uint32_t ledStatus = 0U;

	ledStatus = !ledStatus;
	DrvGpio_Toggle(LED_BLUE_INDEX);
	const char *message = ledStatus != 0U ? "On\n" : "Off\n";
	printf("%s",message);
}



DrvUart_TransferParams_t DebugUartTransfer = {.UartIndex = USART3_UART_INDEX, .Timeout = HAL_MAX_DELAY};

DrvUart_CallbackRequest_t DebugUartCallbackRequest = {.UartIndex = USART3_UART_INDEX,
                                                      .Callback = nullptr,
                                                      .UserData = nullptr};

static uint8_t DebugUartRxBuffer[64U] = {0U};
DrvUart_AttachInterruptRequest_t DebugUartAttachRequest = {.UartIndex = USART3_UART_INDEX,
                                                           .Buffer = DebugUartRxBuffer,
                                                           .BufferLength = sizeof(DebugUartRxBuffer)};


DrvGpio_AttachInterruptRequest_t ButtonGpioAttachRequest =
{
	.GPIOIndex = BUTTON_GPIO_INDEX,
	.Callback = ButtonInterruptCallback,
	.UserData = 0,
};

void Initialize()
{
	if (BNO055_Init() != BNO055_ERROR_NONE)
	{
		Error_Handler();
	}
	printf("BNO055 status: ST=0x%02X SYS=%u ERR=%u\r\n", BnoStatus.STresult, BnoStatus.SYSStatus, BnoStatus.SYSError);

	if(DrvGpio_Ioctl(DRV_GPIO_IOCTL_ATTACH_INTERRUPT,&ButtonGpioAttachRequest) != HAL_OK)
	{
		Error_Handler();
	}


	DebugUartCallbackRequest.Callback = DebugUartDataCallback;
	if ((DrvUart_Ioctl(DRV_UART_IOCTL_REGISTER_CALLBACK, &DebugUartCallbackRequest) != HAL_OK) ||
	    (DrvUart_Ioctl(DRV_UART_IOCTL_ATTACH_INTERRUPT, &DebugUartAttachRequest) != HAL_OK))
	{
		Error_Handler();
	}

}

void Loop()
{
	if (parser.commandReady != 0U)
	{
		CommunicationProtocol_t command = {};

		__disable_irq();
		memcpy(&command, (const void *) &parser.receivedCommand, sizeof(command));
		parser.commandReady = 0U;
		__enable_irq();

		if (command.Header == Header_PWMControl)
		{
			(void) PWM_SetLedRedDuty(command.Cp.PWMControl.Pwm);
		}
		else if (command.Header == Header_MotorControl)
		{
			(void) MOTOR_SetMotor1Angle(command.Cp.MotorControl.Motor1AngleDeg);
			(void) MOTOR_SetMotor2Angle(command.Cp.MotorControl.Motor2AngleDeg);
		}
	}
}


bool ReadOneSensorSample(BNO055_Sensors_t* sample)
{
	if (sample == nullptr)
	{
		return false;
	}
	BnoReadRequest.SensorData = sample;
	if (Bno055_Read(&BnoReadRequest, sizeof(BnoReadRequest)) == BNO055_ERROR_NONE)
	{
		return true;
	}

	return false;
}

void PublishSensorSample(const BNO055_Sensors_t* sample)
{
	if (sample == nullptr)
	{
		return;
	}

	printf("Euler H/R/P: %.2f %.2f %.2f | Gyro XYZ: %.2f %.2f %.2f\r\n",
	       sample->Euler.X,
	       sample->Euler.Y,
	       sample->Euler.Z,
	       sample->Gyro.X,
	       sample->Gyro.Y,
	       sample->Gyro.Z);
}


extern "C" int _write(int file, char *ptr, int len)
{
  (void) file;

  if ((ptr == nullptr) || (len <= 0))
  {
    return 0;
  }

  return (DrvUart_Write(&DebugUartTransfer, ptr, (uint32_t) len) == HAL_OK) ? len : 0;
}
