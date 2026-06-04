
#include "Application.h"
#include "ImuReferenceController.h"

void Initialize()
{
	const IcdFrameTransmitterConfig_t icdTxConfig = {
		.Write = WriteIcdFrame,
		.IsBusy = IsIcdFrameTxBusy,
		.Context = GetDebugUartTransfer(),
	};
	IcdPublisher.Configure(icdTxConfig);
	ImuReferenceController_Init();
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
			ImuReferenceController_NotifyManualMotorCommand(command.Payload.MotorControl.Motor1AngleDeg,
			                                                command.Payload.MotorControl.Motor2AngleDeg);
		}
		else if (command.Header.IcdType == IcdType_ImuReferenceControl)
		{
			ImuReferenceController_ApplyControlCommand(&command.Payload.ImuReferenceControl);
		}
		else if (command.Header.IcdType == IcdType_ImuReferenceTuning)
		{
			ImuReferenceController_ApplyTuningCommand(&command.Payload.ImuReferenceTuning);
		}
	}
}

