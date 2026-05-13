/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   USART glue layer backed by drv_uart / drv_dma.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "usart.h"

#include "drv_uart/inc/drv_uart.h"

#include "board.h"


static DrvUart_OpenParams_t g_usart3OpenParams = {
    .UartIndex = USART3_UART_INDEX,
    .UART_Init = {
        .Instance = USART3,
        .BaudRate = 115200U,
        .WordLength = UART_WORDLENGTH_8B,
        .StopBits = UART_STOPBITS_1,
        .Parity = UART_PARITY_NONE,
        .Mode = UART_MODE_TX_RX,
        .HwFlowCtl = UART_HWCONTROL_NONE,
        .OverSampling = UART_OVERSAMPLING_16,
        .OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE,
        .AdvancedFeatureInit = UART_ADVFEATURE_NO_INIT
    },
    .GPIO_TXInit = {
        .GPIOIndex = USART3_TX_GPIO_INDEX,
        .Port = GPIOD,
        .Pin = GPIO_PIN_8,
        .Function = GPIO_AF7_USART3
    },
    .GPIO_RXInit = {
        .GPIOIndex = USART3_RX_GPIO_INDEX,
        .Port = GPIOD,
        .Pin = GPIO_PIN_9,
        .Function = GPIO_AF7_USART3
    },
    .Interrupt = {
        .Enabled = 1U,
        .IRQn = USART3_IRQn,
        .Priority = 5U,
        .SubPriority = 0U
    },
    .RxDmaConfig = {
        .DmaIndex = USART3_RX_DMA_INDEX,
        .Instance = DMA1_Stream1,
        .Channel = DMA_CHANNEL_4,
        .IRQn = DMA1_Stream1_IRQn
    },
    .TxDmaConfig = {
        .DmaIndex = USART3_TX_DMA_INDEX,
        .Instance = DMA1_Stream3,
        .Channel = DMA_CHANNEL_4,
        .IRQn = DMA1_Stream3_IRQn
    },
    .RxMode = DRV_UART_MODE_DMA,
    .TxMode = DRV_UART_MODE_DMA,
    .UARTHandle = NULL
};

void UART_Init(void)
{
    DrvUart_DeviceParams_t deviceParams = {.UartIndex = USART3_UART_INDEX};

    (void) DrvUart_Close(&deviceParams);

    if (DrvUart_Open(&g_usart3OpenParams) != HAL_OK)
    {
        Error_Handler();
    }
}
