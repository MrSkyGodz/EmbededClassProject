#pragma once

#include <stdint.h>

#include "CommunicationType.h"
#include "FixedFrameQueue.hpp"

static constexpr uint8_t IcdFrameQueueCapacity = 8U;
static constexpr uint8_t IcdFrameMaxLength = 2U + 1U + ICD_HEADER_SIZE_BYTES + sizeof(IcdPayload_u) + 1U;

using IcdFrameQueue = FixedFrameQueue<IcdFrameQueueCapacity, IcdFrameMaxLength>;
