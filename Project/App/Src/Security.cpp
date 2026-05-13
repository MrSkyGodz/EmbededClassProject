/*
 * Security.cpp
 *
 * Embedded Systems Security Module (ELE529E Week 7)
 *
 * Implements runtime security mechanisms for the STM32F767 IMU/Motor project:
 *
 *  1. Input Validation  — ensures only safe command values reach actuators
 *  2. Rate Limiting     — prevents UART command flooding (DoS defense)
 *  3. CRC-8/MAXIM       — stronger integrity check than XOR
 *  4. Stack Monitoring  — detects approaching stack exhaustion early
 *
 * WEEK 7 SECURITY CONCEPTS APPLIED:
 *  - "Least Privilege": actuators only accept values within proven-safe ranges.
 *  - "Availability": rate limiter prevents a flooded interface from
 *    starving the control loop or causing mechanical damage.
 *  - "Integrity": CRC-8 polynomial catches more error patterns than XOR.
 *  - "Defense in Depth": multiple layers (framing, CRC, range check, rate limit).
 *
 * Created: 2026
 */

#include "Security.h"

extern "C" {
#include "cmsis_os.h"          /* osKernelGetTickCount()              */
#include "FreeRTOS.h"
#include "task.h"              /* uxTaskGetStackHighWaterMark()        */
#include <stdio.h>             /* printf (redirected to UART3)         */
}

/* =========================================================================
 * SECTION 1 — Input Validation
 * =========================================================================
 * Week 7: "Never trust data that arrives from outside the system boundary."
 * The UART is an external interface — any received value must be validated
 * before it is used to drive hardware.
 * ========================================================================= */

bool SEC_ValidateMotorAngle(float *angleDeg)
{
    if (angleDeg == nullptr)
    {
        return false;
    }

    bool inRange = true;

    if (*angleDeg < SEC_MOTOR_ANGLE_MIN_DEG)
    {
        printf("[SEC] Motor angle %.1f clamped to %.1f deg\r\n",
               (double)*angleDeg, (double)SEC_MOTOR_ANGLE_MIN_DEG);
        *angleDeg = SEC_MOTOR_ANGLE_MIN_DEG;
        inRange = false;
    }
    else if (*angleDeg > SEC_MOTOR_ANGLE_MAX_DEG)
    {
        printf("[SEC] Motor angle %.1f clamped to %.1f deg\r\n",
               (double)*angleDeg, (double)SEC_MOTOR_ANGLE_MAX_DEG);
        *angleDeg = SEC_MOTOR_ANGLE_MAX_DEG;
        inRange = false;
    }

    return inRange;
}

bool SEC_ValidatePwmValue(uint8_t pwm)
{
    /* uint8_t is always 0–255 by type, but we keep this check explicit
     * as documentation and for future narrowing of the allowed range. */
    (void) pwm;
    return true;   /* All 0–255 values are hardware-valid for this LED driver */
}

/* =========================================================================
 * SECTION 2 — Command Rate Limiter
 * =========================================================================
 * Week 7: "Availability attacks (DoS) can be just as damaging as data
 * corruption in embedded/safety systems."
 *
 * A PC sending malformed or high-frequency commands over UART could:
 *  - Cause servo jitter / mechanical damage
 *  - Starve the sensor task of CPU time
 *  - Overflow the command queue
 *
 * This limiter uses a token-bucket approach with a 1-second window.
 * ========================================================================= */

bool SEC_CommandRateLimiter(void)
{
    static uint32_t windowStartTick = 0U;
    static uint32_t commandsThisWindow = 0U;

    const uint32_t now = osKernelGetTickCount();  /* 1 tick = 1 ms */

    /* Start a new 1-second window when the old one expires */
    if ((now - windowStartTick) >= 1000U)
    {
        windowStartTick   = now;
        commandsThisWindow = 0U;
    }

    if (commandsThisWindow >= SEC_MAX_COMMANDS_PER_SECOND)
    {
        /* Rate limit exceeded — drop this command */
        printf("[SEC] RATE LIMIT: command dropped (>%u cmd/s)\r\n",
               SEC_MAX_COMMANDS_PER_SECOND);
        return false;
    }

    commandsThisWindow++;
    return true;
}

/* =========================================================================
 * SECTION 3 — CRC-8 / MAXIM (Polynomial 0x31, init 0x00)
 * =========================================================================
 * Week 7: "Integrity — data must not be altered in transit without detection."
 *
 * Why CRC-8 is better than the original XOR:
 *  - XOR (parity) misses all even-count multi-bit errors in the same position
 *  - CRC-8 polynomial 0x31 detects all 1-bit, 2-bit, and most burst errors
 *    within an 8-byte payload — sufficient for this protocol's frame sizes.
 *
 * The lookup table approach is fast (no division) and uses only 256 bytes
 * of Flash — acceptable for an STM32F767 with 1 MB Flash.
 * ========================================================================= */

static const uint8_t kCrc8Table[256U] = {
    0x00U, 0x5EU, 0xBCU, 0xE2U, 0x61U, 0x3FU, 0xDDU, 0x83U,
    0xC2U, 0x9CU, 0x7EU, 0x20U, 0xA3U, 0xFDU, 0x1FU, 0x41U,
    0x9DU, 0xC3U, 0x21U, 0x7FU, 0xFCU, 0xA2U, 0x40U, 0x1EU,
    0x5FU, 0x01U, 0xE3U, 0xBDU, 0x3EU, 0x60U, 0x82U, 0xDCU,
    0x23U, 0x7DU, 0x9FU, 0xC1U, 0x42U, 0x1CU, 0xFEU, 0xA0U,
    0xE1U, 0xBFU, 0x5DU, 0x03U, 0x80U, 0xDEU, 0x3CU, 0x62U,
    0xBEU, 0xE0U, 0x02U, 0x5CU, 0xDFU, 0x81U, 0x63U, 0x3DU,
    0x7CU, 0x22U, 0xC0U, 0x9EU, 0x1DU, 0x43U, 0xA1U, 0xFFU,
    0x46U, 0x18U, 0xFAU, 0xA4U, 0x27U, 0x79U, 0x9BU, 0xC5U,
    0x84U, 0xDAU, 0x38U, 0x66U, 0xE5U, 0xBBU, 0x59U, 0x07U,
    0xDBU, 0x85U, 0x67U, 0x39U, 0xBAU, 0xE4U, 0x06U, 0x58U,
    0x19U, 0x47U, 0xA5U, 0xFBU, 0x78U, 0x26U, 0xC4U, 0x9AU,
    0x65U, 0x3BU, 0xD9U, 0x87U, 0x04U, 0x5AU, 0xB8U, 0xE6U,
    0xA7U, 0xF9U, 0x1BU, 0x45U, 0xC6U, 0x98U, 0x7AU, 0x24U,
    0xF8U, 0xA6U, 0x44U, 0x1AU, 0x99U, 0xC7U, 0x25U, 0x7BU,
    0x3AU, 0x64U, 0x86U, 0xD8U, 0x5BU, 0x05U, 0xE7U, 0xB9U,
    0x8CU, 0xD2U, 0x30U, 0x6EU, 0xEDU, 0xB3U, 0x51U, 0x0FU,
    0x4EU, 0x10U, 0xF2U, 0xACU, 0x2FU, 0x71U, 0x93U, 0xCDU,
    0x11U, 0x4FU, 0xADU, 0xF3U, 0x70U, 0x2EU, 0xCCU, 0x92U,
    0xD3U, 0x8DU, 0x6FU, 0x31U, 0xB2U, 0xECU, 0x0EU, 0x50U,
    0xAFU, 0xF1U, 0x13U, 0x4DU, 0xCEU, 0x90U, 0x72U, 0x2CU,
    0x6DU, 0x33U, 0xD1U, 0x8FU, 0x0CU, 0x52U, 0xB0U, 0xEEU,
    0x32U, 0x6CU, 0x8EU, 0xD0U, 0x53U, 0x0DU, 0xEFU, 0xB1U,
    0xF0U, 0xAEU, 0x4CU, 0x12U, 0x91U, 0xCFU, 0x2DU, 0x73U,
    0xCAU, 0x94U, 0x76U, 0x28U, 0xABU, 0xF5U, 0x17U, 0x49U,
    0x08U, 0x56U, 0xB4U, 0xEAU, 0x69U, 0x37U, 0xD5U, 0x8BU,
    0x57U, 0x09U, 0xEBU, 0xB5U, 0x36U, 0x68U, 0x8AU, 0xD4U,
    0x95U, 0xCBU, 0x29U, 0x77U, 0xF4U, 0xAAU, 0x48U, 0x16U,
    0xE9U, 0xB7U, 0x55U, 0x0BU, 0x88U, 0xD6U, 0x34U, 0x6AU,
    0x2BU, 0x75U, 0x97U, 0xC9U, 0x4AU, 0x14U, 0xF6U, 0xA8U,
    0x74U, 0x2AU, 0xC8U, 0x96U, 0x15U, 0x4BU, 0xA9U, 0xF7U,
    0xB6U, 0xE8U, 0x0AU, 0x54U, 0xD7U, 0x89U, 0x6BU, 0x35U,
};

uint8_t SEC_Crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0x00U;

    if (data == nullptr)
    {
        return crc;
    }

    for (size_t i = 0U; i < length; ++i)
    {
        crc = kCrc8Table[crc ^ data[i]];
    }

    return crc;
}

/* =========================================================================
 * SECTION 4 — Stack Watermark Monitor
 * =========================================================================
 * Week 7: "Embedded systems must be self-monitoring — resource exhaustion
 * leads to undefined behavior, crashes, or exploitable conditions."
 *
 * FreeRTOS fills unused stack with 0xA5 at task creation.
 * uxTaskGetStackHighWaterMark() returns the minimum ever seen,
 * in 4-byte words. A value near 0 means imminent stack overflow.
 * ========================================================================= */

/* External task handles declared in freertos.c */
extern osThreadId_t defaultTaskHandle;
extern osThreadId_t sensorTaskHandle;

void SEC_StackWatermarkCheck(void)
{
    if (defaultTaskHandle != NULL)
    {
        UBaseType_t defaultTaskWm =
            uxTaskGetStackHighWaterMark((TaskHandle_t) defaultTaskHandle);
        printf("[SEC] Stack watermark — defaultTask: %lu words remaining\r\n",
               (unsigned long) defaultTaskWm);
    }

    if (sensorTaskHandle != NULL)
    {
        UBaseType_t sensorTaskWm =
            uxTaskGetStackHighWaterMark((TaskHandle_t) sensorTaskHandle);
        printf("[SEC] Stack watermark — sensorTask:  %lu words remaining\r\n",
               (unsigned long) sensorTaskWm);
    }
}
