#ifndef __XQ_ADC_H__
#define __XQ_ADC_H__

#include "main.h"
#include "adc.h"

// ADC通道定义
#define ADC_CHANNEL_DUAL_MODE    0  // ADC1+ADC2双模式交叉采样
#define ADC_CHANNEL_ADC3         1  // ADC3单独采样

extern uint16_t t2t1;

// 函数声明
/**
 * @brief 获取ADC采样数据并转换为电压值
 * @param cn: 通道编号 (0=ADC1+ADC2双模式, 1=ADC3单独采样)
 * @param Volt: 输出电压数组指针
 *              - 通道0: 需要2*num大小，格式为[ADC1[0], ADC2[0], ADC1[1], ADC2[1], ...]
 *              - 通道1: 需要num大小，格式为[ADC3[0], ADC3[1], ...]
 * @param num: 采样点数 (1-1024)
 * @return HAL状态
 * @note 电压转换公式: Volt = ADC_Value * (205.0/65.0) * 3.3 / 4095.0
 */
HAL_StatusTypeDef XQ_GetADC(uint8_t cn, float_t *Volt, uint16_t num);

/**
 * @brief 启动ADC采样
 * @param cn: 通道编号 (0=ADC1+ADC2双模式, 1=ADC3单独采样)
 * @param num: 采样点数 (1-1024)
 * @return HAL状态
 */
HAL_StatusTypeDef XQ_StartADC(uint8_t cn);

/**
 * @brief 停止ADC采样
 * @param cn: 通道编号 (0=ADC1+ADC2双模式, 1=ADC3单独采样)
 * @return HAL状态
 */
HAL_StatusTypeDef XQ_StopADC(uint8_t cn);

extern volatile uint8_t adc1_running;
extern volatile uint8_t adc3_running;

#endif // !__XQ_ADC_H__
