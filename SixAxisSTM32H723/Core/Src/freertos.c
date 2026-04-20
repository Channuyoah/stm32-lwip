/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
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

  // XQ_StartADC (ADC_CHANNEL_DUAL_MODE);
  // XQ_StartADC (ADC_CHANNEL_ADC3);
  
  // 初始化DAC (0-5V范围)
  // XQ_DAC_Init(GP8403_RANGE_10V);

  // 设置初始输出电压
  // XQ_SetDAC(GP8403_CHANNEL_BOTH, 3.1f);

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
  for(;;)
  {
    xqIO_Output = (1ULL << chaser_pin);
    chaser_pin = (chaser_pin + 1) % CHASER_TOTAL;



//     XQ_ABSMove((AxisID)0, 1, 5, 30000, 2450, 5);
// osDelay(500);
// printf ("1*************** \r\n");
// XQ_ABSMove((AxisID)1, 1, 5, 30000, 2450, 5);
// printf ("2*************** \r\n");
// osDelay(1500);
// XQ_ABSMove((AxisID)0, 0, 5, 30000, 2450, 5);
// osDelay(500);
// printf ("3*************** \r\n");
// XQ_ABSMove((AxisID)1, 0, 5, 30000, 2450, 5);
// printf ("4*************** \r\n");
// osDelay(1500);
// printf (" RUNNING*** \r\n");
// printf ("***************************\r\n");

    HAL_GPIO_TogglePin(SYS_RUN_GPIO_Port, SYS_RUN_Pin);
    osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

