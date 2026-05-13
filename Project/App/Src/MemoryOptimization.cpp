/*
 * MemoryOptimization.cpp
 *
 * Bellek Optimizasyonu Implementasyonu (ELE529E Hafta 9)
 *
 * İçerik:
 *  1. Basit sabit-boyutlu Bellek Havuzu (Memory Pool)
 *  2. FreeRTOS Heap İzleme (xPortGetFreeHeapSize, xPortGetMinimumEverFreeHeapSize)
 *
 * Neden malloc()/free() KULLANMIYORUZ?
 *  - Gömülü sistemlerde malloc deterministik değildir (execution time değişken)
 *  - Uzun çalışmada heap fragmentation oluşur
 *  - Yeterli RAM görünse de parçalanmış heap tahsisi başarısız olabilir
 *  - Memory pool: sabit bloklar, O(1) tahsis, sıfır fragmentation
 *
 * FreeRTOS Statik Tahsis:
 *  - configSUPPORT_STATIC_ALLOCATION = 1 zaten aktif (FreeRTOSConfig.h)
 *  - freertos.c'deki statik task/queue/semaphore tanımları heap kullanmaz
 *
 * Created: 2026
 */

#include "MemoryOptimization.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
}

/* =========================================================================
 * BÖLÜM 1 — Sabit-Boyutlu Bellek Havuzu (Fixed-Size Memory Pool)
 * =========================================================================
 *
 * Yapı:
 *   - MEM_POOL_BLOCK_COUNT (8) adet blok
 *   - Her blok MEM_POOL_BLOCK_SIZE (32) byte
 *   - Bitmap ile hangi blokların serbest olduğu takip edilir
 *   - malloc/free YOK → tamamen deterministik, fragmentation YOK
 *
 * Kullanım örneği:
 *   void* buf = MemPool_Alloc();
 *   if (buf) { memcpy(buf, &data, sizeof(data)); ... MemPool_Free(buf); }
 * ========================================================================= */

/* Ham bellek alanı — statik, BSS segmentinde, heap'te DEĞİL */
static uint8_t  poolMemory_[MEM_POOL_BLOCK_COUNT][MEM_POOL_BLOCK_SIZE];

/* Bitmap: 1 = serbest, 0 = dolu */
static uint8_t  poolFree_[MEM_POOL_BLOCK_COUNT];

/* İstatistik */
static uint8_t  poolMinFree_ = MEM_POOL_BLOCK_COUNT;  /* En az görülen serbest blok */

void MemPool_Init(void)
{
    for (uint8_t i = 0U; i < MEM_POOL_BLOCK_COUNT; ++i)
    {
        poolFree_[i] = 1U;  /* Hepsi serbest */
        (void) memset(poolMemory_[i], 0, MEM_POOL_BLOCK_SIZE);
    }
    poolMinFree_ = MEM_POOL_BLOCK_COUNT;
    printf("[MEM] Memory pool hazır: %u blok x %u byte = %u byte\r\n",
           MEM_POOL_BLOCK_COUNT, MEM_POOL_BLOCK_SIZE,
           MEM_POOL_BLOCK_COUNT * MEM_POOL_BLOCK_SIZE);
}

void* MemPool_Alloc(void)
{
    /* İlk serbest bloğu bul — O(n) ama n=8, pratikte O(1) */
    for (uint8_t i = 0U; i < MEM_POOL_BLOCK_COUNT; ++i)
    {
        if (poolFree_[i] != 0U)
        {
            poolFree_[i] = 0U;  /* İşaretle: dolu */

            /* Min serbest istatistiğini güncelle */
            uint8_t freeCount = MemPool_Available();
            if (freeCount < poolMinFree_)
            {
                poolMinFree_ = freeCount;
            }

            return (void*) poolMemory_[i];
        }
    }

    /* Tüm bloklar dolu — tahsis başarısız */
    printf("[MEM] UYARI: Memory pool tükendi!\r\n");
    return nullptr;
}

void MemPool_Free(void *block)
{
    if (block == nullptr)
    {
        return;
    }

    /* Hangi blok olduğunu hesapla */
    const uint8_t (*base)[MEM_POOL_BLOCK_SIZE] = poolMemory_;
    const uint8_t (*ptr)[MEM_POOL_BLOCK_SIZE]  =
        (const uint8_t (*)[MEM_POOL_BLOCK_SIZE]) block;

    ptrdiff_t index = ptr - base;

    if ((index >= 0) && ((size_t) index < MEM_POOL_BLOCK_COUNT))
    {
        /* Blok içeriğini sil (güvenlik: veri sızıntısını engelle) */
        (void) memset(poolMemory_[index], 0, MEM_POOL_BLOCK_SIZE);
        poolFree_[index] = 1U;  /* Serbest */
    }
}

uint8_t MemPool_Available(void)
{
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < MEM_POOL_BLOCK_COUNT; ++i)
    {
        if (poolFree_[i] != 0U)
        {
            count++;
        }
    }
    return count;
}

/* =========================================================================
 * BÖLÜM 2 — FreeRTOS Heap İzleme
 * =========================================================================
 *
 * FreeRTOS Heap-4 (USE_FreeRTOS_HEAP_4 tanımlı, FreeRTOSConfig.h):
 *  - configTOTAL_HEAP_SIZE = 15360 byte
 *  - xPortGetFreeHeapSize()            → anlık serbest heap
 *  - xPortGetMinimumEverFreeHeapSize() → en az görülen serbest heap (watermark)
 *
 * Eğer minimum heap MEM_HEAP_WARNING_THRESHOLD_BYTES'ın altına düşerse
 * bu, sistemin ileride bellek hatası yaşayabileceğine işaret eder.
 * ========================================================================= */

void MEM_HeapReport(void)
{
    const size_t freeNow = xPortGetFreeHeapSize();
    const size_t freeMin = xPortGetMinimumEverFreeHeapSize();
    const size_t total   = configTOTAL_HEAP_SIZE;
    const size_t used    = total - freeNow;

    printf("[MEM] Heap: Toplam=%u  Kullanılan=%u  Serbest=%u  Min.Watermark=%u byte\r\n",
           (unsigned) total,
           (unsigned) used,
           (unsigned) freeNow,
           (unsigned) freeMin);

    printf("[MEM] Pool: %u/%u blok serbest  (Min.görülen: %u)\r\n",
           MemPool_Available(),
           MEM_POOL_BLOCK_COUNT,
           (unsigned) poolMinFree_);

    /* Eşik uyarısı */
    if (freeMin < MEM_HEAP_WARNING_THRESHOLD_BYTES)
    {
        printf("[MEM] *** UYARI: Minimum heap %u byte — kritik eşik altında! ***\r\n",
               (unsigned) freeMin);
    }
}

void MEM_PeriodicCheck(void)
{
    static uint32_t lastCheckMs = 0U;
    const  uint32_t nowMs       = xTaskGetTickCount();

    if ((nowMs - lastCheckMs) >= 30000U)   /* 30 saniyede bir */
    {
        lastCheckMs = nowMs;
        MEM_HeapReport();
    }
}
