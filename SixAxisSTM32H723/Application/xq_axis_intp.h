#ifndef __HT_MOTOR_INTP_H__
#define __HT_MOTOR_INTP_H__

#include "main.h"
#include "xq_axis.h"


/**
 * @brief 两轴直线插补（脉冲级别）  
 * 
 * @param axis_a          第一个轴ID
 * @param axis_b          第二个轴ID
 * @param target_pulse_a  轴A目标位置（脉冲数，绝对位置）
 * @param target_pulse_b  轴B目标位置（脉冲数，绝对位置）
 * @param start_f         开始频率（Hz，路径频率）
 * @param end_f           结束频率（Hz，路径频率）
 * @param max_f           最大频率（Hz，路径频率）
 * @param jerk            加加速度（Hz/s³，路径加加速度）
 * @param a_max           最大加速度（Hz/s²，路径加速度）
 * @param time_factor     时间因子
 * @return int8_t 0=成功，-1=失败
 */
int8_t
xq_line_intp (AxisID axis_a, AxisID axis_b,
              int32_t target_pulse_a, int32_t target_pulse_b,
              uint32_t start_f, uint32_t end_f, uint32_t max_f,
              uint32_t jerk, uint32_t a_max, uint32_t time_factor);


/**
 * @brief 两轴直线插补运动
 * 
 * @param axis_a       第一个轴ID
 * @param axis_b       第二个轴ID
 * @param target_pos_a 轴A目标位置（mm，绝对位置）
 * @param target_pos_b 轴B目标位置（mm，绝对位置）
 * @param start_speed  开始速度（mm/s，路径速度）
 * @param end_speed    结束速度（mm/s，路径速度）
 * @param max_speed    最大速度（mm/s，路径速度）
 * @param a_max        最大加速度（mm/s²）
 * @param jerk         加加速度（mm/s³）
 * @param time_factor  时间因子
 * @return int8_t 0=成功，-1=失败
 */
int8_t
XQ_LineIntp (AxisID axis_a, AxisID axis_b,
             float_t target_pos_a, float_t target_pos_b,
             float_t start_speed, float_t end_speed, float_t max_speed,
             float_t a_max, float_t jerk,
             uint32_t time_factor);


#endif // !__HT_MOTOR_INTP_H__

