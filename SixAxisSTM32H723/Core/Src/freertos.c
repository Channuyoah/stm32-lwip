/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "string.h"
#include "xq_axis.h"
#include "ht_client_task.h"
#include "ht_udp_server.h"
#include "mb.h"
#include "adc.h"
#include "xq_io.h"
#include "xq_encoder.h"
#include "xq_adc.h"
#include "xq_period_task.h"
#include "xq_pwm.h"
#include "xq_dac.h"
#include "tim.h"
#include "xq_sdram.h"
#include "xq_axis_intp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */

// uint8_t level[1000];
// float_t adc_voltage[1000];

/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */

  xq_axis_init();

  ht_client_task_init();

  ht_udp_server_init();

  XQ_TCA9535_Init_All();

  XQ_PeriodTask_Start();

  XQ_Encoder_Start(1, -1, -1, -1, 1);

  xq_modbus_tcp_task_init ();

  XQ_setPWM(0, 0.5, 10.0, 5, 100);
  XQ_setPWM(1, 0.5, 10.0, 5, 100);

  XQ_StartADC (ADC_CHANNEL_DUAL_MODE);
  XQ_StartADC (ADC_CHANNEL_ADC3);
  
  // 初始化DAC (0-5V范围)
  XQ_DAC_Init(GP8403_RANGE_10V);

  // 设置初始输出电压
  XQ_SetDAC(GP8403_CHANNEL_BOTH, 3.1f);

  /* 编码器绑定：编码器0 → 轴0，编码器一圈10000计数，电机一圈5000脉冲 */
  // XQ_Encoder_Bindto_Axis(AXIS_0, ENCODER_AXIS_0, 400, 5000, 20 * 5000, 50000, 50000, 5, 10);

  /* 启动编码器跟随任务 */
  // XQ_Encoder_Follow_Task_Start();

  /* 输出流水跑马灯测试：OUT0~OUT48 依次点亮，每次只亮一个 */
   uint8_t chaser_pin = 0;
   const uint8_t CHASER_TOTAL = 49;  // OUT0 ~ OUT48

  // xq_line_intp (AXIS_0, AXIS_1, 5000 * 100, 5000 * 10, 0, 0, 50000, 50000, 50000, 5);

  // xq_axis_pulse_timed(AXIS_0, 1000, 5000);

  /* Infinite loop */
  for(;;) {

    /* 流水输出测试OUT0 ~ OUT48 */
			 xqIO_Output = (1ULL << chaser_pin);
			 chaser_pin = (chaser_pin + 1) % CHASER_TOTAL;

    /* 打印IO输入 */
    // printf("DI: 0x%02X\r\n", (unsigned int)(xqIO_Input >> 32));  // 只打印高32位的输入状态（TCA9535部分）

    /* 检查输出是否发生变化 */

    // int64_t count = XQ_Axis_Get_Encoder_Count((XQ_EncoderAxis_t)0);
    // int32_t axis0_pulse = encoder_count_to_motor_pulse(&encoder_bindings[ENCODER_AXIS_0], count);

    // printf ("Axis %d: %lld  %d  %d\r\n", 0, count, axis0_pulse, axis[AXIS_0].position);  

    /* 打印所有编码器的值 */
    // for (uint8_t i = 0; i < ENCODER_AXIS_COUNT; i++) {
    //   int64_t count = XQ_Axis_Get_Encoder_Count((XQ_EncoderAxis_t)i);
    //   printf ("Axis %d: %lld  ", i, count);  
    // }
    // printf ("\r\n");

    /* 编码器0的值除以10000，作为轴0的绝对位置目标（直接使用脉冲值，省去浮点运算） */
    // {
    //   int64_t enc0_count = XQ_Axis_Get_Encoder_Count(ENCODER_AXIS_0);
    //   int32_t target_pulse = (int32_t)(enc0_count / 2);

    //   if (target_pulse != axis[AXIS_0].position) {
    //     if (axis[AXIS_0].is_moving == true) {
    //       /* 运动中动态改变目标位置，先将加速度减到零再规划新轨迹 */
    //       xq_axis_change_target_position(AXIS_0, target_pulse, 20 * 5000, 50000, 50000, 5);
    //     } else {
    //       /* 静止状态，根据目标位置创建S曲线并启动 */
    //       SCurve *s = xq_axis_new_curve_for_target_position(AXIS_0, axis[AXIS_0].min_start_f, 20 * 5000, target_pulse, 50000, 50000, 5, AXIS_POS_MODE_ABS);
    //       xq_axis_start_pwm(AXIS_0, s);
    //     }
    //   }
    // }

    // if(((xqIO_Input >> 8) & 1) && !di8)
		// {
    //   printf("DI8 triggered, starting Z-axis movement\r\n");
		// 	XQ_JogMove((AxisID)0, -1, 30000, 2450, 5);//下降找平面的速度mm/s
		// }
		// di8 = (xqIO_Input >> 8) & 1;
		
		// if(((xqIO_Input >> 9) & 1) && !di9)
		// {
		// 	XQ_ABSMove((AxisID)0, (axis[0].position * axis[0].pitch) / axis[0].microsteps, 1.5, 30000, 2450, 5);
		// }
		// di9 = (xqIO_Input >> 9) & 1;


    // PWM测试：每隔200ms改变一次占空比，实现渐变效果
    // if (osKernelGetTickCount() - pwm_tick >= 100) {
    //   pwm_tick = osKernelGetTickCount();
      
    //   // 更新占空比
    //   pwm_duty += pwm_direction * 0.05f;
      
    //   // 检查边界并反转方向
    //   if (pwm_duty >= 0.9f) {
    //     pwm_duty = 0.9f;
    //     pwm_direction = -1;  // 开始递减
    //   } else if (pwm_duty <= 0.1f) {
    //     pwm_duty = 0.1f;
    //     pwm_direction = 1;   // 开始递增
    //   }
      
    //   // 设置PWM占空比（周期500μs，间歇发送2个周期后停止1000μs）
    //   XQ_setPWM(0, pwm_duty, 500.0, 2, 1000);
    //   XQ_setPWM(1, pwm_duty, 500.0, 2, 1000);
    // }

    // 获取交叉采样的ADC数据（ADC1+ADC2双模式）

    // if (status == HAL_OK) {
    //   for (int i = 0; i < 200; i++) {
    //     printf ("%.3f ", adc_voltage[i]);
    //   }
    //   printf("\r\n");
    // }

    // uint32_t ti = t2t1;
    // printf ("t2t1 = %d\n", ti);
    // printf ("MSPS = %.2f\n", 1.0/((double_t)ti/500.0)); // 计算采样率


    /* 打印pwm1_level */
    // for (uint32_t i = 0; i < 1000; i++)
    // {
    //   printf ("%0.3f ", adc_voltage[i]);
    // }

    // printf ("\r\n");
    // printf ("t2t1 = %d\n", t2t1);
    
XQ_ABSMove((AxisID)0, 1, 5, 30000, 2450, 5);
osDelay(500);
printf ("1*************** \r\n");
XQ_ABSMove((AxisID)1, 1, 5, 30000, 2450, 5);
printf ("2*************** \r\n");
osDelay(1500);
XQ_ABSMove((AxisID)0, 0, 5, 30000, 2450, 5);
osDelay(500);
printf ("3*************** \r\n");
XQ_ABSMove((AxisID)1, 0, 5, 30000, 2450, 5);
printf ("4*************** \r\n");
osDelay(1500);
printf (" RUNNING*** \r\n");
printf ("***************************\r\n");
    HAL_GPIO_TogglePin (SYS_RUN_GPIO_Port, SYS_RUN_Pin); 
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */



/* USER CODE END Application */

