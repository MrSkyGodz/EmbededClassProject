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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Application.h"
#include "MemoryOptimization.h"  /* Hafta 9: Bellek optimizasyonu */
#include "iwdg.h"                /* Robustness: Watchdog besleme */

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

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  sensorSampleSemaphoreHandle = osSemaphoreNew(1U, 0U, &sensorSampleSemaphore_attributes);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  sensorDataQueueHandle = osMessageQueueNew(SENSOR_DATA_QUEUE_LENGTH,
                                            sizeof(BNO055_Sensors_t),
                                            &sensorDataQueue_attributes);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);
  sensorTimerHandle = osTimerNew(SensorTimerCallback, osTimerPeriodic, NULL, &sensorTimer_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  BNO055_Sensors_t sample = {0};
  /* USER CODE BEGIN StartDefaultTask */
  (void) argument;

  /* Bellek havuzunu başlat (Hafta 9 — Memory Optimization) */
  MemPool_Init();

  Initialize();

  if (sensorTimerHandle != NULL)
  {
    (void) osTimerStart(sensorTimerHandle, SENSOR_TIMER_PERIOD_MS);
  }

  for(;;)
  {
	while (TryDequeueSensorSample(&sample))
    {
      PublishSensorSample(&sample);
    }

    Loop();

    /* ---- Robustness: IWDG Watchdog besleme (her 500 ms'de bir) ---- */
    static uint32_t lastWatchdogMs = 0U;
    const  uint32_t nowMs          = osKernelGetTickCount();
    if ((nowMs - lastWatchdogMs) >= IWDG_REFRESH_PERIOD_MS)
    {
      lastWatchdogMs = nowMs;
      IWDG_Refresh();  /* 2 saniye dolmadan beslenmezse MCU reset atar */
    }

    /* ---- Hafta 9: Periyodik bellek raporu (her 30 sn'de bir) ---- */
    MEM_PeriodicCheck();

    osDelay(1U);
  }
  /* USER CODE END StartDefaultTask */
}

void StartSensorTask(void *argument)
{
  BNO055_Sensors_t sample = {0};
  (void) argument;

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

/* -------------------------------------------------------------------------
 * Week 7 Security — Stack Overflow Hook
 * -------------------------------------------------------------------------
 * FreeRTOS calls this function when it detects a task has overflowed its
 * stack (enabled by configCHECK_FOR_STACK_OVERFLOW in FreeRTOSConfig.h).
 *
 * Without this hook, a stack overflow silently corrupts adjacent memory,
 * which can lead to:
 *  - Control-flow hijacking (security exploit)
 *  - Data corruption (safety violation)
 *  - Silent incorrect behavior (worst case)
 *
 * Here we: print the offending task name, then halt safely.
 * In production, you could also trigger a watchdog reset.
 * ------------------------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void) xTask;
    /* Note: printf may not work if the stack is too corrupted.
     * The halt below is the safety-critical response. */
    printf("[SEC] STACK OVERFLOW detected in task: %s — halting!\r\n",
           (pcTaskName != NULL) ? pcTaskName : "unknown");
    __disable_irq();
    Error_Handler();
}

/* USER CODE END Application */

