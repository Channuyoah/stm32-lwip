#include "xq_reg_event.h"
#include "xq_modbus.h"   // 可读 usRegHoldBuf 等

// 处理“32路DO输出”（保持寄存器地址 1644）
static void on_DO32_Output(uint16_t addr) {
    uint16_t val = usRegHoldBuf[addr - MB_HOLD_START_ADDR];
    printf("------DO32 changed, addr=%d, value=0x%X\r\n", addr, val);
    // 实际刷新 IO 的代码放在这里
}

// 示例2：处理“设置规划位置”（1652）
static void on_set_plan_pos(uint16_t addr) {
    // addr 1652 和 1653 组成 float，这里简单打印
    float pos = GetFloatFromReg(&usRegHoldBuf[addr - MB_HOLD_START_ADDR]);
    printf("Plan position set: %.3f\r\n", pos);
}

// 示例3：处理“Axis使能”（1292）
static void on_axis_enable(uint16_t addr) {
    uint16_t val = usRegHoldBuf[addr - MB_HOLD_START_ADDR];
    printf("Axis enable cmd, addr=%d, val=%d\r\n", addr, val);
}

// 在初始化时注册所有需要响应的地址
void xq_register_all_handlers(void) {
    xq_reg_register_handler(1645, on_DO32_Output);
    xq_reg_register_handler(1652, on_set_plan_pos);
    xq_reg_register_handler(1292, on_axis_enable);
}