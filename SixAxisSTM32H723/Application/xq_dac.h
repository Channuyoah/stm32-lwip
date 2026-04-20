/**
 * @file    xq_dac.h
 * @brief   GP8403 DAC芯片驱动头文件
 * @details 双通道12位I2C DAC，输出范围0-5V或0-10V
 * @date    2025-12-29
 */

#ifndef __XQ_DAC_H
#define __XQ_DAC_H

#include "main.h"
#include "i2c.h"
#include <stdbool.h>

/* GP8403 I2C地址定义 (7位地址, HAL库调用时需左移1位) */
#define GP8403_I2C_ADDR          0x58  // A0=A1=A2=GND -> 7bit=0x58, 8bit(HAL)=0xB0

/* 输出范围枚举 */
typedef enum {
    GP8403_RANGE_5V  = 0x00,  // 0-5V输出范围
    GP8403_RANGE_10V = 0x11   // 0-10V输出范围
} GP8403_Range_t;

/* DAC通道枚举 */
typedef enum {
    GP8403_CHANNEL_0 = 0,     // 通道0
    GP8403_CHANNEL_1 = 1,     // 通道1
    GP8403_CHANNEL_BOTH = 2   // 同时设置两个通道
} GP8403_Channel_t;

/* API函数声明 */

/**
 * @brief 扫描I2C总线，打印所有响应设备的地址（调试用）
 */
void XQ_I2C_Scan(void);

/**
 * @brief 初始化DAC模块
 * @param range 输出范围(GP8403_RANGE_5V 或 GP8403_RANGE_10V)
 * @note GP8403的输出范围由硬件跳线决定，不需要软件配置
 * @return HAL_OK=成功, HAL_ERROR=失败
 */
HAL_StatusTypeDef XQ_DAC_Init(GP8403_Range_t range);

/**
 * @brief 设置DAC输出范围
 * @param range 输出范围(GP8403_RANGE_5V 或 GP8403_RANGE_10V)
 * @return HAL_OK=成功, HAL_ERROR=失败
 */
HAL_StatusTypeDef XQ_DAC_SetRange(GP8403_Range_t range);

/**
 * @brief 设置DAC输出电压
 * @param channel 通道号(0, 1 或 BOTH)
 * @param voltage 输出电压(0.0-5.0V 或 0.0-10.0V)
 * @return HAL_OK=成功, HAL_ERROR=失败
 */
HAL_StatusTypeDef XQ_SetDAC(GP8403_Channel_t channel, float voltage);

/**
 * @brief 获取DAC当前电压值
 * @param channel 通道号(0或1)
 * @return 当前电压值
 */
float XQ_DAC_GetVoltage(GP8403_Channel_t channel);

#endif /* __XQ_DAC_H */
