#include "drv_motor/inc/drv_motor.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
	DrvMotor_OpenParams_t openParams;
	uint8_t isOpen;
} DrvMotor_Context_t;

static DrvMotor_Context_t g_motorContexts[DRV_MOTOR_MAX_INSTANCES];

static DrvMotor_Context_t* DrvMotor_GetContext(uint32_t index)
{
	if (index >= DRV_MOTOR_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_motorContexts[index];
}

static uint32_t DrvMotor_Clamp(uint32_t value, uint32_t minValue, uint32_t maxValue)
{
	if (value < minValue)
	{
		return minValue;
	}

	if (value > maxValue)
	{
		return maxValue;
	}

	return value;
}

static uint32_t DrvMotor_MapRange(uint32_t value,
                                  uint32_t inMax,
                                  uint32_t outMin,
                                  uint32_t outMax)
{
	if (inMax == 0U)
	{
		return outMin;
	}

	if (value > inMax)
	{
		value = inMax;
	}

	return outMin + (uint32_t) (((uint64_t) (outMax - outMin) * value) / inMax);
}

static uint32_t DrvMotor_MapFloatRange(float value,
                                       float inMax,
                                       uint32_t outMin,
                                       uint32_t outMax)
{
	float scaled = 0.0f;

	if (inMax <= 0.0f)
	{
		return outMin;
	}

	if (value < 0.0f)
	{
		value = 0.0f;
	}
	else if (value > inMax)
	{
		value = inMax;
	}

	scaled = ((float) (outMax - outMin) * value) / inMax;
	return outMin + (uint32_t) (scaled + 0.5f);
}

static uint32_t DrvMotor_MapSignedRange(int32_t value,
                                        int32_t inMax,
                                        uint32_t outMin,
                                        uint32_t outCenter,
                                        uint32_t outMax)
{
	uint32_t magnitude = 0U;

	if (inMax <= 0)
	{
		return outCenter;
	}

	if (value > inMax)
	{
		value = inMax;
	}
	else if (value < -inMax)
	{
		value = -inMax;
	}

	if (value >= 0)
	{
		magnitude = (uint32_t) value;
		return outCenter + (uint32_t) (((uint64_t) (outMax - outCenter) * magnitude) / (uint32_t) inMax);
	}

	magnitude = (uint32_t) (-value);
	return outCenter - (uint32_t) (((uint64_t) (outCenter - outMin) * magnitude) / (uint32_t) inMax);
}

static uint8_t DrvMotor_IsApb2Timer(TIM_TypeDef* instance)
{
	return (instance == TIM1) ||
	       (instance == TIM8) ||
	       (instance == TIM9) ||
	       (instance == TIM10) ||
	       (instance == TIM11);
}

static uint32_t DrvMotor_GetTimerClockHz(TIM_TypeDef* instance)
{
	uint32_t pclk = 0U;
	uint32_t ppre = 0U;

	if (DrvMotor_IsApb2Timer(instance) != 0U)
	{
		pclk = HAL_RCC_GetPCLK2Freq();
		ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
	}
	else
	{
		pclk = HAL_RCC_GetPCLK1Freq();
		ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
	}

	if (ppre >= 4U)
	{
		return pclk * 2U;
	}

	return pclk;
}

static void DrvMotor_ApplyHardwareDefaults(DrvMotor_HardwareInit_t* hardwareInit)
{
	if (hardwareInit->TimerTickHz == 0U)
	{
		hardwareInit->TimerTickHz = DRV_MOTOR_DEFAULT_TIMER_TICK_HZ;
	}

	if (hardwareInit->PwmFrequencyHz == 0U)
	{
		hardwareInit->PwmFrequencyHz = DRV_MOTOR_DEFAULT_PWM_FREQUENCY_HZ;
	}

	if (hardwareInit->TimerClockHz == 0U)
	{
		hardwareInit->TimerClockHz = DrvMotor_GetTimerClockHz(hardwareInit->Instance);
	}
}

static void DrvMotor_ApplyInitDefaults(DrvMotor_Init_t* init)
{
	if (init->MinPulseUs == 0U)
	{
		init->MinPulseUs = DRV_MOTOR_DEFAULT_MIN_PULSE_US;
	}

	if (init->CenterPulseUs == 0U)
	{
		init->CenterPulseUs = DRV_MOTOR_DEFAULT_CENTER_PULSE_US;
	}

	if (init->MaxPulseUs == 0U)
	{
		init->MaxPulseUs = DRV_MOTOR_DEFAULT_MAX_PULSE_US;
	}

	if (init->MaxAngleDeg <= 0.0f)
	{
		init->MaxAngleDeg = (float) DRV_MOTOR_DEFAULT_MAX_ANGLE_DEG;
	}
}

static HAL_StatusTypeDef DrvMotor_BuildPwmOpenParams(const DrvMotor_OpenParams_t* motorParams,
                                                     DrvPwm_OpenParams_t* pwmParams)
{
	DrvMotor_HardwareInit_t hardwareInit = {0};
	DrvMotor_Init_t init = {0};
	uint32_t timerPeriodTicks = 0U;

	if ((motorParams == NULL) ||
	    (pwmParams == NULL) ||
	    (motorParams->Hardware.Instance == NULL) ||
	    (motorParams->Hardware.GPIO_Init.Port == NULL) ||
	    (motorParams->Hardware.GPIO_Init.Pin == 0U))
	{
		return HAL_ERROR;
	}

	hardwareInit = motorParams->Hardware;
	init = motorParams->Init;
	DrvMotor_ApplyHardwareDefaults(&hardwareInit);
	DrvMotor_ApplyInitDefaults(&init);

	if ((hardwareInit.TimerClockHz == 0U) ||
	    (hardwareInit.TimerTickHz == 0U) ||
	    (hardwareInit.PwmFrequencyHz == 0U) ||
	    (hardwareInit.TimerClockHz < hardwareInit.TimerTickHz) ||
	    ((hardwareInit.TimerClockHz % hardwareInit.TimerTickHz) != 0U))
	{
		return HAL_ERROR;
	}

	timerPeriodTicks = hardwareInit.TimerTickHz / hardwareInit.PwmFrequencyHz;
	if (timerPeriodTicks == 0U)
	{
		return HAL_ERROR;
	}

	memset(pwmParams, 0, sizeof(*pwmParams));
	pwmParams->PwmIndex = hardwareInit.PwmIndex;
	pwmParams->PWM_Init.Instance = hardwareInit.Instance;
	pwmParams->PWM_Init.Prescaler = (hardwareInit.TimerClockHz / hardwareInit.TimerTickHz) - 1U;
	pwmParams->PWM_Init.CounterMode = TIM_COUNTERMODE_UP;
	pwmParams->PWM_Init.Period = timerPeriodTicks - 1U;
	pwmParams->PWM_Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	pwmParams->PWM_Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	pwmParams->ChannelInit.OCMode = TIM_OCMODE_PWM1;
	pwmParams->ChannelInit.Pulse = DrvMotor_Clamp(init.CenterPulseUs,
	                                              init.MinPulseUs,
	                                              init.MaxPulseUs);
	pwmParams->ChannelInit.OCPolarity = TIM_OCPOLARITY_HIGH;
	pwmParams->ChannelInit.OCFastMode = TIM_OCFAST_DISABLE;
	pwmParams->GPIO_Init = hardwareInit.GPIO_Init;
	pwmParams->Channel = hardwareInit.Channel;
	pwmParams->TIMHandle = hardwareInit.TIMHandle;
	pwmParams->InitGpio = 1U;

	return HAL_OK;
}

HAL_StatusTypeDef DrvMotor_Open(void* vpParam)
{
	const DrvMotor_OpenParams_t* openParams = (const DrvMotor_OpenParams_t*) vpParam;
	DrvMotor_Context_t* context = NULL;
	DrvPwm_OpenParams_t pwmOpenParams = {0};

	if ((openParams == NULL) ||
	    (openParams->MotorIndex >= DRV_MOTOR_MAX_INSTANCES) ||
	    (openParams->Hardware.PwmIndex >= DRV_PWM_MAX_INSTANCES))
	{
		return HAL_ERROR;
	}

	context = DrvMotor_GetContext(openParams->MotorIndex);
	if (context == NULL)
	{
		return HAL_ERROR;
	}

	if (DrvMotor_BuildPwmOpenParams(openParams, &pwmOpenParams) != HAL_OK)
	{
		return HAL_ERROR;
	}

	memset(context, 0, sizeof(*context));
	context->openParams = *openParams;
	DrvMotor_ApplyHardwareDefaults(&context->openParams.Hardware);
	DrvMotor_ApplyInitDefaults(&context->openParams.Init);

	if (DrvPwm_Open(&pwmOpenParams) != HAL_OK)
	{
		memset(context, 0, sizeof(*context));
		return HAL_ERROR;
	}

	if (DrvPwm_Start(openParams->Hardware.PwmIndex) != HAL_OK)
	{
		DrvPwm_DeviceParams_t pwmDeviceParams = {.PwmIndex = openParams->Hardware.PwmIndex};

		(void) DrvPwm_Close(&pwmDeviceParams);
		memset(context, 0, sizeof(*context));
		return HAL_ERROR;
	}

	context->isOpen = 1U;
	return HAL_OK;
}

HAL_StatusTypeDef DrvMotor_Close(void* vpParam)
{
	const DrvMotor_DeviceParams_t* deviceParams = (const DrvMotor_DeviceParams_t*) vpParam;
	DrvMotor_Context_t* context = NULL;
	DrvPwm_DeviceParams_t pwmDeviceParams = {0};

	if (deviceParams == NULL)
	{
		return HAL_ERROR;
	}

	context = DrvMotor_GetContext(deviceParams->MotorIndex);
	if ((context == NULL) || (context->isOpen == 0U))
	{
		return HAL_ERROR;
	}

	pwmDeviceParams.PwmIndex = context->openParams.Hardware.PwmIndex;
	(void) DrvPwm_Close(&pwmDeviceParams);

	memset(context, 0, sizeof(*context));
	return HAL_OK;
}

HAL_StatusTypeDef DrvMotor_SetPulseUs(void* vpParam)
{
	const DrvMotor_PulseRequest_t* request = (const DrvMotor_PulseRequest_t*) vpParam;
	DrvMotor_Context_t* context = NULL;
	DrvPwm_CompareRequest_t compareRequest = {0};

	if (request == NULL)
	{
		return HAL_ERROR;
	}

	context = DrvMotor_GetContext(request->MotorIndex);
	if ((context == NULL) || (context->isOpen == 0U))
	{
		return HAL_ERROR;
	}

	compareRequest.PwmIndex = context->openParams.Hardware.PwmIndex;
	compareRequest.Compare = DrvMotor_Clamp(request->PulseUs,
	                                        context->openParams.Init.MinPulseUs,
	                                        context->openParams.Init.MaxPulseUs);

	return DrvPwm_SetCompare(&compareRequest);
}

HAL_StatusTypeDef DrvMotor_SetAngle(void* vpParam)
{
	const DrvMotor_AngleRequest_t* request = (const DrvMotor_AngleRequest_t*) vpParam;
	DrvMotor_Context_t* context = NULL;
	DrvMotor_PulseRequest_t pulseRequest = {0};

	if (request == NULL)
	{
		return HAL_ERROR;
	}

	context = DrvMotor_GetContext(request->MotorIndex);
	if ((context == NULL) || (context->isOpen == 0U))
	{
		return HAL_ERROR;
	}

	if (context->openParams.Init.Type != DRV_MOTOR_TYPE_POSITION_SERVO)
	{
		return HAL_ERROR;
	}

	pulseRequest.MotorIndex = request->MotorIndex;
	pulseRequest.PulseUs = DrvMotor_MapFloatRange(request->AngleDeg,
	                                              context->openParams.Init.MaxAngleDeg,
	                                              context->openParams.Init.MinPulseUs,
	                                              context->openParams.Init.MaxPulseUs);

	return DrvMotor_SetPulseUs(&pulseRequest);
}

HAL_StatusTypeDef DrvMotor_SetDuty(void* vpParam)
{
	const DrvMotor_DutyRequest_t* request = (const DrvMotor_DutyRequest_t*) vpParam;
	DrvMotor_Context_t* context = NULL;
	DrvMotor_PulseRequest_t pulseRequest = {0};

	if ((request == NULL) || (request->DutyMax == 0U))
	{
		return HAL_ERROR;
	}

	context = DrvMotor_GetContext(request->MotorIndex);
	if ((context == NULL) || (context->isOpen == 0U))
	{
		return HAL_ERROR;
	}

	pulseRequest.MotorIndex = request->MotorIndex;
	pulseRequest.PulseUs = DrvMotor_MapRange(request->Duty,
	                                         request->DutyMax,
	                                         context->openParams.Init.MinPulseUs,
	                                         context->openParams.Init.MaxPulseUs);

	return DrvMotor_SetPulseUs(&pulseRequest);
}

HAL_StatusTypeDef DrvMotor_SetSpeed(void* vpParam)
{
	const DrvMotor_SpeedRequest_t* request = (const DrvMotor_SpeedRequest_t*) vpParam;
	DrvMotor_Context_t* context = NULL;
	DrvMotor_PulseRequest_t pulseRequest = {0};

	if ((request == NULL) || (request->SpeedMax <= 0))
	{
		return HAL_ERROR;
	}

	context = DrvMotor_GetContext(request->MotorIndex);
	if ((context == NULL) || (context->isOpen == 0U))
	{
		return HAL_ERROR;
	}

	if (context->openParams.Init.Type != DRV_MOTOR_TYPE_CONTINUOUS_SERVO)
	{
		return HAL_ERROR;
	}

	pulseRequest.MotorIndex = request->MotorIndex;
	pulseRequest.PulseUs = DrvMotor_MapSignedRange(request->Speed,
	                                              request->SpeedMax,
	                                              context->openParams.Init.MinPulseUs,
	                                              context->openParams.Init.CenterPulseUs,
	                                              context->openParams.Init.MaxPulseUs);

	return DrvMotor_SetPulseUs(&pulseRequest);
}
