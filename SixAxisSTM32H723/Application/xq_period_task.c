#include "xq_period_task.h"
#include "usart.h"
#include "xq_encoder.h"
#include "xq_axis.h"
#include "xq_adc.h"
#include "xq_modbus.h"
#include <stdlib.h>
#include <string.h>

// 任务句柄，用于发通知
TaskHandle_t xAnalogRefreshTaskHandle = NULL;

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
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // 向模拟刷新任务发送通知
  if (xAnalogRefreshTaskHandle != NULL) {
      xTaskNotifyFromISR(xAnalogRefreshTaskHandle, 0x01, eSetBits, &xHigherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief 模拟输入刷新任务
 * @note 等待 200us 定时器通知，然后执行一次刷新
 */
void xq_Analog_Refresh_Task(void *argument)
{
    const uint32_t AXIS_STATUS_INTERVAL = 10;   // 10次通知 = 2ms
    uint32_t count = 0;

    for (;;) {
        uint32_t ulNotifiedValue;

        // 等待通知，最大等待 500ms（防止中断出问题时任务永远卡死）
        BaseType_t result = xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifiedValue, pdMS_TO_TICKS(500));

        // 收到通知或超时后都执行一次刷新
        if (result == pdTRUE || ulNotifiedValue == 0) {
            // 刷新模拟输入
            xq_refresh_analog_inputs();

            // 每AXIS_STATUS_INTERVAL次刷新一次轴状态
            if (++count >= AXIS_STATUS_INTERVAL) {
                count = 0;
                xq_update_axis_status();
            }
        }
    }
}

/**
 * @brief 刷新模拟输入数据
 * @note 从ADC获取最新的模拟输入值，并更新到Modbus寄存器中
 */
void xq_refresh_analog_inputs(void)
{
    static uint32_t call_count = 0;
    static uint16_t local_buf[1000];   // 使用原始 ADC 值，避免浮点拷贝耗时

    // 模拟输入1 (1200) – 来自 ADC1+ADC2 双模式
    if (adc1_running) {
        /* 关中断，原子拷贝 1000 个 uint16_t */
        __disable_irq();
        memcpy(local_buf, app_adc1_buffer, 1000 * sizeof(uint16_t));
        __enable_irq();

        // 在中断打开后慢慢做浮点转换和平均
        float sum = 0.0f;
        for (int i = 0; i < 500; i++) {
            uint16_t adc_val = local_buf[i * 2];      // ADC1 数据
            float volt = adc_val * (205.0f/65.0f) * 3.3f / 16383.0f;
            sum += volt;
        }
        float ai1 = sum / 500.0f;
        SetFloatToReg(&usRegInputBuf[REG_AI1_ADDR - MB_INPUT_START_ADDR], ai1);

        if ((call_count % 100) == 0) {
            printf("AI1 = %.3f V\r\n", ai1);
        }
    }

    // 模拟输入2 (1202) – 来自 ADC3
    if (adc3_running) {
        __disable_irq();
        memcpy(local_buf, app_adc3_buffer, 1000 * sizeof(uint16_t));
        __enable_irq();

        float sum = 0.0f;
        for (int i = 0; i < 1000; i++) {
            float volt = local_buf[i] * (205.0f/65.0f) * 3.3f / 4095.0f;
            sum += volt;
        }
        float ai2 = sum / 1000.0f;
        SetFloatToReg(&usRegInputBuf[REG_AI2_ADDR - MB_INPUT_START_ADDR], ai2);

        if ((call_count % 100) == 0) {
            printf("AI2 = %.3f V\r\n", ai2);
        }
    }
    call_count++;
}
