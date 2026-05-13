/*
 * Security.h
 *
 * Embedded Systems Security Module (ELE529E Week 7)
 *
 * Implements:
 *  - Input validation  (Integrity — prevents out-of-range actuation)
 *  - Command rate limiting (Availability — prevents DoS via UART flooding)
 *  - CRC-8/MAXIM integrity check (Integrity — stronger than XOR)
 *  - Stack watermark monitoring (Integrity — detects stack exhaustion)
 *
 * Created: 2026
 */

#ifndef APP_INC_SECURITY_H_
#define APP_INC_SECURITY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -----------------------------------------------------------------------
 * Motor Angle Limits
 * Restricting to ±90° prevents mechanical over-travel damage.
 * (Principle of least privilege applied to physical actuators)
 * ----------------------------------------------------------------------- */
#define SEC_MOTOR_ANGLE_MIN_DEG   (-90.0f)
#define SEC_MOTOR_ANGLE_MAX_DEG   ( 90.0f)

/* -----------------------------------------------------------------------
 * PWM Value Limits
 * ----------------------------------------------------------------------- */
#define SEC_PWM_MIN    (0U)
#define SEC_PWM_MAX    (255U)

/* -----------------------------------------------------------------------
 * Rate Limiter — Max Commands Per Second
 * Prevents a flooded UART from overwhelming the actuators (DoS defense).
 * At 100ms loop cadence, 20 cmd/s = 1 command every 2 loops.
 * ----------------------------------------------------------------------- */
#define SEC_MAX_COMMANDS_PER_SECOND   (20U)

/* -----------------------------------------------------------------------
 * Stack Watermark Report Period (ms)
 * ----------------------------------------------------------------------- */
#define SEC_STACK_CHECK_PERIOD_MS     (10000U)

/* -----------------------------------------------------------------------
 * Input Validation
 * ----------------------------------------------------------------------- */

/**
 * @brief Validate and clamp a motor angle to the safe range.
 * @param angleDeg  Requested angle in degrees (will be clamped in-place).
 * @return true if the original value was already in range, false if clamped.
 */
bool SEC_ValidateMotorAngle(float *angleDeg);

/**
 * @brief Validate a PWM duty-cycle byte value (0–255).
 * @param pwm  PWM value to check.
 * @return true if valid, false otherwise.
 */
bool SEC_ValidatePwmValue(uint8_t pwm);

/* -----------------------------------------------------------------------
 * Rate Limiter
 * ----------------------------------------------------------------------- */

/**
 * @brief Check whether a new command is allowed under the rate limit.
 *        Must be called once per command attempt.
 * @return true if command is allowed, false if it must be dropped.
 */
bool SEC_CommandRateLimiter(void);

/* -----------------------------------------------------------------------
 * Integrity — CRC-8 (polynomial 0x31, used in Dallas/MAXIM 1-Wire)
 * This is stronger than the original XOR-only check because a single
 * bit flip in any position always changes the CRC, while XOR-only
 * passes certain multi-bit error patterns silently.
 * ----------------------------------------------------------------------- */

/**
 * @brief Compute CRC-8/MAXIM over a buffer.
 * @param data   Pointer to data bytes.
 * @param length Number of bytes.
 * @return 8-bit CRC value.
 */
uint8_t SEC_Crc8(const uint8_t *data, size_t length);

/* -----------------------------------------------------------------------
 * Stack Monitoring
 * ----------------------------------------------------------------------- */

/**
 * @brief Log remaining stack watermark for all known tasks via printf.
 *        Call periodically from the default task (every SEC_STACK_CHECK_PERIOD_MS).
 */
void SEC_StackWatermarkCheck(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_SECURITY_H_ */
