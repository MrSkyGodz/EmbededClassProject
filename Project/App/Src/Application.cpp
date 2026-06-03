
#include "Application.h"

void Initialize()
{
	const IcdFrameTransmitterConfig_t icdTxConfig = {
		.Write = WriteIcdFrame,
		.IsBusy = IsIcdFrameTxBusy,
		.Context = GetDebugUartTransfer(),
	};
	IcdPublisher.Configure(icdTxConfig);
	StartGpio();
	StartImu();
	UartStart();


}

void Loop()
{

	IcdPublisher.Process();

	if (parser.commandReady != 0U)
	{
		IcdMessage_t command = {};


		memcpy(&command, (const void *) &parser.receivedCommand, sizeof(command));
		parser.commandReady = 0U;


		if (command.Header.IcdType == IcdType_PWMControl)
		{
			SetRedLedDimmer(command.Payload.PWMControl.Pwm);
		}
		else if (command.Header.IcdType == IcdType_MotorControl)
		{
			SetMotorPositionReference(command.Payload.MotorControl.Motor1AngleDeg, command.Payload.MotorControl.Motor2AngleDeg);
		}
	}
}


