/*
 * iwdg.h
 *
 * Independent Watchdog (IWDG) Driver
 * STM32F767 — LSI ~32 kHz
 *
 * ROBUSTNESS (Hocanın revizyonu):
 * IWDG, yazılımın belirli aralıklarla "ben hâlâ çalışıyorum" demesini zorunlu
 * kılar. Sistem kilitlenirse, sonsuz döngüye girerse ya da kritik bir görev
 * dursa, watchdog süresi dolduğunda MCU otomatik olarak RESET atar.
 *
 * Zaman aşımı hesabı (LSI = 32000 Hz):
 *   Timeout = (4 * 2^Prescaler * Reload) / LSI_freq
 *   Prescaler = IWDG_PRESCALER_32  → bölen = 32
 *   Reload    = 2000
 *   Timeout   = (32 * 2000) / 32000 = 2.0 saniye
 *
 * defaultTask her 1ms'de bir döngü attığı için
 * IWDG_Refresh() her 500ms'de çağrılır → 2s timeout aşılmaz.
 *
 * Created: 2026
 */

#ifndef CORE_INC_IWDG_H_
#define CORE_INC_IWDG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"

/* -----------------------------------------------------------------------
 * IWDG Zaman Aşımı Ayarları
 * LSI = 32000 Hz (yaklaşık)
 * Prescaler bölen = 32  →  32000/32 = 1000 sayım/saniye
 * Reload = 2000          →  2000/1000 = 2.0 saniye timeout
 * ----------------------------------------------------------------------- */
#define IWDG_PRESCALER_VALUE   IWDG_PRESCALER_32
#define IWDG_RELOAD_VALUE      2000U

/* Watchdog'u kaç ms'de bir besleyeceğiz (timeout'un yarısı = güvenli) */
#define IWDG_REFRESH_PERIOD_MS 500U

/**
 * @brief IWDG'yi başlatır ve saymaya başlatır.
 *        main.cpp içinde FreeRTOS başlamadan ÖNCE çağrılmalıdır.
 * @retval HAL_OK → başarılı, HAL_ERROR → hata
 */
HAL_StatusTypeDef IWDG_Init(void);

/**
 * @brief IWDG sayacını sıfırlar (watchdog'u besler / "kick" eder).
 *        Bu fonksiyon çağrılmazsa 2 saniye içinde MCU reset atar.
 *        defaultTask içinde periyodik olarak çağrılır.
 */
void IWDG_Refresh(void);

/* Dışarıdan erişim için handle (gerekirse) */
extern IWDG_HandleTypeDef hiwdg;

#ifdef __cplusplus
}
#endif

#endif /* CORE_INC_IWDG_H_ */
