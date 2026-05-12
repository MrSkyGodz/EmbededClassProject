#include "motor.h"

#include "drv_motor/inc/drv_motor.h"

static DrvMotor_HardwareInit_t g_motor1Hardware =
{
	.PwmIndex = MOTOR1_PWM_INDEX,
	.Instance = MOTOR1_PWM_INSTANCE,
	.Channel = MOTOR1_PWM_CHANNEL,
	.GPIO_Init = {
		.GPIOIndex = MOTOR1_GPIO_INDEX,
		.Port = MOTOR1_GPIO_Port,
		.Pin = MOTOR1_Pin,
		.Function = MOTOR1_PWM_GPIO_AF,
	},
	.TIMHandle = NULL,
	.PwmFrequencyHz = MOTOR_SERVO_PWM_FREQUENCY_HZ,
};

static DrvMotor_HardwareInit_t g_motor2Hardware =
{
	.PwmIndex = MOTOR2_PWM_INDEX,
	.Instance = MOTOR2_PWM_INSTANCE,
	.Channel = MOTOR2_PWM_CHANNEL,
	.GPIO_Init = {
		.GPIOIndex = MOTOR2_GPIO_INDEX,
		.Port = MOTOR2_GPIO_Port,
		.Pin = MOTOR2_Pin,
		.Function = MOTOR2_PWM_GPIO_AF,
	},
	.TIMHandle = NULL,
	.PwmFrequencyHz = MOTOR_SERVO_PWM_FREQUENCY_HZ,
};

static DrvMotor_OpenParams_t g_motor1OpenParams =
{
	.MotorIndex = MOTOR1_INDEX,
	.Init = {
		.Type = MOTOR1_TYPE,
		.MinPulseUs = MOTOR1_SERVO_MIN_PULSE_US,
		.CenterPulseUs = MOTOR1_SERVO_CENTER_PULSE_US,
		.MaxPulseUs = MOTOR1_SERVO_MAX_PULSE_US,
		.MaxAngleDeg = MOTOR1_SERVO_MAX_ANGLE_DEG,
	},
};

static DrvMotor_OpenParams_t g_motor2OpenParams =
{
	.MotorIndex = MOTOR2_INDEX,
	.Init = {
		.Type = MOTOR2_TYPE,
		.MinPulseUs = MOTOR2_SERVO_MIN_PULSE_US,
		.CenterPulseUs = MOTOR2_SERVO_CENTER_PULSE_US,
		.MaxPulseUs = MOTOR2_SERVO_MAX_PULSE_US,
		.MaxAngleDeg = MOTOR2_SERVO_MAX_ANGLE_DEG,
	},
};

void MOTOR_Init(void)
{
	g_motor1OpenParams.Hardware = g_motor1Hardware;
	g_motor2OpenParams.Hardware = g_motor2Hardware;

	if ((DrvMotor_Open(&g_motor1OpenParams) != HAL_OK) ||
	    (DrvMotor_Open(&g_motor2OpenParams) != HAL_OK))
	{
		Error_Handler();
	}
}

HAL_StatusTypeDef MOTOR_SetMotor1Duty(uint32_t duty)
{
	DrvMotor_DutyRequest_t request =
	{
		.MotorIndex = MOTOR1_INDEX,
		.Duty = duty,
		.DutyMax = MOTOR_SERVO_DUTY_MAX,
	};

	return DrvMotor_SetDuty(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor2Duty(uint32_t duty)
{
	DrvMotor_DutyRequest_t request =
	{
		.MotorIndex = MOTOR2_INDEX,
		.Duty = duty,
		.DutyMax = MOTOR_SERVO_DUTY_MAX,
	};

	return DrvMotor_SetDuty(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor1PulseUs(uint32_t pulseUs)
{
	DrvMotor_PulseRequest_t request =
	{
		.MotorIndex = MOTOR1_INDEX,
		.PulseUs = pulseUs,
	};

	return DrvMotor_SetPulseUs(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor2PulseUs(uint32_t pulseUs)
{
	DrvMotor_PulseRequest_t request =
	{
		.MotorIndex = MOTOR2_INDEX,
		.PulseUs = pulseUs,
	};

	return DrvMotor_SetPulseUs(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor1Angle(float angleDeg)
{
	DrvMotor_AngleRequest_t request =
	{
		.MotorIndex = MOTOR1_INDEX,
		.AngleDeg = angleDeg,
	};

	return DrvMotor_SetAngle(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor2Angle(float angleDeg)
{
	DrvMotor_AngleRequest_t request =
	{
		.MotorIndex = MOTOR2_INDEX,
		.AngleDeg = angleDeg,
	};

	return DrvMotor_SetAngle(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor1Speed(int32_t speed)
{
	DrvMotor_SpeedRequest_t request =
	{
		.MotorIndex = MOTOR1_INDEX,
		.Speed = speed,
		.SpeedMax = MOTOR_SERVO_SPEED_MAX,
	};

	return DrvMotor_SetSpeed(&request);
}

HAL_StatusTypeDef MOTOR_SetMotor2Speed(int32_t speed)
{
	DrvMotor_SpeedRequest_t request =
	{
		.MotorIndex = MOTOR2_INDEX,
		.Speed = speed,
		.SpeedMax = MOTOR_SERVO_SPEED_MAX,
	};

	return DrvMotor_SetSpeed(&request);
}
