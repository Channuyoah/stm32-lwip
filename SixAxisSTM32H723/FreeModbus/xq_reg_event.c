#include "xq_reg_event.h"
#include <stdio.h>
#include <stddef.h>

// 跳转表实体
RegEventHandler_t xq_reg_handler_table[REG_SLOT_COUNT] = { NULL };
QueueHandle_t xq_reg_event_queue = NULL;

// 根据地址计算表下标
static inline int addr_to_index(uint16_t addr) {
    if (addr < REG_ADDR_MIN || addr > REG_ADDR_MAX) return -1;
    return (int)(addr - REG_ADDR_MIN);
}

// 注册处理函数
void xq_reg_register_handler(uint16_t addr, RegEventHandler_t handler) {
    int i = addr_to_index(addr);
    if (i >= 0) {
        xq_reg_handler_table[i] = handler;
    }
}

// 初始化队列
void xq_reg_event_init(void) {
    xq_reg_event_queue = xQueueCreate(64, sizeof(uint16_t));
    // 64 是队列深度，可根据突发写入量调整
}

// 通知地址变化（在回调中调用）
void xq_reg_notify(uint16_t usAddress) {
    if (xq_reg_event_queue != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // 因为 Modbus 回调可能运行在中断或任务中，为了通用，这里用任务级 API
        // 如果确认回调总是在任务中，直接用 xQueueSend
        xQueueSend(xq_reg_event_queue, &usAddress, 0);
    }
}