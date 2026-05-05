#include "xq_reg_event.h"
#include "xq_modbus.h"   // 可读 usRegHoldBuf 等
#include "xq_io.h"
#include "xq_dac.h"
#include "xq_adc.h"


//待处理逻辑：
//数字输出：xqIO_Output     √
//数字输入：xqIO_Input      √
//模拟输出：XQ_SetDAC(GP8403_CHANNEL_BOTH, 3.1f);       √
//模拟输入：XQ_GetADC(uint8_t cn, float_t *Volt, uint16_t num);     √
//PWM输出：XQ_setPWM(0, 0.5, 10.0, 5, 100);


//编码器读数：先不写


//轴控制ABS模式：XQ_ABSMove((AxisID)0, 1, 5, 30000, 2450, 5);
//轴控制JOG模式：XQ_JogMove((AxisID)0, -1, 30000, 2450, 5);
//绑定轴的上下限位，原点。
//读当前位置


// 处理“32路DO输出”（保持寄存器地址 1644）
static void on_DO32_Output(uint16_t addr) {
    uint16_t val = usRegHoldBuf[addr - MB_HOLD_START_ADDR];
    printf("------DO32 changed, addr=%d, value=0x%X\r\n", addr, val);
    // 实际刷新 IO 的代码放在这里
    xqIO_Output = usRegHoldBuf[addr - MB_HOLD_START_ADDR];
}

// 处理“模拟输出AO1”（保持寄存器地址 1660）
static void on_AO1_Set(uint16_t addr) {
    uint16_t offset = addr - MB_HOLD_START_ADDR;
    float voltage = GetFloatFromReg(&usRegHoldBuf[offset]);
    printf("AO1 set to %.3f V\r\n", voltage);
    XQ_SetDAC(GP8403_CHANNEL_0, voltage);
}

// 处理“模拟输出AO2”（保持寄存器地址 1662）
static void on_AO2_Set(uint16_t addr) {
    uint16_t offset = addr - MB_HOLD_START_ADDR;
    float voltage = GetFloatFromReg(&usRegHoldBuf[offset]);
    printf("AO2 set to %.3f V\r\n", voltage);
    XQ_SetDAC(GP8403_CHANNEL_1, voltage);
}

// // 示例2：处理“设置规划位置”（1652）
// static void on_set_plan_pos(uint16_t addr) {
//     // addr 1652 和 1653 组成 float，这里简单打印
//     float pos = GetFloatFromReg(&usRegHoldBuf[addr - MB_HOLD_START_ADDR]);
//     printf("Plan position set: %.3f\r\n", pos);
// }

// // 示例3：处理“Axis使能”（1292）
// static void on_axis_enable(uint16_t addr) {
//     uint16_t val = usRegHoldBuf[addr - MB_HOLD_START_ADDR];
//     printf("Axis enable cmd, addr=%d, val=%d\r\n", addr, val);
// }

// 在初始化时注册所有需要响应的地址
void xq_register_all_handlers(void) {
    xq_reg_register_handler(1644, on_DO32_Output);
    xq_reg_register_handler(1660, on_AO1_Set);
    xq_reg_register_handler(1662, on_AO2_Set);
    // xq_reg_register_handler(1652, on_set_plan_pos);
    // xq_reg_register_handler(1292, on_axis_enable);
}