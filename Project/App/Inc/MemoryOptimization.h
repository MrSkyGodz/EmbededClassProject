/*
 * MemoryOptimization.h
 *
 * Bellek Optimizasyonu Modülü (ELE529E Hafta 9)
 *
 * Teknikler:
 *  1. Statik FreeRTOS Nesneleri  — heap parçalanmasını (fragmentation) engeller
 *  2. Heap Kullanım İzleme       — xPortGetFreeHeapSize() ile anlık takip
 *  3. Bellek Havuzu (Pool)       — sabit boyutlu bloklar, malloc() kullanmaz
 *  4. Yığın (Stack) Boyutu Doğrulama — watermark ile minimum stack ölçümü
 *
 * Neden Önemli?
 *  - Dinamik heap allocation (malloc/free) gömülü sistemlerde tehlikelidir:
 *    → Deterministik olmayan gecikme (real-time ihlali)
 *    → Zamanla heap fragmentation → tahsis başarısız → sistem çökmesi
 *  - Statik allocation + memory pool bu riskleri ortadan kaldırır.
 *
 * Created: 2026
 */

#ifndef APP_INC_MEMORY_OPTIMIZATION_H_
#define APP_INC_MEMORY_OPTIMIZATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * Heap Uyarı Eşiği
 * Serbest heap bu değerin altına düşerse uyarı loglanır.
 * configTOTAL_HEAP_SIZE = 15360 byte → %20 = 3072
 * ----------------------------------------------------------------------- */
#define MEM_HEAP_WARNING_THRESHOLD_BYTES   3072U

/* -----------------------------------------------------------------------
 * Basit Bellek Havuzu (Memory Pool)
 * Sensör verisi gibi sabit boyutlu nesneler için heap yerine kullanılır.
 * Blok sayısı ve boyutu derleme zamanında sabittir → tamamen deterministik.
 * ----------------------------------------------------------------------- */
#define MEM_POOL_BLOCK_COUNT   8U    /* Kaç blok var */
#define MEM_POOL_BLOCK_SIZE    32U   /* Her blok kaç byte */

/* Pool bloğu handle typedef */
typedef void* MemPool_Block_t;

/**
 * @brief Bellek havuzunu başlatır (tüm blokları serbest olarak işaretler).
 */
void MemPool_Init(void);

/**
 * @brief Havuzdan bir blok tahsis eder. O(1) deterministik.
 * @return Blok işaretçisi, veya NULL (tüm bloklar dolu ise).
 */
void* MemPool_Alloc(void);

/**
 * @brief Tahsis edilmiş bloğu havuza geri bırakır. O(1) deterministik.
 * @param block Geri bırakılacak blok.
 */
void MemPool_Free(void *block);

/**
 * @brief Havuzdaki kullanılabilir blok sayısını döndürür.
 */
uint8_t MemPool_Available(void);

/* -----------------------------------------------------------------------
 * Heap İzleme
 * ----------------------------------------------------------------------- */

/**
 * @brief Anlık serbest heap ve minimum (ever-seen) heap miktarını loglar.
 *        Eşik altına düşerse uyarı verir.
 */
void MEM_HeapReport(void);

/**
 * @brief Periyodik bellek raporlama görevi için çağrılacak fonksiyon.
 *        defaultTask'tan her 30 saniyede bir çağrılır.
 */
void MEM_PeriodicCheck(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INC_MEMORY_OPTIMIZATION_H_ */
