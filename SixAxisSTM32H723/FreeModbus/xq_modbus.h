#ifndef __XQ_MODBUS_H__
#define __XQ_MODBUS_H__

#include "main.h"
#include "mb.h"     // FreeModbus 接口

/* 保持寄存器缓冲区（读写） */
#define MB_HOLD_START_ADDR  1000
#define MB_HOLD_SIZE        712     // 1000~1711 共712个16位寄存器
extern uint16_t usRegHoldBuf[MB_HOLD_SIZE];

/* 输入寄存器缓冲区（只读） */
#define MB_INPUT_START_ADDR 1200
#define MB_INPUT_SIZE       512     // 1200~1711 共512个
extern uint16_t usRegInputBuf[MB_INPUT_SIZE];

/* 线圈（位） 1642~1648 使用连续的位地址 */
#define MB_COILS_START_ADDR 1642
#define MB_COILS_SIZE       64      // 足够存放64个线圈

/* 离散输入（位）1642~1643 共32位 */
#define MB_DISC_START_ADDR  1642
#define MB_DISC_SIZE        32

#endif