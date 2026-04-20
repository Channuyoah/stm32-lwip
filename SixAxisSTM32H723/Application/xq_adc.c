#include "xq_adc.h"
#include "xq_pwm.h"
#include "tim.h"
#include "usart.h"
#include "string.h"
#include <stdio.h>
#include <stdbool.h>

__attribute__((section(".d3ram_adc1"))) uint32_t adc1_buffer[1024];
__attribute__((section(".d3ram_adc3"))) uint16_t adc3_buffer[1024]; /* 半字 */

static uint16_t app_adc1_buffer[1024];
static uint16_t app_adc3_buffer[1024];

static volatile uint8_t adc1_running = 0;  // ADC运行状态: 0=停止, 1=运行
static volatile uint8_t adc3_running = 0;  // ADC运行状态: 0=停止, 1=运行

/* 最近2次高电平历史，[0]=最近一次                                                          */
/* pwm_high_offset_us[i]   : 上升沿距 ADC 窗口起点的时间(us)                               */
/* pwm_high_duration_us[i] : 高电平时长(us)，                                             */
#define PWM_HIGH_HISTORY_SIZE  2u
volatile float_t  pwm_high_offset_us[PWM_HIGH_HISTORY_SIZE];   // 上升沿距窗口起点(us)，找不到为0
volatile float_t  pwm_high_duration_us[PWM_HIGH_HISTORY_SIZE]; // 高电平时长(us)，找不到为0


/**
 * @brief 获取ADC采样数据并转换为电压值
 * @param cn: 通道编号 (0=ADC1+ADC2交叉采样, 1=ADC3单独采样)
 * @param Volt: 输出电压数组指针
 * @param num: 需要获取的数据点数
 * @return HAL状态
 * @note 直接从应用缓冲区获取数据，需要先调用XQ_StartADC启动采样
 */
HAL_StatusTypeDef 
XQ_GetADC(uint8_t cn, float_t *Volt, uint16_t num) {
  
  // 参数检查
  if (cn > 1) {
    printf("Error: Invalid ADC channel (%d), should be 0 or 1\r\n", cn);
    return HAL_ERROR;
  }
  
  if (Volt == NULL) {
    printf("Error: Volt pointer is NULL\r\n");
    return HAL_ERROR;
  }
  
  if (num == 0) {
    printf("Error: Invalid sample number (%d)\r\n", num);
    return HAL_ERROR;
  }

  /* 从应用缓冲区获取ADC1+ADC2交错采样数据 */
  if (cn == ADC_CHANNEL_DUAL_MODE) {
    
    // 检查ADC是否正在运行
    if (adc1_running == 0) {
      printf("Error: ADC dual mode is not running, call XQ_StartADC first\r\n");
      return HAL_ERROR;
    }
    
    // 限制获取的数据数量
    uint16_t max_samples = (num > 1000) ? 1000 : num;
    
    // 从应用缓冲区获取数据并转换为电压
    // app_adc1_buffer中存储的是交叉的ADC1和ADC2数据 [ADC1, ADC2, ADC1, ADC2, ...]
    for (uint16_t i = 0; i < max_samples; i++) {
      uint16_t adc_data = app_adc1_buffer[i];
      
      // 转换为电压: Volt = ADC_Value * (205.0/65.0) * 3.3 / 16383.0
      // 205.0/65.0 是电阻分压比，16383.0是14位ADC的最大值
      Volt[i] = adc_data * (205.0f/65.0f) * 3.3f / 16383.0f;
    }

  } else if (cn == ADC_CHANNEL_ADC3) {

    // 检查ADC是否正在运行
    if (adc3_running == 0) {
      printf("Error: ADC3 is not running, call XQ_StartADC first\r\n");
      return HAL_ERROR;
    }
    
    // 限制获取的数据数量
    uint16_t max_samples = (num > 1000) ? 1000 : num;
    
    // 从应用缓冲区获取ADC3数据并转换为电压
    for (uint16_t i = 0; i < max_samples; i++) {
      uint16_t adc3_data = app_adc3_buffer[i];
      
      // 转换为电压: Volt = ADC_Value * (205.0/65.0) * 3.3 / 4095.0
      // 4095.0是12位ADC的最大值 (2^12 - 1)
      Volt[i] = adc3_data * (205.0f/65.0f) * 3.3f / 4095.0f;
    }
  }
  
  return HAL_OK;
}


/**
 * @brief 启动ADC采样
 * @param cn: 通道编号 (0=ADC1+ADC2双模式, 1=ADC3单独采样)
 * @return HAL状态
 */
HAL_StatusTypeDef 
XQ_StartADC(uint8_t cn) {
  
  HAL_StatusTypeDef status = HAL_OK;
  
  // 参数检查
  if (cn > 1) {
    printf("Error: Invalid ADC channel (%d), should be 0 or 1\r\n", cn);
    return HAL_ERROR;
  }

  /* 开启TIM7 */
  HAL_TIM_Base_Start(&htim7);

  /* ADC1+ADC2交错采样模式（TIM6定时器触发，5MHz） */
  if (cn == ADC_CHANNEL_DUAL_MODE) {
    
    // 检查是否已经在运行
    if (adc1_running == 1) {
      printf("Warning: ADC dual mode is already running\r\n");
      return HAL_BUSY;
    }
    
    // ADC1校准
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    // ADC2校准
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    
    // 启动ADC1+ADC2交错模式DMA采样，500个双模式数据，实际采样点1000个
    status = HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)adc1_buffer, 500);

    if (status != HAL_OK) {
      printf("Error: ADC dual mode DMA start failed\r\n");
      return status;
    }

    // 设置运行状态
    adc1_running = 1;

  } else if (cn == ADC_CHANNEL_ADC3) {

    // 检查是否已经在运行
    if (adc3_running == 1) {
      printf("Warning: ADC3 is already running\r\n");
      return HAL_BUSY;
    }
    
    // ADC3校准
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_FACTOR_LINEARITY_REGOFFSET, ADC_SINGLE_ENDED);
    
    // 启动ADC3循环DMA采样，1000个数据，持续运行
    status = HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3_buffer, 1000);

    /* 清除定时器6计数 */
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    /* 定时器6开始，触发ADC采样 */
    HAL_TIM_Base_Start(&htim6);

    if (status != HAL_OK) {
      printf("Error: ADC3 DMA start failed\r\n");
      return status;
    }
    
    // 设置运行状态
    adc3_running = 1;
  }
  
  return status;
}


/**
 * @brief 停止ADC采样
 * @param cn: 通道编号 (0=ADC1+ADC2双模式, 1=ADC3单独采样)
 * @return HAL状态
 */
HAL_StatusTypeDef 
XQ_StopADC(uint8_t cn) {
  
  HAL_StatusTypeDef status = HAL_OK;
  
  // 参数检查
  if (cn > 1) {
    printf("Error: Invalid ADC channel (%d), should be 0 or 1\r\n", cn);
    return HAL_ERROR;
  }

  /* 停止ADC1+ADC2交错模式 */
  if (cn == ADC_CHANNEL_DUAL_MODE) {
    
    // 检查是否正在运行
    if (adc1_running != 1) {
      printf("Warning: ADC dual mode is not running\r\n");
      return HAL_OK;  // 已经停止，返回OK
    }
    
    // 停止ADC1+ADC2交错模式DMA采样
    status = HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    if (status != HAL_OK) {
      printf("Error: ADC dual mode DMA stop failed\r\n");
    }
    
    // 清除运行状态
    adc1_running = 0;

  } else if (cn == ADC_CHANNEL_ADC3) {

    // 检查是否正在运行
    if (adc3_running != 1) {
      printf("Warning: ADC3 is not running\r\n");
      return HAL_OK;  // 已经停止，返回OK
    }

    // 停止ADC3 DMA采样
    status = HAL_ADC_Stop_DMA(&hadc3);
    if (status != HAL_OK) {
      printf("Error: ADC3 DMA stop failed\r\n");
    }
    
    // 清除运行状态
    adc3_running = 0;
  }
  
  return status;
}

uint16_t t1 = 0, t2 = 0, t2t1 = 0;

/**
 * @brief ADC DMA转换完成回调函数
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance == ADC1) {

    /* 记录当前时刻的时间戳和PWM CNT */
    uint16_t current_pwm_cnt = TIM5->CNT;
    uint16_t current_pwm_count = pwm1_config->current_count;

    // ADC1+ADC2交错模式完成 - 后250个双模式数据
    SCB_InvalidateDCache_by_Addr((uint32_t*)adc1_buffer, sizeof(adc1_buffer));
    
    // 直接拷贝后250个双模式数据到应用缓冲区后半部分
    // adc1_buffer[i](uint32_t)低16位=ADC1、高16位=ADC2，与app_adc1_buffer交叉布局完全一致
    memcpy(&app_adc1_buffer[500], &adc1_buffer[250], 250 * sizeof(uint32_t));

    /* 记录最近2次高电平的起始位置和时长，找不到则为0 */
    pwm_high_offset_us[0] = pwm_high_offset_us[1] = 0.0f;
    pwm_high_duration_us[0] = pwm_high_duration_us[1] = 0.0f;

    if (pwm1_config->is_running) {
     
      /* 1000个ADC数据采样完成需要200us，计算这200us窗口对应的PWM高电平信息 */
      float_t  t_window    = 200.0f;

      /* PWM此时定时器CNT的值 */
      uint32_t tick_offset = current_pwm_cnt;

      /* 计算PWM总周期数 */
      uint32_t tc = pwm1_config->total_count[0] + pwm1_config->total_count[1];
      uint8_t  found   = 0;
      uint8_t  frame_n = 0;  /* 仅用于区分第0帧（partial）与历史帧 */

      /* frame_n < 64u 最多迭代64次 */
      while (found < PWM_HIGH_HISTORY_SIZE && frame_n < 64u) {

        /* 往前偏移到PWM定时器CNT是0的时刻，此时可能是一个高电平的开始时刻，也有可能是间隙模式的开始时刻 */
        float_t offset_n = t_window - tick_offset * 0.1f;
        
        if (offset_n < 0.0f) break;

        uint32_t frame     = (uint32_t)((uint16_t)current_pwm_count - (uint16_t)frame_n);

        /* in_normal用于判断当前帧是否处于正常PWM模式，还是停止模式 */
        uint8_t  in_normal = (tc == 0) || ((frame % tc) < pwm1_config->total_count[0]);

        /* 根据当前是正常模式还是停止模式，选择对应的CCR和ARR值 */
        uint16_t ccr_n     = in_normal ? pwm1_config->ccr[0] : pwm1_config->ccr[1];
        uint16_t arr_n     = in_normal ? pwm1_config->arr[0] : pwm1_config->arr[1];

        if (in_normal && ccr_n > 0) { /* 如果是正常模式且CCR大于0，此时PWM一定会有高电平 */
          pwm_high_offset_us[found]   = offset_n;
          pwm_high_duration_us[found] = (frame_n == 0 && current_pwm_cnt < ccr_n)
                                        ? current_pwm_cnt * 0.1f
                                        : ccr_n           * 0.1f;
          found++;
        }
        tick_offset += (uint32_t)arr_n + 1u;
        frame_n++;
      }
    }

    // printf("PWM high offset(us): %.1f, %.1f; duration(us): %.1f, %.1f\r\n",
    //        pwm_high_offset_us[0], pwm_high_offset_us[1],
    //        pwm_high_duration_us[0], pwm_high_duration_us[1]);

  } else if (hadc->Instance == ADC3) {

    // t2 = TIM13->CNT;
    // t2t1 = t2 - t1;

    // ADC3单独采样完成 - 后500个数据
    SCB_InvalidateDCache_by_Addr((uint32_t*)adc3_buffer, sizeof(adc3_buffer));
    
    // 存储ADC3后半部分数据到应用缓冲区后半部分
    memcpy(&app_adc3_buffer[500], &adc3_buffer[500], 500 * sizeof(uint16_t));

  }
}


/**
 * @brief ADC DMA转换半完成回调函数
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance == ADC1) {

    // ADC1+ADC2交错模式半完成 - 前250个双模式数据
    SCB_InvalidateDCache_by_Addr((uint32_t*)adc1_buffer, 250 * sizeof(uint32_t));
    
    // 直接拷贝前250个双模式数据到应用缓冲区前半部分
    memcpy(&app_adc1_buffer[0], &adc1_buffer[0], 250 * sizeof(uint32_t));
  }
  else if (hadc->Instance == ADC3) {

    // t1 = TIM13->CNT;

    // ADC3半完成 - 前500个数据
    SCB_InvalidateDCache_by_Addr((uint32_t*)adc3_buffer, 500 * sizeof(uint16_t));
    
    // 存储ADC3前半部分数据到应用缓冲区前半部分
    memcpy(&app_adc3_buffer[0], &adc3_buffer[0], 500 * sizeof(uint16_t));
  }
}

