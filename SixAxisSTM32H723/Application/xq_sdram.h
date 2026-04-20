#ifndef __XQ_SDRAM_H
#define __XQ_SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* SDRAM 测试结果状态 */
typedef enum {
    SDRAM_TEST_OK = 0,
    SDRAM_TEST_FAILED = 1
} SDRAM_TestStatus;

/* SDRAM 基地址和大小配置 (根据FMC配置自动确定) */
/* FMC Bank1: 行13位, 列9位, 4个内部Bank, 16位数据宽度 */
/* 容量计算: 2^13 × 2^9 × 4 × 2字节 = 32MB */
#define SDRAM_BANK_ADDR         ((uint32_t)0xC0000000)  // FMC_SDRAM_BANK1
#define SDRAM_TIMEOUT           ((uint32_t)0xFFFF)
#define SDRAM_SIZE              ((uint32_t)0x2000000)   // 32MB = 32 * 1024 * 1024

/* 测试相关宏定义 */
#define SDRAM_TEST_START_ADDR   SDRAM_BANK_ADDR
#define SDRAM_TEST_SIZE         SDRAM_SIZE

/* 函数声明 */
SDRAM_TestStatus SDRAM_Test_Simple(void);
SDRAM_TestStatus SDRAM_Test_FullRange(void);
SDRAM_TestStatus SDRAM_Test_AddressBus(void);
SDRAM_TestStatus SDRAM_Test_DataBus(void);
SDRAM_TestStatus SDRAM_Test_MarchingPattern(void);
SDRAM_TestStatus SDRAM_Test_Comprehensive(void);

void SDRAM_FillMemory(uint32_t start_addr, uint32_t size, uint32_t pattern);
SDRAM_TestStatus SDRAM_VerifyMemory(uint32_t start_addr, uint32_t size, uint32_t pattern);
void SDRAM_PrintTestResult(const char* test_name, SDRAM_TestStatus status);

#ifdef __cplusplus
}
#endif

#endif /* __XQ_SDRAM_H */
