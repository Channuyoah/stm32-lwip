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

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

#include "usart.h"

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR( void );
static void prvvUARTRxISR( void );

/* ----------------------- Start implementation -----------------------------*/
void
vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable ) {

	if (xRxEnable)
  {
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_RESET);
  }
  else
  {
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_SET);
  }
  
  if (xTxEnable)
  {
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_SET);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_TXE);
  }
  else
  {
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_RESET);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE);
  }

}


BOOL
xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity ) {

  // hlpuart1.Instance = LPUART1;
  // hlpuart1.Init.BaudRate = 115200;
  // // hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  // hlpuart1.Init.StopBits = UART_STOPBITS_1;
  // // hlpuart1.Init.Parity = UART_PARITY_NONE;
  // hlpuart1.Init.Mode = UART_MODE_TX_RX;
  // hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  // hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  // hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  // hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  // hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;

  // switch(eParity) {
  //   case MB_PAR_ODD: /* 奇校验 */
  //     hlpuart1.Init.Parity = UART_PARITY_ODD;
  //     hlpuart1.Init.WordLength = UART_WORDLENGTH_9B;
  //     break;

  //   case MB_PAR_EVEN: /* 偶校验 */
  //     hlpuart1.Init.Parity = UART_PARITY_EVEN;
  //     hlpuart1.Init.WordLength = UART_WORDLENGTH_9B;
  //     break;

  //   default: /* 无校验 */
  //     hlpuart1.Init.Parity = UART_PARITY_NONE;
  //     hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  //     break;
  // }

  // if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  // {
  //   Error_Handler();
  // }
  // if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  // {
  //   Error_Handler();
  // }
  // if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  // {
  //   Error_Handler();
  // }
  // if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  // {
  //   Error_Handler();
  // }

  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);

  return TRUE;
}


BOOL
xMBPortSerialPutByte( CHAR ucByte ) {

  /* 使能发送 */
  HAL_GPIO_WritePin (RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_SET);
  
  huart1.Instance->TDR = (uint8_t)(ucByte & 0xFFU);

  return TRUE;
}


BOOL
xMBPortSerialGetByte( CHAR * pucByte ) {

  /* 使能接收 */
  HAL_GPIO_WritePin (RS485_EN_GPIO_Port, RS485_EN_Pin, GPIO_PIN_RESET);

  *pucByte = huart1.Instance->RDR & (uint8_t)(0x00FFU);

  return TRUE;
}

/* Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call 
 * xMBPortSerialPutByte( ) to send the character.
 */
void prvvUARTTxReadyISR( void )
{
    pxMBFrameCBTransmitterEmpty(  );
}

/* Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
void prvvUARTRxISR( void )
{
    pxMBFrameCBByteReceived(  );
}


void USART1_IRQHandler(void) {

  /* 处理接收中断 */
  if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
  {
    /* 通知Modbus有数据到达 */
    prvvUARTRxISR();
    __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_RXNE);
  }


  /* 处理发送中断 */
  else if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE))
  {

    /* 通知Modbus发送下一个数据 */
    prvvUARTTxReadyISR();
    __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_TXE);
  }

}



