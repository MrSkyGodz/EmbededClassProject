#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CircularBuffer.hpp"
#include "IComLib.h"
#include "stm32f7xx_hal.h"

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

class UartComImp : public IComLib
{
public:
    enum class UartMode
    {
        Blocking,
        Interrupt,
        Dma
    };

    static constexpr size_t DefaultRxCapacity = 128U;
    static constexpr size_t DefaultTxCapacity = 128U;
    static constexpr size_t MaxRxChunkLength = CircularBuffer::MaxCapacity;
    static constexpr size_t MaxTxChunkLength = CircularBuffer::MaxCapacity;
    static constexpr size_t DefaultInterruptChunkLength = 1U;
    static constexpr size_t DefaultDmaChunkLength = 64U;
    static constexpr size_t MaxRegisteredInstances = 4U;
    using RxInterruptCallback = void (*)(const uint8_t* buffer, size_t length);

	UartComImp();
	explicit UartComImp(UART_HandleTypeDef& handle);

	void Reinit(UART_HandleTypeDef& handle,
                UartMode rxMode,
                UartMode txMode = UartMode::Blocking);
    void RegisterRxInterruptCallback(RxInterruptCallback callback,
                                     size_t rxChunkLength = 0U);

	bool Write(const uint8_t* buffer, size_t length) override;
	size_t Read(uint8_t* buffer, size_t bufferLength) override;

private:
    friend void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
    friend void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
    friend void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

    static void RegisterInstance(UartComImp* instance);
    static UartComImp* FindByHandle(UART_HandleTypeDef* huart);
    static void DispatchRxCompleteIRQ(UART_HandleTypeDef* huart);
    static void DispatchRxEventIRQ(UART_HandleTypeDef* huart, uint16_t size);
    static void DispatchTxCompleteIRQ(UART_HandleTypeDef* huart);

    void ArmAsyncReceive();
    void OnAsyncReceive(uint16_t size);
    bool StartAsyncTransmit();
    void OnAsyncTransmitComplete();
    size_t ResolveDefaultRxChunkLength() const;
    size_t ResolveDefaultTxChunkLength() const;

	bool initialized_;
	UART_HandleTypeDef* handle_;
    UartMode rxMode_;
    UartMode txMode_;
    uint8_t rxChunkBuffer_[MaxRxChunkLength];
    size_t rxChunkLength_;
    CircularBuffer rxBuffer_;
    RxInterruptCallback rxInterruptCallback_;
    uint8_t txChunkBuffer_[MaxTxChunkLength];
    size_t txChunkLength_;
    size_t txInFlightLength_;
    bool txBusy_;
    CircularBuffer txBuffer_;
};
