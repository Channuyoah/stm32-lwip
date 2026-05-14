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

/* ======== 新地址映射（基于功能分组） ======== */

// 轴命令参数区（保持寄存器 1500-1527）
#define REG_AXIS_CMD_BASE      1500
#define REG_AXIS_CMD_STRIDE    4       // 每个轴占4个寄存器
#define REG_AXIS_CMD_OFF_POS   0       // 目标位置(float) 偏移
#define REG_AXIS_CMD_OFF_SPEED 2       // 速度(int16) 偏移
#define REG_AXIS_CMD_OFF_MODE  3       // 运动模式(int16) 偏移

// 轴限位/原点配置区（保持寄存器 1600-1620）
#define REG_AXIS_LIMIT_BASE    1600
#define REG_AXIS_LIMIT_STRIDE  3       // 每个轴占3个寄存器
#define REG_AXIS_LIMIT_OFF_POS 0       // 正限位DI位号
#define REG_AXIS_LIMIT_OFF_NEG 1       // 负限位DI位号
#define REG_AXIS_LIMIT_OFF_HOME 2      // 原点DI位号

// 模拟输出（DAC）区域（保持寄存器 1300-1303）
#define REG_AO_BASE            1300
#define REG_AO1_ADDR           1300    // 模拟输出1 (float, 1300-1301)
#define REG_AO2_ADDR           1302    // 模拟输出2 (float, 1302-1303)

// PWM 参数区域（保持寄存器 1310-1317）
#define REG_PWM_BASE           1310
#define REG_PWM_CH0_TON        1310    // 通道0占空比千分比
#define REG_PWM_CH0_PERIOD     1311    // 通道0周期(us)
#define REG_PWM_CH0_COUNT      1312    // 通道0发送周期数
#define REG_PWM_CH0_IDLE       1313    // 通道0停止时间(us)
#define REG_PWM_CH1_TON        1314    // 通道1占空比千分比
#define REG_PWM_CH1_PERIOD     1315    // 通道1周期(us)
#define REG_PWM_CH1_COUNT      1316    // 通道1发送周期数
#define REG_PWM_CH1_IDLE       1317    // 通道1停止时间(us)

// 数字IO（保持寄存器，沿用原地址）
#define REG_DI32_ADDR          1642    // 32路DI状态（只读，后台刷新）
#define REG_DO32_ADDR          1644    // 32路DO输出（读写）

// 轴状态读取区（输入寄存器 1250-1319）
#define REG_AXIS_STATUS_BASE   1250
#define REG_AXIS_STATUS_STRIDE 10      // 每个轴占10个寄存器
#define REG_AXIS_STATUS_OFF_ID         0   // 轴号
#define REG_AXIS_STATUS_OFF_MOVING     1   // 运动状态
#define REG_AXIS_STATUS_OFF_CURPOS     2   // 当前位置(float)
#define REG_AXIS_STATUS_OFF_TARPOS     4   // 目标位置(float)
#define REG_AXIS_STATUS_OFF_SPEED      6   // 当前速度(int16)
#define REG_AXIS_STATUS_OFF_LIMIT      7   // 限位触发状态
#define REG_AXIS_STATUS_OFF_HOMING     8   // 回零状态
#define REG_AXIS_STATUS_OFF_RESERVED   9   // 预留


// 模拟输入读取区（输入寄存器 1200-1203）
#define REG_AI_BASE            1200
#define REG_AI1_ADDR           1200    // 模拟输入1 (float, 1200-1201)
#define REG_AI2_ADDR           1202    // 模拟输入2 (float, 1202-1203)

/* 工具函数：Modbus 寄存器 <-> float 互转 */
float GetFloatFromReg(const uint16_t *reg);
void SetFloatToReg(uint16_t *reg, float val);

#endif