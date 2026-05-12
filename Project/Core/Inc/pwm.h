/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pwm.h
  * @brief   This file contains all the function prototypes for
  *          the pwm.c file
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __PWM_H__
#define __PWM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "board.h"
#include "drv_pwm/inc/drv_pwm.h"

void PWM_Init(void);
HAL_StatusTypeDef PWM_InitWithParams(const DrvPwm_OpenParams_t* openParams);
HAL_StatusTypeDef PWM_SetDuty(uint32_t pwmIndex, uint32_t duty, uint32_t dutyMax);
HAL_StatusTypeDef PWM_SetLedRedDuty(uint32_t duty);

#ifdef __cplusplus
}
#endif

#endif
