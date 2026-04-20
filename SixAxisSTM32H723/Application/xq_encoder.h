#ifndef __XQ_ENCODER_H__
#define __XQ_ENCODER_H__

#include "main.h"
#include "tim.h"
#include "lptim.h"

// 编码器轴定义
#define ENCODER_AXIS_COUNT  5

typedef enum {
    ENCODER_AXIS_0 = 0,  // TIM4
    ENCODER_AXIS_1 = 1,  // LPTIM2
    ENCODER_AXIS_2 = 2,  // LPTIM1
    ENCODER_AXIS_3 = 3,  // TIM3
    ENCODER_AXIS_4 = 4,  // TIM23
} XQ_EncoderAxis_t;

/**
 * @brief 编码器与轴的绑定信息
 */
typedef struct {
    uint8_t            bound;              /* 是否已绑定 (0=未绑定, 1=已绑定) */
    uint8_t            axis_id;            /* 绑定的轴号 (AxisID) */
    XQ_EncoderAxis_t   encoder_id;         /* 编码器号 */
    uint32_t           encoder_counts_per_rev; /* 编码器一圈计数值 */
    uint32_t           motor_counts_per_rev;   /* 电机一圈脉冲数（microsteps） */
    uint32_t           max_speed_f;        /* 跟随运动最大速度 (Hz) */
    uint32_t           jerk_f;             /* 跟随运动加加速度 (Hz/s^3) */
    uint32_t           a_max_f;            /* 跟随运动最大加速度 (Hz/s^2) */
    uint32_t           time_factor;        /* 时间因子 */
    /* 位置保持 */
    int32_t            hold_motor_pulse;   /* 需要保持的电机脉冲位置 */
    uint32_t           deadband;           /* 最小允许偏差（脉冲），|偏差| ≤ deadband 时不进行补偿 */
    uint8_t            hold_active;        /* 位置保持是否已激活 (0=未激活, 1=已激活) */
    uint8_t            is_compensating;    /* 当前运动是否由本任务发起的补偿运动 */
    uint8_t            prev_is_moving;     /* 上次轮询时轴的 is_moving 状态（检测停止沿） */
} XQ_EncoderAxisBinding_t;

extern XQ_EncoderAxisBinding_t encoder_bindings[ENCODER_AXIS_COUNT];

/**
 * @brief 将编码器计数值转换为电机脉冲数
 * @param binding 绑定信息指针
 * @param encoder_count 编码器计数值
 * @return 对应的电机脉冲位置
 */
static inline int32_t
encoder_count_to_motor_pulse(const XQ_EncoderAxisBinding_t *binding, int64_t encoder_count) {
  return (int32_t)((encoder_count * (int64_t)binding->motor_counts_per_rev)
                   / (int64_t)binding->encoder_counts_per_rev);
}

// 函数声明
HAL_StatusTypeDef XQ_Encoder_Start(int8_t dir0, int8_t dir1, int8_t dir2, int8_t dir3, int8_t dir4);

// 编码器读取函数（读取脉冲数）
int64_t XQ_Axis_Get_Encoder_Count(XQ_EncoderAxis_t axis);

// 毫米位置设置和读取函数  
void XQ_SetEncmm(XQ_EncoderAxis_t axis, double_t Encmm);
void XQ_GetEncmm(XQ_EncoderAxis_t axis, double_t* Encmm, uint8_t num);

// 中断回调函数
void XQ_Encoder_TIM_Callback(TIM_HandleTypeDef *htim);
void XQ_Encoder_LPTIM_Callback(LPTIM_HandleTypeDef *hlptim);

/**
 * @brief 绑定编码器与电机轴
 * @param axis_id         电机轴号 (AxisID)
 * @param encoder_id      编码器号 (XQ_EncoderAxis_t)
 * @param encoder_counts_per_rev 编码器一圈计数值
 * @param motor_counts_per_rev   电机一圈脉冲数（细分值）
 * @param max_speed_f     跟随运动最大速度 (Hz)
 * @param jerk_f          跟随运动加加速度 (Hz/s^3)
 * @param a_max_f         跟随运动最大加速度 (Hz/s^2)
 * @param time_factor     时间因子
 * @param deadband        最小允许偏差（脉冲），|偏差| ≤ deadband 时不进行补偿
 * @return 0=成功, -1=参数错误, -2=编码器已绑定
 * @note 每个编码器最多绑定一个轴，绑定后不可解绑，不可重新绑定
 */
int8_t XQ_Encoder_Bindto_Axis(uint8_t axis_id, XQ_EncoderAxis_t encoder_id,
                               uint32_t encoder_counts_per_rev, uint32_t motor_counts_per_rev,
                               uint32_t max_speed_f, uint32_t jerk_f, uint32_t a_max_f,
                               uint32_t time_factor, uint32_t deadband);

/**
 * @brief 根据编码器当前位置同步轴的电机脉冲位置
 * @param axis_id 轴编号 (AxisID)
 * @note 运动前调用，将编码器实际位置换算为电机脉冲数并赋值给 axis[].position，
 *       使电机位置与编码器真实位置一致，作为后续运动的起点。
 */
void XQ_Encoder_Sync_Position(uint8_t axis_id);

/**
 * @brief 通知编码器跟随任务：该轴有外部运动指令
 * @param axis_id 轴编号 (AxisID)
 * @note 在 XQ_JogMove / XQ_ABSMove / XQ_Stop / XQ_IncMoveStep 中调用，
 *       使跟随任务暂停位置保持，避免与用户指令冲突。
 */
void XQ_Encoder_Notify_External_Move(uint8_t axis_id);

/**
 * @brief 启动编码器跟随任务
 * @note 创建FreeRTOS任务，周期性检查已绑定编码器的位置，
 *       当轴停止且位置不匹配时自动驱动电机跟随编码器位置
 */
void XQ_Encoder_Follow_Task_Start(void);

/**
 * @brief 绑定手脉到指定轴（手脉使用最后一个编码器）
 * @param axis_id      电机轴号
 * @param ratio_num    倍率分子（编码器增量 × ratio_num / ratio_den = 电机脉冲增量）
 * @param ratio_den    倍率分母
 * @param max_speed_f  跟随运动最大速度 (Hz)
 * @param jerk_f       加加速度 (Hz/s^3)
 * @param a_max_f      最大加速度 (Hz/s^2)
 * @param time_factor  时间因子
 * @return 0=成功, -1=参数错误, -2=已绑定
 */
int8_t XQ_Handwheel_Bind(uint8_t axis_id, int32_t ratio_num, int32_t ratio_den,
                          uint32_t max_speed_f, uint32_t jerk_f, uint32_t a_max_f,
                          uint32_t time_factor);

/**
 * @brief 解除手脉绑定
 * @return 0=成功, -1=未绑定
 */
int8_t XQ_Handwheel_Unbind(void);

#endif // !__XQ_ENCODER_H__
