#ifndef __XQ_REG_EVENT_H__
#define __XQ_REG_EVENT_H__

#include "FreeRTOS.h"
#include "queue.h"

// 寄存器地址范围
#define REG_ADDR_MIN   1000
#define REG_ADDR_MAX   1711
#define REG_SLOT_COUNT (REG_ADDR_MAX - REG_ADDR_MIN + 1)   // 712

// 处理函数原型（传入地址）
typedef void (*RegEventHandler_t)(uint16_t usAddress);

// 全局跳转表，在 .c 文件中定义
extern RegEventHandler_t xq_reg_handler_table[REG_SLOT_COUNT];

// 全局队列句柄，存储待处理的寄存器地址
extern QueueHandle_t xq_reg_event_queue;

/* 初始化函数（在 main 或 MX_FREERTOS_Init 中调用） */
void xq_reg_event_init(void);

/* 在 Modbus 写回调中调用，通知某地址变化 */
void xq_reg_notify(uint16_t usAddress);

#endif