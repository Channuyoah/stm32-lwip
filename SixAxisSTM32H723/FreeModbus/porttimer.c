/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- Defines ------------------------------------------*/
#include "tim.h"

/* ----------------------- static functions ---------------------------------*/
static void prvvTIMERExpiredISR( void );

/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBPortTimersInit( USHORT usTim1Timerout50us ) {


  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 13749; /* 除以275得到1Mhz，对应1us，计算50us需要多大分频， 275 *  50 = 13750 */
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = usTim1Timerout50us - 1;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(TIM7_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(TIM7_IRQn);

	/* 先清除一下定时器的中断标记,防止使能中断后直接触发中断 */
	__HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE); 
	HAL_TIM_Base_Start_IT(&htim7); 

  return FALSE;
}


inline void
vMBPortTimersEnable(  )
{
  __HAL_TIM_SetCounter (&htim7, 0);
	HAL_TIM_Base_Start_IT(&htim7);
}

inline void
vMBPortTimersDisable(  )
{
	HAL_TIM_Base_Stop_IT(&htim7);
}

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
static void prvvTIMERExpiredISR( void )
{
  ( void )pxMBPortCBTimerExpired(  );
}


void TIM7_IRQHandler(void) {

	if(__HAL_TIM_GET_FLAG(&htim7, TIM_FLAG_UPDATE) == SET) {

    __HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);
		/* 通知modbus 3.5个字符等待时间到 */
    prvvTIMERExpiredISR();
	
  }

}
