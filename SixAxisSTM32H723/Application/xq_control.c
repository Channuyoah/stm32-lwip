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

// TODO: DI/DO 48位


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

    // 计算当前位置 (mm)
    float cur_pos_mm = (float)axis[axis_id].position * axis[axis_id].pitch / axis[axis_id].microsteps;
    // 移动距离 (mm)
    float distance = fabsf(target_pos - cur_pos_mm);

    // 根据移动距离限制最大速度
    float max_speed = 10.0f;   // 默认上限
    if (distance <= 0.5f) {
        max_speed = 1.0f;
    } else if (distance <= 1.0f) {   // 0.5~1.0 mm 采用 5 mm/s
        max_speed = 5.0f;
    }

    if (speed < 0) {
        speed = 0;
        printf("Axis %d: Warning - speed cannot be negative, set to 0\r\n", axis_id);
    } else if (speed > max_speed) {
        speed = max_speed; // 限制最大速度
        printf("Axis %d: speed limited to %.1f mm/s (distance=%.3f mm)\r\n",
               axis_id, max_speed, distance);
    }

    printf("Axis %d cmd: mode=%d, pos=%.3f, speed=%.1f\r\n",
           axis_id, mode, target_pos, speed);

    // ---------- 限位与回零保护 ----------
    uint8_t pos_bit = axis[axis_id].limit_cfg.limit_pos;
    uint8_t neg_bit = axis[axis_id].limit_cfg.limit_neg;

    // 回零中拒绝任何非停止命令
    if (axis[axis_id].limit_cfg.homing == 1 && mode != 0) {
        printf("Axis %d: homing in progress, command ignored\r\n", axis_id);
        return;
    }

    // 计算意图运动方向
    AxisDir intended_dir;
    if (mode == 2) {   // JOG：速度符号决定方向
        intended_dir = (speed > 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
    } else {          // ABS：目标位置决定方向
        int32_t target_pulse = lroundf(target_pos * axis[axis_id].microsteps / axis[axis_id].pitch);
        intended_dir = (target_pulse >= axis[axis_id].position) ? AXIS_DIR_CCW : AXIS_DIR_CW;
    }

    // 检查正限位
    if (intended_dir == AXIS_DIR_CCW && pos_bit < 48 && (xqIO_Input & (1ULL << pos_bit))) {
        printf("Axis %d: positive limit active, move denied\r\n", axis_id);
        return;
    }
    // 检查负限位
    if (intended_dir == AXIS_DIR_CW && neg_bit < 48 && (xqIO_Input & (1ULL << neg_bit))) {
        printf("Axis %d: negative limit active, move denied\r\n", axis_id);
        return;
    }

    // ---------- 执行运动 ----------
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
        case 3: // 回零（手动触发）
            // 停止当前运动，设置 homing 标志，由 XQ_IO_Refresh_Task 接管后续
            XQ_Stop((AxisID)axis_id, true, 0, 0, 0);
            axis[axis_id].limit_cfg.homing = 1;
            printf("Axis %d: manual home command, homing started\r\n", axis_id);
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
        uint16_t *pStatus = &usRegInputBuf[base - MB_INPUT_START_ADDR];

        // 0: 轴号
        pStatus[REG_AXIS_STATUS_OFF_ID] = i;

        // 1: 运动状态
        pStatus[REG_AXIS_STATUS_OFF_MOVING] = axis[i].is_moving ? 1 : 0;

        // 2-3: 当前位置 (float, mm)
        float cur_pos_mm = (float)axis[i].position * axis[i].pitch / axis[i].microsteps;
        SetFloatToReg(&pStatus[REG_AXIS_STATUS_OFF_CURPOS], cur_pos_mm);

        // 4-5: 目标位置 (float, mm)  -- 从命令区同步
        uint16_t cmd_base = REG_AXIS_CMD_BASE + i * REG_AXIS_CMD_STRIDE;
        float target_pos = GetFloatFromReg(&usRegHoldBuf[cmd_base - MB_HOLD_START_ADDR + REG_AXIS_CMD_OFF_POS]);
        SetFloatToReg(&pStatus[REG_AXIS_STATUS_OFF_TARPOS], target_pos);

        // 6: 当前速度 (int16, mm/s)  -- 暂时用命令区设置的速度
        int16_t speed_cmd = (int16_t)usRegHoldBuf[cmd_base - MB_HOLD_START_ADDR + REG_AXIS_CMD_OFF_SPEED];
        pStatus[REG_AXIS_STATUS_OFF_SPEED] = (uint16_t)speed_cmd;

        // 7: 限位触发状态
        uint16_t limit_state = 0;
        uint8_t pos_bit = axis[i].limit_cfg.limit_pos;
        uint8_t neg_bit = axis[i].limit_cfg.limit_neg;
        uint8_t home_bit = axis[i].limit_cfg.home;
        if (pos_bit < 48 && (xqIO_Input & (1ULL << pos_bit))) limit_state |= 0x01; // 正限位触发
        if (neg_bit < 48 && (xqIO_Input & (1ULL << neg_bit))) limit_state |= 0x02; // 负限位触发
        if (home_bit < 48 && (xqIO_Input & (1ULL << home_bit))) limit_state |= 0x04; // 原点触发
        pStatus[REG_AXIS_STATUS_OFF_LIMIT] = limit_state;

        // 8: 回零状态
        pStatus[REG_AXIS_STATUS_OFF_HOMING] = axis[i].limit_cfg.homing;

        // 9: 预留，填0
        pStatus[REG_AXIS_STATUS_OFF_RESERVED] = 0;
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