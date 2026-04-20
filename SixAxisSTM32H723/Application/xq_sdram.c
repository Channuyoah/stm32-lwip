/**
  ******************************************************************************
  * @file    xq_sdram.c
  * @brief   SDRAM 测试程序
  * @description 包含多种SDRAM测试方法，用于验证SDRAM工作是否正常
  ******************************************************************************
  */

#include "xq_sdram.h"
#include <stdio.h>
#include <string.h>

/**
  * @brief  简单的SDRAM读写测试
  * @param  None
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_Test_Simple(void)
{
    uint32_t test_addr = SDRAM_TEST_START_ADDR;
    uint32_t write_value = 0x55AA55AA;
    uint32_t read_value = 0;
    
    printf("开始简单读写测试...\n");
    
    // 写入数据
    *(volatile uint32_t *)test_addr = write_value;
    
    // 读取数据
    read_value = *(volatile uint32_t *)test_addr;
    
    // 验证数据
    if (read_value == write_value) {
        printf("  简单读写测试: 通过 (地址:0x%08lX, 数据:0x%08lX)\n", test_addr, read_value);
        return SDRAM_TEST_OK;
    } else {
        printf("  简单读写测试: 失败 (写入:0x%08lX, 读取:0x%08lX)\n", write_value, read_value);
        return SDRAM_TEST_FAILED;
    }
}

/**
  * @brief  填充SDRAM内存
  * @param  start_addr: 起始地址
  * @param  size: 填充大小(字节)
  * @param  pattern: 填充模式
  * @retval None
  */
void SDRAM_FillMemory(uint32_t start_addr, uint32_t size, uint32_t pattern)
{
    uint32_t *pAddr = (uint32_t *)start_addr;
    uint32_t count = size / 4;
    
    for (uint32_t i = 0; i < count; i++) {
        *pAddr++ = pattern;
    }
}

/**
  * @brief  验证SDRAM内存
  * @param  start_addr: 起始地址
  * @param  size: 验证大小(字节)
  * @param  pattern: 期望模式
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_VerifyMemory(uint32_t start_addr, uint32_t size, uint32_t pattern)
{
    uint32_t *pAddr = (uint32_t *)start_addr;
    uint32_t count = size / 4;
    
    for (uint32_t i = 0; i < count; i++) {
        if (*pAddr != pattern) {
            printf("  验证失败: 地址=0x%08X, 期望=0x%08X, 实际=0x%08X\n", 
                   (uint32_t)pAddr, pattern, *pAddr);
            return SDRAM_TEST_FAILED;
        }
        pAddr++;
    }
    
    return SDRAM_TEST_OK;
}

/**
  * @brief  全范围SDRAM测试
  * @param  None
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_Test_FullRange(void)
{
    uint32_t test_patterns[] = {0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA};
    uint32_t pattern_count = sizeof(test_patterns) / sizeof(test_patterns[0]);
    
    printf("开始全范围测试 (大小: %lu KB)...\n", SDRAM_TEST_SIZE / 1024);
    
    for (uint32_t i = 0; i < pattern_count; i++) {
        uint32_t pattern = test_patterns[i];
        printf("  测试模式: 0x%08lX\n", pattern);
        
        // 填充内存
        printf("    填充内存...\n");
        SDRAM_FillMemory(SDRAM_TEST_START_ADDR, SDRAM_TEST_SIZE, pattern);
        
        // 验证内存
        printf("    验证内存...\n");
        if (SDRAM_VerifyMemory(SDRAM_TEST_START_ADDR, SDRAM_TEST_SIZE, pattern) != SDRAM_TEST_OK) {
            printf("  全范围测试: 失败 (模式:0x%08lX)\n", pattern);
            return SDRAM_TEST_FAILED;
        }
    }
    
    printf("  全范围测试: 通过\n");
    return SDRAM_TEST_OK;
}

/**
  * @brief  地址线测试
  * @param  None
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_Test_AddressBus(void)
{
    uint32_t pattern = 0xAAAAAAAA;
    uint32_t antipattern = 0x55555555;
    uint32_t addressMask = (SDRAM_TEST_SIZE - 1);
    uint32_t offset;
    uint32_t testOffset;
    
    printf("开始地址线测试...\n");
    
    // 写入模式到所有地址位
    for (offset = sizeof(uint32_t); (offset & addressMask) != 0; offset <<= 1) {
        *(volatile uint32_t *)(SDRAM_TEST_START_ADDR + offset) = pattern;
    }
    
    // 写入反模式到基地址
    *(volatile uint32_t *)SDRAM_TEST_START_ADDR = antipattern;
    
    // 检查所有地址位
    for (offset = sizeof(uint32_t); (offset & addressMask) != 0; offset <<= 1) {
        if (*(volatile uint32_t *)(SDRAM_TEST_START_ADDR + offset) != pattern) {
            printf("  地址线测试: 失败 (偏移:0x%08lX)\n", offset);
            return SDRAM_TEST_FAILED;
        }
    }
    
    // 检查基地址
    if (*(volatile uint32_t *)SDRAM_TEST_START_ADDR != antipattern) {
        printf("  地址线测试: 失败 (基地址)\n");
        return SDRAM_TEST_FAILED;
    }
    
    printf("  地址线测试: 通过\n");
    return SDRAM_TEST_OK;
}

/**
  * @brief  数据线测试
  * @param  None
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_Test_DataBus(void)
{
    uint32_t pattern;
    
    printf("开始数据线测试...\n");
    
    // 测试每一位数据线
    for (pattern = 1; pattern != 0; pattern <<= 1) {
        *(volatile uint32_t *)SDRAM_TEST_START_ADDR = pattern;
        
        if (*(volatile uint32_t *)SDRAM_TEST_START_ADDR != pattern) {
            printf("  数据线测试: 失败 (模式:0x%08lX)\n", pattern);
            return SDRAM_TEST_FAILED;
        }
    }
    
    printf("  数据线测试: 通过\n");
    return SDRAM_TEST_OK;
}

/**
  * @brief  行进模式测试(March C算法)
  * @param  None
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_Test_MarchingPattern(void)
{
    // 使用较小的测试区域以节省时间
    uint32_t test_size = SDRAM_TEST_SIZE / 16;  // 测试1/16的区域
    uint32_t *pAddr;
    uint32_t offset;
    
    printf("开始行进模式测试 (测试大小: %lu KB)...\n", test_size / 1024);
    
    // Step 1: 写0 (向上)
    printf("  步骤1: 写入0...\n");
    pAddr = (uint32_t *)SDRAM_TEST_START_ADDR;
    for (offset = 0; offset < test_size / 4; offset++) {
        *pAddr++ = 0x00000000;
    }
    
    // Step 2: 读0写1 (向上)
    printf("  步骤2: 读0写1...\n");
    pAddr = (uint32_t *)SDRAM_TEST_START_ADDR;
    for (offset = 0; offset < test_size / 4; offset++) {
        if (*pAddr != 0x00000000) {
            printf("  行进模式测试: 失败 (步骤2, 地址:0x%08lX)\n", (uint32_t)pAddr);
            return SDRAM_TEST_FAILED;
        }
        *pAddr++ = 0xFFFFFFFF;
    }
    
    // Step 3: 读1写0 (向上)
    printf("  步骤3: 读1写0...\n");
    pAddr = (uint32_t *)SDRAM_TEST_START_ADDR;
    for (offset = 0; offset < test_size / 4; offset++) {
        if (*pAddr != 0xFFFFFFFF) {
            printf("  行进模式测试: 失败 (步骤3, 地址:0x%08lX)\n", (uint32_t)pAddr);
            return SDRAM_TEST_FAILED;
        }
        *pAddr++ = 0x00000000;
    }
    
    // Step 4: 读0 (向下)
    printf("  步骤4: 验证0...\n");
    pAddr = (uint32_t *)(SDRAM_TEST_START_ADDR + test_size - 4);
    for (offset = 0; offset < test_size / 4; offset++) {
        if (*pAddr != 0x00000000) {
            printf("  行进模式测试: 失败 (步骤4, 地址:0x%08X)\n", (uint32_t)pAddr);
            return SDRAM_TEST_FAILED;
        }
        pAddr--;
    }
    
    printf("  行进模式测试: 通过\n");
    return SDRAM_TEST_OK;
}

/**
  * @brief  综合测试(执行所有测试)
  * @param  None
  * @retval SDRAM_TestStatus: 测试结果
  */
SDRAM_TestStatus SDRAM_Test_Comprehensive(void)
{
    printf("\n========================================\n");
    printf("       SDRAM 综合测试开始\n");
    printf("========================================\n");
    printf("SDRAM 起始地址: 0x%08X\n", SDRAM_TEST_START_ADDR);
    printf("SDRAM 大小: %lu MB\n", SDRAM_TEST_SIZE / (1024 * 1024));
    printf("========================================\n\n");
    
    // 测试1: 简单读写测试
    if (SDRAM_Test_Simple() != SDRAM_TEST_OK) {
        printf("\n>>> 测试终止: 简单读写测试失败\n");
        return SDRAM_TEST_FAILED;
    }
    printf("\n");
    
    // 测试2: 数据线测试
    if (SDRAM_Test_DataBus() != SDRAM_TEST_OK) {
        printf("\n>>> 测试终止: 数据线测试失败\n");
        return SDRAM_TEST_FAILED;
    }
    printf("\n");
    
    // 测试3: 地址线测试
    if (SDRAM_Test_AddressBus() != SDRAM_TEST_OK) {
        printf("\n>>> 测试终止: 地址线测试失败\n");
        return SDRAM_TEST_FAILED;
    }
    printf("\n");
    
    // 测试4: 行进模式测试
    if (SDRAM_Test_MarchingPattern() != SDRAM_TEST_OK) {
        printf("\n>>> 测试终止: 行进模式测试失败\n");
        return SDRAM_TEST_FAILED;
    }
    printf("\n");
    
    // 测试5: 全范围测试
    if (SDRAM_Test_FullRange() != SDRAM_TEST_OK) {
        printf("\n>>> 测试终止: 全范围测试失败\n");
        return SDRAM_TEST_FAILED;
    }
    printf("\n");
    
    printf("========================================\n");
    printf("  所有SDRAM测试通过！\n");
    printf("========================================\n\n");
    
    return SDRAM_TEST_OK;
}

/**
  * @brief  打印测试结果
  * @param  test_name: 测试名称
  * @param  status: 测试状态
  * @retval None
  */
void SDRAM_PrintTestResult(const char* test_name, SDRAM_TestStatus status)
{
    if (status == SDRAM_TEST_OK) {
        printf("[通过] %s\n", test_name);
    } else {
        printf("[失败] %s\n", test_name);
    }
}
