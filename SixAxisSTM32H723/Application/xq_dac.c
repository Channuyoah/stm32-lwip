/**
 * @file    xq_dac.c
 * @brief   GP8403 DAC芯片驱动实现
 * @details 双通道12位I2C DAC，支持0-5V和0-10V输出范围
 * @date    2025-12-29
 */

#include "xq_dac.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* DAC运行时状态数据 */
static struct {
    GP8403_Range_t range;
    float ch0_voltage;
    float ch1_voltage;
    bool initialized;
} dac_runtime = {GP8403_RANGE_5V, 0.0f, 0.0f, false};

/* I2C超时时间 */
#define I2C_TIMEOUT  100  // ms

/**
 * @brief 扫描I2C总线，打印所有响应的设备地址
 */
void XQ_I2C_Scan(void)
{
    printf("Scanning I2C bus...\r\n");
    int found = 0;
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 10) == HAL_OK) {
            printf("  Found device at 7-bit addr=0x%02X (8-bit=0x%02X)\r\n", addr, addr << 1);
            found++;
        }
    }
    if (found == 0) {
        printf("  No devices found! Check SDA/SCL wiring and pull-up resistors.\r\n");
    } else {
        printf("Scan complete: %d device(s) found.\r\n", found);
    }
}

/**
 * @brief 初始化DAC模块
 */
HAL_StatusTypeDef XQ_DAC_Init(GP8403_Range_t range)
{
    HAL_StatusTypeDef status;

    XQ_I2C_Scan();  // 自动扫描I2C设备，诊断连接问题
    
    printf("Initializing GP8403 (I2C addr=0x%02X)...\r\n", GP8403_I2C_ADDR);
    
    // 检查I2C设备是否存在
    status = HAL_I2C_IsDeviceReady(&hi2c1, GP8403_I2C_ADDR << 1, 3, I2C_TIMEOUT);
    if (status != HAL_OK) {
        printf("Error: GP8403 device not found on I2C bus (status=%d)\r\n", status);
        printf("Please check: 1) I2C wiring 2) Power supply 3) I2C address\r\n");
        // XQ_I2C_Scan();  // 自动扫描帮助诊断
        return HAL_ERROR;
    }
    printf("GP8403 device found on I2C bus\r\n");
    
    // 初始化状态
    dac_runtime.ch0_voltage = 0.0f;
    dac_runtime.ch1_voltage = 0.0f;
    dac_runtime.initialized = true;
    
    // 设置输出范围
    status = XQ_DAC_SetRange(range);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }
    
    // 初始化输出为0V
    status = XQ_SetDAC(GP8403_CHANNEL_BOTH, 0.0f);
    if (status != HAL_OK) {
        printf("Error: GP8403 initial output set failed\r\n");
        return HAL_ERROR;
    }
    
    printf("GP8403 initialized successfully\r\n");
    
    return HAL_OK;
}

/**
 * @brief 设置DAC输出范围
 */
HAL_StatusTypeDef XQ_DAC_SetRange(GP8403_Range_t range)
{
    HAL_StatusTypeDef status;
    
    // GP8403设置输出范围: 寄存器0x01
    // 0x00=5V, 0x11=10V
    uint8_t range_value = range;
    status = HAL_I2C_Mem_Write(&hi2c1, GP8403_I2C_ADDR << 1, 0x01, I2C_MEMADD_SIZE_8BIT, &range_value, 1, I2C_TIMEOUT);
    
    if (status != HAL_OK) {
        printf("Error: GP8403 range configuration failed (status=%d)\r\n", status);
        return HAL_ERROR;
    }
    
    // 更新运行时状态
    dac_runtime.range = range;
    
    printf("GP8403 output range set to %s\r\n", 
           (range == GP8403_RANGE_5V) ? "0-5V" : "0-10V");
    
    return HAL_OK;
}


/**
 * @brief 设置DAC输出电压
 * @param channel 通道号(0, 1 或 BOTH)
 * @param voltage 输出电压(0.0-5.0V 或 0.0-10.0V)
 * @return HAL_OK=成功, HAL_ERROR=失败
 */
HAL_StatusTypeDef XQ_SetDAC(GP8403_Channel_t channel, float voltage)
{
    if (!dac_runtime.initialized) {
        printf("Error: GP8403 not initialized\r\n");
        return HAL_ERROR;
    }
    
    // 获取电压范围
    float max_voltage = (dac_runtime.range == GP8403_RANGE_5V) ? 5.0f : 10.0f;
    
    // 参数检查
    if (voltage < 0.0f || voltage > max_voltage) {
        printf("Error: Voltage %.2fV out of range [0-%.1fV]\r\n", voltage, max_voltage);
        return HAL_ERROR;
    }
    
    if (channel > GP8403_CHANNEL_BOTH) {
        printf("Error: Invalid channel %d\r\n", channel);
        return HAL_ERROR;
    }
    
    // 转换为12位DAC值 (0-4095)
    uint16_t dac_value = (uint16_t)roundf(voltage / max_voltage * 4095.0f);
    
    // 限制范围
    if (dac_value > 4095) {
        dac_value = 4095;
    }
    
    // 准备I2C数据
    uint8_t dac_data[2];
    // GP8403数据格式（关键：无视DATA Low的低4位）：
    // 12bit数据左移4位，对齐到16bit格式
    // bit[15:4] = 12bit数据, bit[3:0] = 被忽略
    uint16_t value_shifted = dac_value << 4;  // 左移4位
    dac_data[0] = value_shifted & 0xFF;         // DATA Low (低字节)
    dac_data[1] = (value_shifted >> 8) & 0xFF;  // DATA High (高字节)
    
    // 写入对应通道
    HAL_StatusTypeDef status = HAL_OK;
    
    if (channel == GP8403_CHANNEL_0 || channel == GP8403_CHANNEL_BOTH) {
        // 通道0: 寄存器0x02
        status = HAL_I2C_Mem_Write(&hi2c1, GP8403_I2C_ADDR << 1, 0x02, I2C_MEMADD_SIZE_8BIT, dac_data, 2, I2C_TIMEOUT);
        if (status != HAL_OK) {
            printf("Error: Channel 0 write failed (status=%d)\r\n", status);
            return status;
        }
    }
    
    if (channel == GP8403_CHANNEL_1 || channel == GP8403_CHANNEL_BOTH) {
        // 通道1: 寄存器0x04
        status = HAL_I2C_Mem_Write(&hi2c1, GP8403_I2C_ADDR << 1, 0x04, I2C_MEMADD_SIZE_8BIT, dac_data, 2, I2C_TIMEOUT);
        if (status != HAL_OK) {
            printf("Error: Channel 1 write failed (status=%d)\r\n", status);
            return status;
        }
    }
    
    // 更新运行时状态
    if (status == HAL_OK) {
        if (channel == GP8403_CHANNEL_0 || channel == GP8403_CHANNEL_BOTH) {
            dac_runtime.ch0_voltage = voltage;
        }
        if (channel == GP8403_CHANNEL_1 || channel == GP8403_CHANNEL_BOTH) {
            dac_runtime.ch1_voltage = voltage;
        }
    }
    
    return status;
}

/**
 * @brief 获取DAC当前电压值
 */
float XQ_DAC_GetVoltage(GP8403_Channel_t channel)
{
    if (channel > GP8403_CHANNEL_1) {
        return -1.0f;
    }
    
    if (channel == GP8403_CHANNEL_0) {
        return dac_runtime.ch0_voltage;
    } else {
        return dac_runtime.ch1_voltage;
    }
}

