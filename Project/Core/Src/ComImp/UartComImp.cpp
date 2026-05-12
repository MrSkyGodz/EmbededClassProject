#include "ComImp/UartComImp.h"

namespace
{
UartComImp* g_uartComImpInstances[UartComImp::MaxRegisteredInstances] = {nullptr};
}

UartComImp::UartComImp()
	: initialized_(false),
	  handle_(nullptr),
	  rxMode_(UartMode::Blocking),
	  txMode_(UartMode::Blocking),
	  rxChunkBuffer_{0U},
	  rxChunkLength_(DefaultInterruptChunkLength),
	  rxBuffer_(DefaultRxCapacity),
	  rxInterruptCallback_(nullptr),
	  txChunkBuffer_{0U},
	  txChunkLength_(DefaultInterruptChunkLength),
	  txInFlightLength_(0U),
	  txBusy_(false),
	  txBuffer_(DefaultTxCapacity)
{
}

UartComImp::UartComImp(UART_HandleTypeDef& handle)
	: UartComImp()
{
	Reinit(handle, UartMode::Blocking, UartMode::Blocking);
}

void UartComImp::Reinit(UART_HandleTypeDef& handle,
                        UartMode rxMode,
                        UartMode txMode)
{
	handle_ = &handle;
	initialized_ = (handle_ != nullptr) && (handle_->Instance != nullptr);
	rxMode_ = rxMode;
	txMode_ = txMode;

	for (size_t i = 0U; i < MaxRxChunkLength; ++i)
	{
		rxChunkBuffer_[i] = 0U;
	}
	for (size_t i = 0U; i < MaxTxChunkLength; ++i)
	{
		txChunkBuffer_[i] = 0U;
	}

	rxChunkLength_ = ResolveDefaultRxChunkLength();
	txChunkLength_ = ResolveDefaultTxChunkLength();
	rxBuffer_.clear();
	txBuffer_.clear();
	txInFlightLength_ = 0U;
	txBusy_ = false;
	RegisterInstance(this);
	if (initialized_ && (rxMode_ != UartMode::Blocking))
	{
		ArmAsyncReceive();
	}
}

void UartComImp::RegisterRxInterruptCallback(RxInterruptCallback callback,
	                                         size_t rxChunkLength)
{
	rxInterruptCallback_ = callback;

	if ((rxChunkLength == 0U) || (rxChunkLength > MaxRxChunkLength))
	{
		rxChunkLength_ = ResolveDefaultRxChunkLength();
	}
	else
	{
		rxChunkLength_ = rxChunkLength;
	}

	if (initialized_ && (handle_ != nullptr))
	{
		if (rxMode_ != UartMode::Blocking)
		{
			(void) HAL_UART_AbortReceive(handle_);
			ArmAsyncReceive();
		}
	}
}

bool UartComImp::Write(const uint8_t* buffer, size_t length)
{
	if (!initialized_ || (handle_ == nullptr))
	{
		return false;
	}

	if ((buffer == nullptr) && (length != 0U))
	{
		return false;
	}

	if (length > 0xFFFFU)
	{
		return false;
	}

	if (length == 0U)
	{
		return true;
	}

	if (txMode_ != UartMode::Blocking)
	{
		size_t capacityLeft = 0U;
		bool shouldStart = false;
		const uint32_t primask = __get_PRIMASK();
		__disable_irq();
		capacityLeft = txBuffer_.capacity() - txBuffer_.available();
		if (length <= capacityLeft)
		{
			for (size_t i = 0U; i < length; ++i)
			{
				(void) txBuffer_.push(buffer[i]);
			}

			shouldStart = !txBusy_;
		}
		__set_PRIMASK(primask);

		if (length > capacityLeft)
		{
			return false;
		}

		if (shouldStart)
		{
			return StartAsyncTransmit();
		}

		return true;
	}

	return HAL_UART_Transmit(handle_,
	                         const_cast<uint8_t*>(buffer),
	                         (uint16_t) length,
	                         HAL_MAX_DELAY) == HAL_OK;
}

size_t UartComImp::Read(uint8_t* buffer, size_t bufferLength)
{
	if (!initialized_ || (handle_ == nullptr))
	{
		return 0U;
	}

	if ((buffer == nullptr) && (bufferLength != 0U))
	{
		return 0U;
	}

	if (rxMode_ != UartMode::Blocking)
	{
		size_t readCount = 0U;
		const uint32_t primask = __get_PRIMASK();
		__disable_irq();
		while ((readCount < bufferLength) && rxBuffer_.pop(buffer[readCount]))
		{
			readCount++;
		}
		__set_PRIMASK(primask);

		return readCount;
	}

	if (bufferLength > 0xFFFFU)
	{
		return 0U;
	}

	if (bufferLength == 0U)
	{
		return 0U;
	}

	if (HAL_UART_Receive(handle_,
	                     buffer,
	                     (uint16_t) bufferLength,
	                     HAL_MAX_DELAY) != HAL_OK)
	{
		return 0U;
	}

	return bufferLength;
}

size_t UartComImp::ResolveDefaultRxChunkLength() const
{
	return (rxMode_ == UartMode::Dma) ? DefaultDmaChunkLength : DefaultInterruptChunkLength;
}

size_t UartComImp::ResolveDefaultTxChunkLength() const
{
	return (txMode_ == UartMode::Dma) ? DefaultDmaChunkLength : DefaultInterruptChunkLength;
}

void UartComImp::RegisterInstance(UartComImp* instance)
{
	for (size_t i = 0U; i < MaxRegisteredInstances; ++i)
	{
		if (g_uartComImpInstances[i] == instance)
		{
			return;
		}
	}

	for (size_t i = 0U; i < MaxRegisteredInstances; ++i)
	{
		if (g_uartComImpInstances[i] == nullptr)
		{
			g_uartComImpInstances[i] = instance;
			return;
		}
	}
}

UartComImp* UartComImp::FindByHandle(UART_HandleTypeDef* huart)
{
	for (size_t i = 0U; i < MaxRegisteredInstances; ++i)
	{
		if ((g_uartComImpInstances[i] != nullptr) && (g_uartComImpInstances[i]->handle_ == huart))
		{
			return g_uartComImpInstances[i];
		}
	}

	return nullptr;
}

void UartComImp::DispatchRxCompleteIRQ(UART_HandleTypeDef* huart)
{
	UartComImp* instance = FindByHandle(huart);
	if ((instance != nullptr) && (instance->rxMode_ == UartMode::Interrupt))
	{
		instance->OnAsyncReceive((uint16_t) instance->rxChunkLength_);
	}
}

void UartComImp::DispatchRxEventIRQ(UART_HandleTypeDef* huart, uint16_t size)
{
	UartComImp* instance = FindByHandle(huart);
	if ((instance != nullptr) && (instance->rxMode_ == UartMode::Dma))
	{
		instance->OnAsyncReceive(size);
	}
}

void UartComImp::DispatchTxCompleteIRQ(UART_HandleTypeDef* huart)
{
	UartComImp* instance = FindByHandle(huart);
	if ((instance != nullptr) && (instance->txMode_ != UartMode::Blocking))
	{
		instance->OnAsyncTransmitComplete();
	}
}

void UartComImp::ArmAsyncReceive()
{
	if (!initialized_ || (handle_ == nullptr) || (rxMode_ == UartMode::Blocking))
	{
		return;
	}

	if (rxMode_ == UartMode::Interrupt)
	{
		(void) HAL_UART_Receive_IT(handle_,
		                           rxChunkBuffer_,
		                           (uint16_t) rxChunkLength_);
		return;
	}

	if (handle_->hdmarx == nullptr)
	{
		return;
	}

	if (HAL_UARTEx_ReceiveToIdle_DMA(handle_,
	                                 rxChunkBuffer_,
	                                 (uint16_t) rxChunkLength_) == HAL_OK)
	{
		if (handle_->hdmarx != nullptr)
		{
			__HAL_DMA_DISABLE_IT(handle_->hdmarx, DMA_IT_HT);
		}
	}
}

void UartComImp::OnAsyncReceive(uint16_t size)
{
	for (uint16_t i = 0U; i < size; ++i)
	{
		(void) rxBuffer_.pushOverwriteOldest(rxChunkBuffer_[i]);
	}

	if (rxInterruptCallback_ != nullptr)
	{
		rxInterruptCallback_(rxChunkBuffer_, size);
	}

	ArmAsyncReceive();
}

bool UartComImp::StartAsyncTransmit()
{
	if (!initialized_ || (handle_ == nullptr) || (txMode_ == UartMode::Blocking))
	{
		return false;
	}

	size_t chunkLength = 0U;
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();
	if (txBusy_)
	{
		__set_PRIMASK(primask);
		return true;
	}

	chunkLength = txBuffer_.available();
	if (chunkLength > txChunkLength_)
	{
		chunkLength = txChunkLength_;
	}

	if (chunkLength == 0U)
	{
		__set_PRIMASK(primask);
		return true;
	}

	for (size_t i = 0U; i < chunkLength; ++i)
	{
		(void) txBuffer_.peek(i, txChunkBuffer_[i]);
	}

	txBusy_ = true;
	txInFlightLength_ = chunkLength;
	__set_PRIMASK(primask);

	HAL_StatusTypeDef status = HAL_ERROR;
	if (txMode_ == UartMode::Interrupt)
	{
		status = HAL_UART_Transmit_IT(handle_, txChunkBuffer_, (uint16_t) chunkLength);
	}
	else if ((txMode_ == UartMode::Dma) && (handle_->hdmatx != nullptr))
	{
		status = HAL_UART_Transmit_DMA(handle_, txChunkBuffer_, (uint16_t) chunkLength);
	}

	if (status == HAL_OK)
	{
		return true;
	}

	const uint32_t restorePrimask = __get_PRIMASK();
	__disable_irq();
	txBusy_ = false;
	txInFlightLength_ = 0U;
	__set_PRIMASK(restorePrimask);
	return false;
}

void UartComImp::OnAsyncTransmitComplete()
{
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();
	(void) txBuffer_.discard(txInFlightLength_);
	txInFlightLength_ = 0U;
	txBusy_ = false;
	__set_PRIMASK(primask);

	(void) StartAsyncTransmit();
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	UartComImp::DispatchRxCompleteIRQ(huart);
}

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	UartComImp::DispatchRxEventIRQ(huart, Size);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	UartComImp::DispatchTxCompleteIRQ(huart);
}
