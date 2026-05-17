#include "Application.h"
#include "IcdUartPublisher.hpp"
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

IcdUartPublisher IcdPublisher(DebugUartTransfer);
static uint8_t TelemetryCounter = 0U;

static IcdMessage_t BuildBno055TelemetryMessage(const BNO055_Sensors_t* sample)
{
	IcdMessage_t message = {};
	message.Header.TimetagMs = HAL_GetTick();
	message.Header.Counter = TelemetryCounter++;
	message.Header.IcdType = IcdType_Bno055Telemetry;
	message.Payload.Bno055Telemetry.EulerX = sample->Euler.X;
	message.Payload.Bno055Telemetry.EulerY = sample->Euler.Y;
	message.Payload.Bno055Telemetry.EulerZ = sample->Euler.Z;
	message.Payload.Bno055Telemetry.GyroX = sample->Gyro.X;
	message.Payload.Bno055Telemetry.GyroY = sample->Gyro.Y;
	message.Payload.Bno055Telemetry.GyroZ = sample->Gyro.Z;
	return message;
}

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
		IcdMessage_t command = {};

		__disable_irq();
		memcpy(&command, (const void *) &parser.receivedCommand, sizeof(command));
		parser.commandReady = 0U;
		__enable_irq();

		if (command.Header.IcdType == IcdType_PWMControl)
		{
			(void) PWM_SetLedRedDuty(command.Payload.PWMControl.Pwm);
		}
		else if (command.Header.IcdType == IcdType_MotorControl)
		{
			(void) MOTOR_SetMotor1Angle(command.Payload.MotorControl.Motor1AngleDeg);
			(void) MOTOR_SetMotor2Angle(command.Payload.MotorControl.Motor2AngleDeg);
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

	const IcdMessage_t message = BuildBno055TelemetryMessage(sample);
	(void) IcdPublisher.Publish(message);
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
