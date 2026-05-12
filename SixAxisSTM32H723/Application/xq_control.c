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


/* ========== 数字输出 ========== */
static void on_DO32_Output(uint16_t addr)
{
    (void)addr;
    uint16_t val = usRegHoldBuf[REG_DO32_ADDR - MB_HOLD_START_ADDR];
    printf("DO32 changed, value=0x%X\r\n", val);
    xqIO_Output = val;
}

/* ========== 模拟输出 ========== */
static void on_AO1_Set(uint16_t addr)
{
    (void)addr;
    float voltage = GetFloatFromReg(&usRegHoldBuf[REG_AO1_ADDR - MB_HOLD_START_ADDR]);
    printf("AO1 set to %.3f V\r\n", voltage);
    XQ_SetDAC(GP8403_CHANNEL_0, voltage);
}

static void on_AO2_Set(uint16_t addr)
{
    (void)addr;
    float voltage = GetFloatFromReg(&usRegHoldBuf[REG_AO2_ADDR - MB_HOLD_START_ADDR]);
    printf("AO2 set to %.3f V\r\n", voltage);
    XQ_SetDAC(GP8403_CHANNEL_1, voltage);
}

/* ========== PWM 输出 ========== */
static void on_PWM0_Update(uint16_t addr)
{
    (void)addr;
    uint16_t ton_permille = usRegHoldBuf[REG_PWM_CH0_TON    - MB_HOLD_START_ADDR];
    uint16_t period_us    = usRegHoldBuf[REG_PWM_CH0_PERIOD - MB_HOLD_START_ADDR];
    uint16_t burst_count  = usRegHoldBuf[REG_PWM_CH0_COUNT  - MB_HOLD_START_ADDR];
    uint16_t idle_us      = usRegHoldBuf[REG_PWM_CH0_IDLE   - MB_HOLD_START_ADDR];

    float ton = ton_permille / 1000.0f;
    if (ton > 1.0f) ton = 1.0f;
    float T = (period_us > 0) ? (float)period_us : 100.0f;
    float idle_T = (idle_us > 0) ? (float)idle_us : 0.2f;

    XQ_setPWM(0, ton, T, burst_count, idle_T);
    printf("PWM0: ton=%.3f, T=%.1f us, burst=%d, idle=%.1f us\r\n",
           ton, T, burst_count, idle_T);
}

static void on_PWM1_Update(uint16_t addr)
{
    (void)addr;
    uint16_t ton_permille = usRegHoldBuf[REG_PWM_CH1_TON    - MB_HOLD_START_ADDR];
    uint16_t period_us    = usRegHoldBuf[REG_PWM_CH1_PERIOD - MB_HOLD_START_ADDR];
    uint16_t burst_count  = usRegHoldBuf[REG_PWM_CH1_COUNT  - MB_HOLD_START_ADDR];
    uint16_t idle_us      = usRegHoldBuf[REG_PWM_CH1_IDLE   - MB_HOLD_START_ADDR];

    float ton = ton_permille / 1000.0f;
    if (ton > 1.0f) ton = 1.0f;
    float T = (period_us > 0) ? (float)period_us : 100.0f;
    float idle_T = (idle_us > 0) ? (float)idle_us : 0.2f;

    XQ_setPWM(1, ton, T, burst_count, idle_T);
    printf("PWM1: ton=%.3f, T=%.1f us, burst=%d, idle=%.1f us\r\n",
           ton, T, burst_count, idle_T);
}

/* ========== 轴限位配置 ========== */
static void on_limit_cfg_changed(uint16_t addr)
{
    // 通过地址反算轴号和配置项
    int offset = addr - REG_AXIS_LIMIT_BASE;
    int axis_id = offset / REG_AXIS_LIMIT_STRIDE;
    int item = offset % REG_AXIS_LIMIT_STRIDE;

    if (axis_id < 0 || axis_id >= AXIS_SUM) return;

    uint16_t val = usRegHoldBuf[addr - MB_HOLD_START_ADDR];
    uint8_t bit = (val > 47) ? 0xFF : (uint8_t)val;

    switch (item) {
        case REG_AXIS_LIMIT_OFF_POS:
            axis[axis_id].limit_cfg.limit_pos = bit;
            printf("Axis %d: pos limit -> DI_%d\r\n", axis_id, bit);
            break;
        case REG_AXIS_LIMIT_OFF_NEG:
            axis[axis_id].limit_cfg.limit_neg = bit;
            printf("Axis %d: neg limit -> DI_%d\r\n", axis_id, bit);
            break;
        case REG_AXIS_LIMIT_OFF_HOME:
            axis[axis_id].limit_cfg.home = bit;
            printf("Axis %d: home -> DI_%d\r\n", axis_id, bit);
            break;
    }
}

/* ========== 轴运动命令执行 ========== */
static void on_axis_cmd_execute(uint16_t addr)
{
    // 通过地址反算轴号
    int offset = addr - REG_AXIS_CMD_BASE;
    int axis_id = offset / REG_AXIS_CMD_STRIDE;
    int item = offset % REG_AXIS_CMD_STRIDE;

    if (axis_id < 0 || axis_id >= AXIS_SUM) return;

    // 读取该轴的三个参数
    uint16_t base = REG_AXIS_CMD_BASE + axis_id * REG_AXIS_CMD_STRIDE;

    float target_pos = GetFloatFromReg(&usRegHoldBuf[base - MB_HOLD_START_ADDR + REG_AXIS_CMD_OFF_POS]);
    int16_t speed_raw = (int16_t)usRegHoldBuf[base - MB_HOLD_START_ADDR + REG_AXIS_CMD_OFF_SPEED];
    float speed = (float)speed_raw;
    int16_t mode = (int16_t)usRegHoldBuf[base - MB_HOLD_START_ADDR + REG_AXIS_CMD_OFF_MODE];

    printf("Axis %d cmd: mode=%d, pos=%.3f, speed=%.1f\r\n",
           axis_id, mode, target_pos, speed);

    switch (mode) {
        case 0: // 停止
            XQ_Stop((AxisID)axis_id, false, 30000.0f, 2450.0f, 5);
            break;
        case 1: // ABS 绝对运动
            XQ_ABSMove((AxisID)axis_id, target_pos, speed, 30000.0f, 2450.0f, 5);
            break;
        case 2: // JOG 点动
            XQ_JogMove((AxisID)axis_id, speed, 30000.0f, 2450.0f, 5);
            break;
        case 3: // 回零（预留）
            // XQ_Home((AxisID)axis_id, speed, 30000.0f, 2450.0f, 5);
            printf("Axis %d: Home mode not implemented yet\r\n", axis_id);
            break;
        default:
            printf("Axis %d: Invalid mode %d\r\n", axis_id, mode);
            break;
    }
}

/* ========== 轴状态刷新（供周期任务调用） ========== */
void xq_update_axis_status(void)
{
    for (int i = 0; i < AXIS_SUM; i++) {
        uint16_t base = REG_AXIS_STATUS_BASE + i * REG_AXIS_STATUS_STRIDE;

        // 轴号
        usRegInputBuf[base - MB_INPUT_START_ADDR + REG_AXIS_STATUS_OFF_ID] = i;

        // 运动状态
        usRegInputBuf[base - MB_INPUT_START_ADDR + REG_AXIS_STATUS_OFF_MOVING] = axis[i].is_moving;

        // 当前位置 (float, mm)
        float cur_pos = (float)axis[i].position * axis[i].pitch / axis[i].microsteps;
        SetFloatToReg(&usRegInputBuf[base - MB_INPUT_START_ADDR + REG_AXIS_STATUS_OFF_CURPOS], cur_pos);

        // 目标位置 (float, mm) — 需要从命令区同步过来，这里简单留空或读命令区
        // 也可以不填，上位机直接读命令区

        // 当前速度 (int16, mm/s)
        // 需从轴结构体获取实际速度，暂时填0
        usRegInputBuf[base - MB_INPUT_START_ADDR + REG_AXIS_STATUS_OFF_SPEED] = 0;

        // 限位触发状态
        uint16_t limit_state = 0;
        uint8_t pos_bit = axis[i].limit_cfg.limit_pos;
        uint8_t neg_bit = axis[i].limit_cfg.limit_neg;
        uint8_t home_bit = axis[i].limit_cfg.home;
        if (pos_bit < 48 && (xqIO_Input & (1ULL << pos_bit))) limit_state |= 0x01;
        if (neg_bit < 48 && (xqIO_Input & (1ULL << neg_bit))) limit_state |= 0x02;
        if (home_bit < 48 && (xqIO_Input & (1ULL << home_bit))) limit_state |= 0x04;
        usRegInputBuf[base - MB_INPUT_START_ADDR + REG_AXIS_STATUS_OFF_LIMIT] = limit_state;

        // 回零状态
        usRegInputBuf[base - MB_INPUT_START_ADDR + REG_AXIS_STATUS_OFF_HOMING] = axis[i].limit_cfg.homing;
    }
}

/* ========== 注册所有处理函数 ========== */
void xq_register_all_handlers(void)
{
    // ---- 数字输出 ----
    xq_reg_register_handler(REG_DO32_ADDR, on_DO32_Output);

    // ---- 模拟输出 ----
    xq_reg_register_handler(REG_AO1_ADDR, on_AO1_Set);
    xq_reg_register_handler(REG_AO2_ADDR, on_AO2_Set);

    // ---- PWM ----
    for (int addr = REG_PWM_BASE; addr < REG_PWM_BASE + 8; addr++) {
        if (addr <= REG_PWM_CH0_IDLE)
            xq_reg_register_handler(addr, on_PWM0_Update);
        else
            xq_reg_register_handler(addr, on_PWM1_Update);
    }

    // ---- 轴限位配置 ----
    for (int i = 0; i < AXIS_SUM; i++) {
        uint16_t base = REG_AXIS_LIMIT_BASE + i * REG_AXIS_LIMIT_STRIDE;
        xq_reg_register_handler(base + REG_AXIS_LIMIT_OFF_POS,  on_limit_cfg_changed);
        xq_reg_register_handler(base + REG_AXIS_LIMIT_OFF_NEG,  on_limit_cfg_changed);
        xq_reg_register_handler(base + REG_AXIS_LIMIT_OFF_HOME, on_limit_cfg_changed);
    }

    // ---- 轴运动命令 ----
    for (int i = 0; i < AXIS_SUM; i++) {
        uint16_t base = REG_AXIS_CMD_BASE + i * REG_AXIS_CMD_STRIDE;
        xq_reg_register_handler(base + REG_AXIS_CMD_OFF_SPEED, on_axis_cmd_execute); // 速度触发
        xq_reg_register_handler(base + REG_AXIS_CMD_OFF_MODE,  on_axis_cmd_execute); // 模式触发
    }
}