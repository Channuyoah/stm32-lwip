#include "xq_reg_event.h"
#include "xq_modbus.h"   // 可读 usRegHoldBuf 等
#include "xq_io.h"
#include "xq_dac.h"
#include "xq_adc.h"
#include "xq_pwm.h"
#include "xq_axis.h"


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

// PWM0 参数变化时调用（1670 或 1671 被写时触发）
static void on_PWM0_Update(uint16_t addr)
{
    (void)addr;  // 不需要关心具体是哪个地址

    // 从保持寄存器读取当前的两个参数
    uint16_t ton_permille = usRegHoldBuf[1670 - MB_HOLD_START_ADDR];   // 占空比千分比
    uint16_t period_us    = usRegHoldBuf[1671 - MB_HOLD_START_ADDR];   // 周期 μs

    // 转换
    float ton = ton_permille / 1000.0f;
    if (ton > 1.0f) ton = 1.0f;

    float T = (period_us > 0) ? (float)period_us : 100.0f;   // 默认 100μs

    // 调用 PWM 设置（连续模式：T_count=0, idle_T 随便填但不生效）
    XQ_setPWM(0, ton, T, 0, 0.0f);

    printf("PWM0: ton=%.3f, T=%.1f us\r\n", ton, T);
}

// PWM1 （用 1672、1673）
static void on_PWM1_Update(uint16_t addr)
{
    uint16_t ton_permille = usRegHoldBuf[1672 - MB_HOLD_START_ADDR];
    uint16_t period_us    = usRegHoldBuf[1673 - MB_HOLD_START_ADDR];

    float ton = ton_permille / 1000.0f;
    if (ton > 1.0f) ton = 1.0f;
    float T = (period_us > 0) ? (float)period_us : 100.0f;

    XQ_setPWM(1, ton, T, 0, 0.0f);
    printf("PWM1: ton=%.3f, T=%.1f us\r\n", ton, T);
}

// 处理轴控制命令（1652-1655）
static void on_axis_execute(uint16_t addr)
{
    // 读取轴号
    int16_t axis_id = (int16_t)usRegHoldBuf[1655 - MB_HOLD_START_ADDR];
    if (axis_id < 0 || axis_id >= AXIS_SUM) {
        printf("Invalid axis ID: %d\r\n", axis_id);
        return;
    }

    // 读取运动模式
    int16_t mode = (int16_t)usRegHoldBuf[1680 - MB_HOLD_START_ADDR];
    if (mode < 1 || mode > 2) {
        printf("Invalid mode: %d (1=ABS, 2=JOG)\r\n", mode);
        return;
    }

    // 读取目标位置（float，1652‑1653）
    float target_pos = GetFloatFromReg(&usRegHoldBuf[1652 - MB_HOLD_START_ADDR]);

    // 读取速度（int16 → float mm/s）
    int16_t speed_raw = (int16_t)usRegHoldBuf[1654 - MB_HOLD_START_ADDR];
    float speed = (float)speed_raw;

    printf("Axis command: id=%d, mode=%d, pos=%.3f, speed=%.1f\r\n",
           axis_id, mode, target_pos, speed);

    if (mode == 1) {
        // ABS 绝对运动，其余参数固定为 30000,2450,5
        XQ_ABSMove((AxisID)axis_id, target_pos, speed, 30000.0f, 2450.0f, 5);
    } else if (mode == 2) {
        // JOG 点动，速度符号控制方向
        XQ_JogMove((AxisID)axis_id, speed, 30000.0f, 2450.0f, 5);
    }
}

static void on_axis_stop(uint16_t addr) {
    // 1298 的低字节是轴号，高字节是停止模式（0=平滑，1=急停）
    uint16_t val = usRegHoldBuf[1298 - MB_HOLD_START_ADDR];
    int16_t axis_id = val & 0xFF;
    uint8_t emg = (val >> 8) & 0x01;   // 0=平滑减速，1=急停

    if (axis_id >= 0 && axis_id < AXIS_SUM) {
        XQ_Stop((AxisID)axis_id, emg, 30000.0f, 2450.0f, 5);
        printf("Axis %d stop (EMG=%d)\r\n", axis_id, emg);
    }
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
    xq_reg_register_handler(1298, on_axis_stop);
    xq_reg_register_handler(1644, on_DO32_Output);
    xq_reg_register_handler(1655, on_axis_execute);// 轴控制命令：写入轴号时触发
    xq_reg_register_handler(1660, on_AO1_Set);
    xq_reg_register_handler(1662, on_AO2_Set);
    // xq_reg_register_handler(1652, on_set_plan_pos);
    // xq_reg_register_handler(1292, on_axis_enable);

    // PWM0：1670 和 1671 任一被写，都触发 on_PWM0_Update
    xq_reg_register_handler(1670, on_PWM0_Update);
    xq_reg_register_handler(1671, on_PWM0_Update);
    // PWM1：1672 和 1673
    xq_reg_register_handler(1672, on_PWM1_Update);
    xq_reg_register_handler(1673, on_PWM1_Update);
}