#include "xq_period_task.h"
#include "usart.h"
#include "xq_encoder.h"
#include "xq_axis.h"
#include <stdlib.h>

/**
 * @brief 启动周期性任务
 * @param None
 * @return HAL状态
 * @note 使用LPTIM1定时器，每200us触发一次中断
 * 
 * LPTIM1配置说明：
 * - LPTIM1时钟源：100MHz
 * - 预分频器：8
 * - 实际工作频率：100MHz / 8 = 12.5MHz
 * - 目标周期：200us
 * - 计算：200us × 12.5MHz = 200 × 10^-6 × 12.5 × 10^6 = 2500
 * - ARR值：2500 - 1 = 2499 (0x9C3)
 */
HAL_StatusTypeDef XQ_PeriodTask_Start(void)
{
  HAL_StatusTypeDef status;
  
  // 启动LPTIM1定时器中断模式
  // ARR = 2499，实现200us周期
  // 计算过程：100MHz / 8 = 12.5MHz, 200us × 12.5MHz = 2500, ARR = 2500-1 = 2499
  // status = HAL_LPTIM_Counter_Start_IT(&hlptim1, 2499);
  
  // if (status == HAL_OK) {
  //     printf("Periodic task started successfully!\r\n");
  // } else {
  //     printf("Failed to start periodic task! Error: %d\r\n", status);
  // }
  
  return status;
}


/**
 * @brief LPTIM1周期完成中断回调函数
 * @param hlptim: LPTIM句柄指针
 * @return None
 * @note 这个函数会被HAL库自动调用，然后调用我们的任务回调函数
 */
void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{  
  if (hlptim->Instance == LPTIM1) {
    // 周期性任务已移到stm32h7xx_it.c的LPTIM1_IRQHandler中执行
    // 以提高响应优先级并减少函数调用开销
  }
}


/**
 * @brief 周期性任务中断回调函数
 * @param None
 * @return None
 * @note 在LPTIM1中断中被调用，每200us执行一次
 *       这里可以添加需要周期性执行的任务代码
 */
void XQ_PeriodTask_Callback(void)
{

  // static uint32_t counter = 0;
  // static uint32_t counter_if = 5000;

  // if (counter++ >= counter_if) { // 每0.1秒执行一次
  //   counter = 0;
  //   float_t random_speed = (float_t)((rand() % 6) + 1);
  //   counter_if = (uint32_t)((rand() % 5000));
  //   // XQ_JogMove(AXIS_0, random_speed, 2.0f, 20.0f, 5);
  //   XQ_ABSMove(AXIS_0, counter_if, random_speed, 2.0f, 20.0f, 5);
  // }

}


