#ifndef __XQ_PERIOD_TASK_H__
#define __XQ_PERIOD_TASK_H__

#include "main.h"
#include "lptim.h"
#include "FreeRTOS.h"
#include "task.h"

/* 周期性任务相关函数和变量声明 */
extern TaskHandle_t xAnalogRefreshTaskHandle;

/**
 * @brief 启动周期性任务
 * @param None
 * @return HAL状态
 * @note 使用LPTIM1定时器，每200us触发一次中断
 */
HAL_StatusTypeDef XQ_PeriodTask_Start(void);

/**
 * @brief 周期性任务中断回调函数
 * @param None
 * @return None
 * @note 在LPTIM1中断中被调用，每200us执行一次
 */
void XQ_PeriodTask_Callback(void);

/**
 * @brief 模拟输入刷新任务
 * @note 等待 200us 定时器通知，然后执行一次刷新
 */
void xq_Analog_Refresh_Task(void *argument);

#endif // !__XQ_PERIOD_TASK_H__
