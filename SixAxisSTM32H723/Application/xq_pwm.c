#include "xq_pwm.h"
#include "usart.h"
#include <stdio.h>
#include <math.h>

/* 定时器时钟周期定义（PSC=24时，250MHz/25=10MHz，周期=100ns）*/
#define TIM_CLOCK_PERIOD_NS  100.0f  // 定时器计数周期(纳秒)

/* PWM通道配置数组，PSC=24时计数频率=250MHz/25=10MHz，分辨率=100ns */
PWM_Config_t pwm_configs[] = {
    {&htim5, TIM_CHANNEL_1, "TIM5_CH1", 10000000, 0, {0,0}, {0,0}, {0,0}, 0},
    {&htim1, TIM_CHANNEL_2, "TIM1_CH2", 10000000, 0, {0,0}, {0,0}, {0,0}, 0}
};


PWM_Config_t *pwm1_config = &pwm_configs[0];
PWM_Config_t *pwm2_config = &pwm_configs[1];


/**
 * @brief 设置PWM输出参数（带间歇发送功能）- 新版本
 * @param cn: PWM通道编号 (0=TIM5_CH1, 1=TIM1_CH2)
 * @param ton: 占空比（0.0-1.0），0表示关闭PWM输出
 * @param T: PWM周期时间（微秒μs），范围：0.2-5000μs
 * @param T_count: 连续发送的PWM周期数（0=连续发送）
 * @param idle_T: 停止时间（微秒μs），范围：0.2-5000μs，在此期间输出低电平
 * @return HAL状态
 * 
 * @note ton=0时停止PWM输出
 * @note T和idle_T支持周期范围：0.2μs - 5000μs
 * 
 * @example
 *   // 连续发送模式
 *   XQ_setPWM(0, 0.5, 100.0, 0, 0.0);
 *   
 *   // 间歇发送模式：发送10个周期(1000μs) → 停止500μs → 循环
 *   XQ_setPWM(0, 0.5, 100.0, 10, 500.0);
 *   
 *   // 发送1个脉冲后停止1ms
 *   XQ_setPWM(0, 0.5, 100.0, 1, 1000.0);
 *   
 *   // 停止输出
 *   XQ_setPWM(0, 0.0, 0, 0, 0.0);
 */
HAL_StatusTypeDef 
XQ_setPWM(uint8_t cn, float_t ton, float_t T, uint32_t T_count, float_t idle_T) {

  PWM_Config_t* config = &pwm_configs[cn];

  // 参数检查
  if (cn >= 2) {
      printf("Error: Invalid PWM channel (%d), should be 0 or 1\r\n", cn);
      return HAL_ERROR;
  }

  if (T < 0.2f || T > 5000.0f) {
      printf("Error: PWM period (%.2fμs) out of range [0.2-5000μs]\r\n", T);
      return HAL_ERROR;
  }
  
  if (ton < 0.0f || ton > 1.0f) {
      printf("Error: Duty cycle (%.3f) out of range [0.0-1.0]\r\n", ton);
      return HAL_ERROR;
  }

  // 如果占空比为0，停止PWM输出
  if (ton == 0.0f) {
    config->is_running = 0;
    HAL_TIM_PWM_Stop(config->htim, config->channel);
    __HAL_TIM_DISABLE_IT(config->htim, TIM_IT_UPDATE);
    return HAL_OK;
  }
  
  /* 计算正常模式的ARR和CCR值（使用纳秒运算，精度更高）*/
  float_t T_ns = T * 1000.0f;  // μs → ns
  config->arr[0] = (uint16_t)roundf(T_ns / TIM_CLOCK_PERIOD_NS) - 1;  // 正常PWM周期（四舍五入）
  config->ccr[0] = (uint16_t)roundf((config->arr[0] + 1) * ton);      // 正常PWM占空比（四舍五入）
  config->ccr[1] = 0;  // 停止模式CCR=0（强制低电平）
  config->total_count[0] = T_count;  // 正常模式发送周期数
  
  /*  计算停止模式的ARR和CCR值（使用纳秒运算，精度更高）*/
  if (T_count == 0) { /* 连续发送模式 */

    config->total_count[1] = 0;  // 停止模式不使用

  } else { /* 间歇发送模式 */

    // idle_T范围检查
    if (idle_T < 0.2f || idle_T > 5000.0f) {
      printf("Error: idle_T (%.2fμs) out of range [0.2-5000μs]\r\n", idle_T);
      return HAL_ERROR;
    }

    float_t idle_T_ns = idle_T * 1000.0f;  // μs → ns
    config->arr[1] = (uint16_t)roundf(idle_T_ns / TIM_CLOCK_PERIOD_NS) - 1;  // 停止期间的周期（四舍五入）
    config->ccr[1] = 0;  // 停止期间CCR=0（强制低电平）
    
    config->total_count[1] = 1;        // 停止模式只需1个周期（时长=idle_T）
  }

  if (!config->is_running) {

    if (config->htim == &htim1) {
      HAL_NVIC_SetPriority(TIM1_UP_IRQn, 0, 0);
      HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
    } else if (config->htim == &htim5) {
      HAL_NVIC_SetPriority(TIM5_IRQn, 0, 0);
      HAL_NVIC_EnableIRQ(TIM5_IRQn);
    }

    __HAL_TIM_SET_AUTORELOAD(config->htim, config->arr[0]);
    __HAL_TIM_SET_COMPARE(config->htim, config->channel, config->ccr[0]);

    // 手动触发更新事件，使ARR和CCR的预装载值立即生效
    config->htim->Instance->EGR = TIM_EGR_UG;

    // 清除更新中断标志，避免误触发
    __HAL_TIM_CLEAR_FLAG(config->htim, TIM_FLAG_UPDATE);

    // 需要启用中断
    __HAL_TIM_ENABLE_IT(config->htim, TIM_IT_UPDATE);

    config->is_running = 1;
    config->current_count = 0;

    // 启动PWM输出
    if (HAL_TIM_PWM_Start(config->htim, config->channel) != HAL_OK) {
      printf("Error: %s PWM start failed\r\n", config->name);
      return HAL_ERROR;
    }

  }

  return HAL_OK;
}



/**
 * @brief TIM5更新中断回调（直接操作寄存器）
 * @note 在TIM5_IRQHandler中直接调用
 */
void TIM5_UpdateCallback(void) {

  pwm1_config->current_count++;

  if (pwm1_config->total_count[1] != 0) { /* 间隙模式 */

    if (pwm1_config->current_count >= (pwm1_config->total_count[0] + pwm1_config->total_count[1])) {
      
      // 切换到重新开始模式
      __HAL_TIM_SET_AUTORELOAD(pwm1_config->htim, pwm1_config->arr[0]);
      __HAL_TIM_SET_COMPARE(pwm1_config->htim, pwm1_config->channel, pwm1_config->ccr[0]);

      pwm1_config->current_count = 0;  // 重新计数

    } else if (pwm1_config->current_count >= pwm1_config->total_count[0]) {

      // 切换到停止模式
      __HAL_TIM_SET_AUTORELOAD(pwm1_config->htim, pwm1_config->arr[1]);
      __HAL_TIM_SET_COMPARE(pwm1_config->htim, pwm1_config->channel, pwm1_config->ccr[1]);
    }
  } else { /* 连续模式 */
    return;  // 连续模式不处理
  }
}



/**
 * @brief TIM1更新中断回调（直接操作寄存器）
 * @note 在TIM1_UP_IRQHandler中直接调用
 */
void TIM1_UpdateCallback(void) {
  
  pwm2_config->current_count++;

  if (pwm2_config->total_count[1] != 0) { /* 间隙模式 */

    if (pwm2_config->current_count >= (pwm2_config->total_count[0] + pwm2_config->total_count[1])) {
      
      // 切换到重新开始模式
      __HAL_TIM_SET_AUTORELOAD(pwm2_config->htim, pwm2_config->arr[0]);
      __HAL_TIM_SET_COMPARE(pwm2_config->htim, pwm2_config->channel, pwm2_config->ccr[0]);
      pwm2_config->current_count = 0;  // 重新计数
      
    } else if (pwm2_config->current_count >= pwm2_config->total_count[0]) {

      // 切换到停止模式
      __HAL_TIM_SET_AUTORELOAD(pwm2_config->htim, pwm2_config->arr[1]);
      __HAL_TIM_SET_COMPARE(pwm2_config->htim, pwm2_config->channel, pwm2_config->ccr[1]);
    }
  } else { /* 连续模式 */
    return;  // 连续模式不处理
  }
}
