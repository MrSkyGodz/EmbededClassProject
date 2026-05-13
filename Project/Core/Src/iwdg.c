/*
 * iwdg.c
 *
 * Independent Watchdog (IWDG) Implementasyonu
 * STM32F767 — HAL tabanlı
 *
 * Robustness (Dayanıklılık) Katmanı:
 *  - IWDG, yazılım donması / kilitlenmesi durumunda otomatik reset sağlar
 *  - Görev açlığı (task starvation), sonsuz döngü, donanım hatası
 *    gibi durumlarda sistemin kurtarılmasına imkân tanır
 *  - Quality Attribute: Availability + Reliability
 *
 * Created: 2026
 */

#include "iwdg.h"
#include <stdio.h>

/* HAL handle — tek global örnek */
IWDG_HandleTypeDef hiwdg;

/* =========================================================================
 * IWDG_Init
 * =========================================================================
 * STM32F767'nin IWDG çevrimi LSI (Low Speed Internal) osilatörüne bağlıdır.
 * LSI frekansı ~32 kHz'dir (32000 Hz).
 *
 * Hesaplama:
 *   Prescaler = IWDG_PRESCALER_32  →  bölen = 32
 *   Sayım frekansı = 32000 / 32 = 1000 Hz (1 ms başına 1 sayım)
 *   Reload = 2000  →  2000 ms = 2 saniye timeout
 *
 * defaultTask'ın periyodik olarak IWDG_Refresh() çağırması beklenir.
 * 2 saniyeden uzun süre çağrılmazsa MCU otomatik olarak reset atar.
 * ========================================================================= */
HAL_StatusTypeDef IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_VALUE;   /* /32 */
    hiwdg.Init.Reload    = IWDG_RELOAD_VALUE;       /* 2000 → 2 sn */
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;     /* Pencere devre dışı */

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        /* IWDG başlatılamazsa sistem güvenli şekilde durdurulur */
        return HAL_ERROR;
    }

    printf("[IWDG] Watchdog aktif — %u ms timeout\r\n",
           (unsigned)(IWDG_RELOAD_VALUE));  /* ~2000ms */

    return HAL_OK;
}

/* =========================================================================
 * IWDG_Refresh
 * =========================================================================
 * IWDG sayacını reload değerine sıfırlar.
 * Bu fonksiyon her IWDG_REFRESH_PERIOD_MS (500 ms) çağrılmalıdır.
 * Çağrılmazsa 2 saniye sonra MCU reset atar.
 * ========================================================================= */
void IWDG_Refresh(void)
{
    (void) HAL_IWDG_Refresh(&hiwdg);
}
