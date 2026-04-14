/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TCA9535_INPUT_1_Pin GPIO_PIN_2
#define TCA9535_INPUT_1_GPIO_Port GPIOE
#define TCA9535_INPUT_2_Pin GPIO_PIN_3
#define TCA9535_INPUT_2_GPIO_Port GPIOE
#define TCA9535_INPUT_3_Pin GPIO_PIN_4
#define TCA9535_INPUT_3_GPIO_Port GPIOE
#define M2_DIR_Pin GPIO_PIN_6
#define M2_DIR_GPIO_Port GPIOE
#define SYS_RUN_Pin GPIO_PIN_13
#define SYS_RUN_GPIO_Port GPIOC
#define XQ_IN_30_Pin GPIO_PIN_14
#define XQ_IN_30_GPIO_Port GPIOC
#define XQ_IN_29_Pin GPIO_PIN_15
#define XQ_IN_29_GPIO_Port GPIOC
#define M7_DIR_Pin GPIO_PIN_7
#define M7_DIR_GPIO_Port GPIOF
#define M5_DIR_Pin GPIO_PIN_3
#define M5_DIR_GPIO_Port GPIOA
#define ETH_RESET_Pin GPIO_PIN_0
#define ETH_RESET_GPIO_Port GPIOB
#define XQ_IN_27_Pin GPIO_PIN_1
#define XQ_IN_27_GPIO_Port GPIOB
#define M6_DIR_Pin GPIO_PIN_8
#define M6_DIR_GPIO_Port GPIOC
#define RS485_EN_Pin GPIO_PIN_8
#define RS485_EN_GPIO_Port GPIOA
#define XQ_IN_31_Pin GPIO_PIN_11
#define XQ_IN_31_GPIO_Port GPIOA
#define M1_DIR_Pin GPIO_PIN_12
#define M1_DIR_GPIO_Port GPIOA
#define XQ_IN_28_Pin GPIO_PIN_3
#define XQ_IN_28_GPIO_Port GPIOD
#define TCA9535_INPUT_MIXED_1_Pin GPIO_PIN_6
#define TCA9535_INPUT_MIXED_1_GPIO_Port GPIOD
#define XQ_OUT_0_Pin GPIO_PIN_7
#define XQ_OUT_0_GPIO_Port GPIOD
#define XQ_OUT_1_Pin GPIO_PIN_9
#define XQ_OUT_1_GPIO_Port GPIOG
#define XQ_OUT_2_Pin GPIO_PIN_10
#define XQ_OUT_2_GPIO_Port GPIOG
#define XQ_OUT_4_Pin GPIO_PIN_14
#define XQ_OUT_4_GPIO_Port GPIOG
#define XQ_OUT_3_Pin GPIO_PIN_3
#define XQ_OUT_3_GPIO_Port GPIOB
#define M4_DIR_Pin GPIO_PIN_4
#define M4_DIR_GPIO_Port GPIOB
#define M3_DIR_Pin GPIO_PIN_5
#define M3_DIR_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
