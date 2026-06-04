/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
extern "C"
{
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
}
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Application.h"
#include "ImuReferenceController.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SENSOR_TIMER_PERIOD_MS 100U
#define SENSOR_DATA_QUEUE_LENGTH 4U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t sensorTaskHandle;
osTimerId_t sensorTimerHandle;
osSemaphoreId_t sensorSampleSemaphoreHandle;
osMessageQueueId_t sensorDataQueueHandle;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
const osTimerAttr_t sensorTimer_attributes = {
  .name = "sensorTimer",
};
const osSemaphoreAttr_t sensorSampleSemaphore_attributes = {
  .name = "sensorSampleSemaphore",
};
const osMessageQueueAttr_t sensorDataQueue_attributes = {
  .name = "sensorDataQueue",
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartSensorTask(void *argument);
void SensorTimerCallback(void *argument);
static bool QueueSensorSample(const BNO055_Sensors_t* sample);
static bool TryDequeueSensorSample(BNO055_Sensors_t* sample);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */


void FREERTOS_Init(void) {

  sensorSampleSemaphoreHandle = osSemaphoreNew(1U, 0U, &sensorSampleSemaphore_attributes);

  sensorDataQueueHandle = osMessageQueueNew(SENSOR_DATA_QUEUE_LENGTH,
                                            sizeof(BNO055_Sensors_t),
                                            &sensorDataQueue_attributes);

  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);
  sensorTimerHandle = osTimerNew(SensorTimerCallback, osTimerPeriodic, NULL, &sensorTimer_attributes);


}

//#include "IcdUartPublisher.hpp"
//#include "IcdMessageCodec.hpp"
#include "PayloadProcess.h"
void PublishSensorSample(IcdUartPublisher& Publisher, const BNO055_Sensors_t* sample)
{
	if (sample == nullptr)
	{
		return;
	}

	const IcdMessage_t message = BuildBno055TelemetryMessage(sample);
	Publisher.Publish(message);

	IcdMessage_t controlStatus = {};
	if (ImuReferenceController_Update(sample, &controlStatus))
	{
		Publisher.Publish(controlStatus);
	}
}



void StartDefaultTask(void *argument)
{

	Initialize();

	if (sensorTimerHandle != NULL)
	{
	(void) osTimerStart(sensorTimerHandle, SENSOR_TIMER_PERIOD_MS);
	}


	BNO055_Sensors_t sample;

	for(;;)
	{
		while (TryDequeueSensorSample(&sample))
		{
			PublishSensorSample(IcdPublisher,&sample);
		}
		Loop();
		osDelay(1U);
	}
  /* USER CODE END StartDefaultTask */
}

void StartSensorTask(void *argument)
{

  (void) argument;
  BNO055_Sensors_t sample = {0};

  for(;;)
  {
    if (osSemaphoreAcquire(sensorSampleSemaphoreHandle, osWaitForever) == osOK)
    {
      if (ReadOneSensorSample(&sample))
      {
        (void) QueueSensorSample(&sample);
      }
    }
  }
}

void SensorTimerCallback(void *argument)
{
  (void) argument;

  if (sensorSampleSemaphoreHandle != NULL)
  {
    (void) osSemaphoreRelease(sensorSampleSemaphoreHandle);
  }
}

static bool QueueSensorSample(const BNO055_Sensors_t* sample)
{
  BNO055_Sensors_t discardedSample = {0};

  if ((sensorDataQueueHandle == NULL) || (sample == NULL))
  {
    return false;
  }

  if (osMessageQueuePut(sensorDataQueueHandle, sample, 0U, 0U) == osOK)
  {
    return true;
  }

  if (osMessageQueueGet(sensorDataQueueHandle, &discardedSample, NULL, 0U) == osOK)
  {
    return osMessageQueuePut(sensorDataQueueHandle, sample, 0U, 0U) == osOK;
  }

  return false;
}

static bool TryDequeueSensorSample(BNO055_Sensors_t* sample)
{
  if ((sensorDataQueueHandle == NULL) || (sample == NULL))
  {
    return false;
  }

  /* Zero timeout keeps queue reads non-blocking in the default task. */
  return osMessageQueueGet(sensorDataQueueHandle, sample, NULL, 0U) == osOK;
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

