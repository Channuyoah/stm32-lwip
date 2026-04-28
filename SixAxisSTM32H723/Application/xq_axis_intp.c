#include "stdlib.h"
#include "math.h"
#include "string.h"
#include "usart.h"
#include "xq_axis_intp.h"
#include "tim.h"


extern SCurve curve_pool[];


/**
 * @brief 根据主轴曲线表，生成从轴曲线表（同 ARR + Bresenham 脉冲门控方案）
 * 
 * @param master_s          主轴S曲线（已生成完毕，脉冲数最多的轴）
 * @param slave_id          从轴ID
 * @param slave_dir         从轴运动方向
 * @param slave_total_pulse 从轴需要走的总脉冲数
 * @return SCurve* 从轴曲线指针，失败返回NULL
 * 
 * @note 算法说明：
 *       从轴直接复制主轴的 time_table（ARR）和 step_table（每台阶溢出次数），
 *       保证两轴定时器以完全相同的频率、完全相同的节拍推进各台阶。
 *       实际脉冲输出由 ISR 中的全局 Bresenham 门控决定：
 *       每次定时器溢出时判断是否输出脉冲（compare=ARR/2）还是空转（compare=0），
 *       从而在 master_total 次溢出中精确输出 slave_total 个脉冲。
 */
static SCurve *
xq_line_intp_create_slave_curve (SCurve *master_s, AxisID slave_id,
                                  AxisDir slave_dir, uint32_t slave_total_pulse) {

  if (master_s == NULL || slave_total_pulse == 0)
    return NULL;

  /* 从曲线内存池中分配 */
  SCurve *s = NULL;
  for (uint32_t i = 0; i < CURVE_POOL_SIZE; i++) {
    if (curve_pool[i].used == false) {
      s = &curve_pool[i];
      break;
    }
  }

  if (s == NULL) {
    printf ("%s: Curve pool exhausted!\n", __func__);
    return NULL;
  }

  memset (s, 0, sizeof(SCurve));
  s->used = true;

  /* 复制主轴曲线的完整表结构和元数据 */
  s->type = master_s->type;
  s->table_num = master_s->table_num;
  s->table_max_speed_index = master_s->table_max_speed_index;
  s->table_end_index = master_s->table_end_index;
  s->table_current_index = master_s->table_current_index;
  s->each_stage_point = master_s->each_stage_point;
  s->dir = slave_dir;
  s->min_start_f = axis[slave_id].min_start_f;

  /* 同 ARR：从轴定时器频率与主轴完全一致 */
  memcpy (s->time_table, master_s->time_table, sizeof(s->time_table));

  /* 同 step_table：从轴每台阶溢出次数与主轴完全一致，保证台阶推进同步 */
  memcpy (s->step_table, master_s->step_table, sizeof(s->step_table));

  s->table_pulse_num = slave_total_pulse;

  return s;
}


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
 * 
 * @note 算法说明：
 *       1. 计算两轴相对位移（脉冲数），确定主轴（脉冲数最多）和从轴
 *       2. 将路径频率按比例分解到主轴
 *       3. start_f == end_f 时使用 xq_axis_new_curve_for_target_position
 *          start_f != end_f 时使用 xq_axis_new_curve_for_target_speed
 *       4. 根据主轴曲线表，按比例生成从轴曲线表
 *       5. 同时启动两轴 PWM 输出
 */
int8_t
xq_line_intp (AxisID axis_a, AxisID axis_b,
              int32_t target_pulse_a, int32_t target_pulse_b,
              uint32_t start_f, uint32_t end_f, uint32_t max_f,
              uint32_t jerk, uint32_t a_max, uint32_t time_factor) {

  /* 任意轴正在运动则返回错误 */
  if (axis[axis_a].is_moving || axis[axis_b].is_moving)
    return -1;

  /* 计算各轴相对位移（脉冲数） */
  int32_t delta_a = target_pulse_a - axis[axis_a].position;
  int32_t delta_b = target_pulse_b - axis[axis_b].position;

  uint32_t abs_a = abs(delta_a);
  uint32_t abs_b = abs(delta_b);

  if (abs_a == 0 && abs_b == 0)
    return -1;

  /* 单轴退化：直接用位置曲线 */
  if (abs_b == 0) {
    SCurve *s = xq_axis_new_curve_for_target_position (axis_a, start_f, max_f,
                    target_pulse_a, jerk, a_max, time_factor, AXIS_POS_MODE_ABS);
    if (s == NULL) return -1;
    xq_axis_start_pwm (axis_a, s);
    return 0;
  }
  if (abs_a == 0) {
    SCurve *s = xq_axis_new_curve_for_target_position (axis_b, start_f, max_f,
                    target_pulse_b, jerk, a_max, time_factor, AXIS_POS_MODE_ABS);
    if (s == NULL) return -1;
    xq_axis_start_pwm (axis_b, s);
    return 0;
  }

  /* 确定主轴（脉冲数最多）和从轴 */
  AxisID master_id, slave_id;
  int32_t master_target;
  uint32_t master_abs, slave_abs;
  AxisDir master_dir, slave_dir;

  if (abs_a >= abs_b) {
    master_id = axis_a;   slave_id = axis_b;
    master_target = target_pulse_a;
    master_abs = abs_a;   slave_abs = abs_b;
    master_dir = (delta_a >= 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
    slave_dir  = (delta_b >= 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
  } else {
    master_id = axis_b;   slave_id = axis_a;
    master_target = target_pulse_b;
    master_abs = abs_b;   slave_abs = abs_a;
    master_dir = (delta_b >= 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
    slave_dir  = (delta_a >= 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
  }

  /* 路径频率分解到主轴：ratio = 主轴脉冲 / 路径脉冲长度 */
  float_t path_pulse_len = sqrtf((float_t)abs_a * abs_a + (float_t)abs_b * abs_b);
  float_t master_ratio = (float_t)master_abs / path_pulse_len;

  uint32_t m_start_f = (uint32_t)lroundf(start_f * master_ratio);
  uint32_t m_end_f   = (uint32_t)lroundf(end_f   * master_ratio);
  uint32_t m_max_f   = (uint32_t)lroundf(max_f   * master_ratio);
  uint32_t m_jerk    = (uint32_t)lroundf(jerk     * master_ratio);
  uint32_t m_a_max   = (uint32_t)lroundf(a_max    * master_ratio);

  /* 频率下限保护 */
  if (m_start_f < axis[master_id].min_start_f) m_start_f = axis[master_id].min_start_f;
  if (m_end_f   < axis[master_id].min_start_f) m_end_f   = axis[master_id].min_start_f;
  if (m_max_f   < m_start_f) m_max_f = m_start_f;
  if (m_max_f   < m_end_f)   m_max_f = m_end_f;
  if (m_a_max == 0) m_a_max = 1;
  if (m_jerk  == 0) m_jerk  = 1;

  SCurve *master_s = NULL;

  if (m_start_f == m_end_f) {

    /**
     * 开始频率 == 结束频率：使用位置曲线（完整 S 曲线：加速→匀速→减速）
     * start_f  = m_start_f（段入口速度分解到主轴）
     * max_f    = m_max_f  （最大运行速度分解到主轴）
     */
    master_s = xq_axis_new_curve_for_target_position (
                  master_id, m_start_f, m_max_f, master_target,
                  m_jerk, m_a_max, time_factor, AXIS_POS_MODE_ABS);

  } else {

    /**
     * 开始频率 != 结束频率：使用速度曲线（从 start_f 过渡到 end_f）
     * 生成后将 AXIS_STEP_FOREVER 替换为实际所需脉冲数
     */
    master_s = xq_axis_new_curve_for_target_speed (
                  master_id, master_dir, m_start_f, m_end_f,
                  m_jerk, m_a_max, time_factor, false);

    if (master_s != NULL) {

      uint32_t computed_pulses = 0;
      int32_t forever_idx = -1;

      for (uint32_t i = master_s->table_current_index; i <= master_s->table_end_index; i++) {
        if (master_s->step_table[i] == AXIS_STEP_FOREVER)
          forever_idx = (int32_t)i;
        else
          computed_pulses += master_s->step_table[i];
      }

      if (forever_idx >= 0) {
        if (m_start_f > m_end_f) {
          /* 减速：先在高速段匀速行驶剩余距离，再减速 */
          master_s->step_table[forever_idx] =
              (forever_idx > 0) ? master_s->step_table[forever_idx - 1] : 1;
          computed_pulses += master_s->step_table[forever_idx];

          if (master_abs > computed_pulses)
            master_s->step_table[master_s->table_current_index] += (master_abs - computed_pulses);
        } else {
          /* 加速：加速后在目标速度匀速行驶剩余距离 */
          uint32_t remain = (master_abs > computed_pulses) ? (master_abs - computed_pulses) : 1;
          master_s->step_table[forever_idx] = remain;
        }
      }

      master_s->table_pulse_num = master_abs;
    }
  }

  if (master_s == NULL)
    return -1;

  /* 根据主轴曲线表，按比例生成从轴曲线表 */
  SCurve *slave_s = xq_line_intp_create_slave_curve (master_s, slave_id, slave_dir, slave_abs);

  if (slave_s == NULL) {
    xq_curve_free (master_s);
    return -1;
  }

  /* 设置运行模式为插补模式 */
  axis[master_id].run_mode = AXIS_RUN_MODE_INTP;
  axis[slave_id].run_mode = AXIS_RUN_MODE_INTP;

  /* 初始化从轴 Bresenham 脉冲门控参数 */
  axis[master_id].intp_is_slave = 0;
  axis[slave_id].intp_is_slave = 1;
  axis[slave_id].intp_slave_total = slave_abs;
  axis[slave_id].intp_master_total = master_abs;
  axis[slave_id].intp_gate_error = 0;

  /* 启动两轴（从轴先启动减少同步误差） */
  xq_axis_start_pwm (slave_id, slave_s);
  xq_axis_start_pwm (master_id, master_s);

  return 0;
}


/**
 * @brief 两轴直线插补运动（物理单位版本）
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
 * 
 * @note 将物理参数（mm/s）转换为脉冲参数（Hz）后调用 xq_line_intp
 */
int8_t
XQ_LineIntp (AxisID axis_a, AxisID axis_b,
             float_t target_pos_a, float_t target_pos_b,
             float_t start_speed, float_t end_speed, float_t max_speed,
             float_t a_max, float_t jerk,
             uint32_t time_factor) {

  /* 目标位置转脉冲（绝对位置） */
  int32_t target_pulse_a = lroundf(target_pos_a * axis[axis_a].microsteps / axis[axis_a].pitch);
  int32_t target_pulse_b = lroundf(target_pos_b * axis[axis_b].microsteps / axis[axis_b].pitch);

  /* 各轴位移（mm、脉冲） */
  float_t cur_mm_a = (float_t)axis[axis_a].position * axis[axis_a].pitch / axis[axis_a].microsteps;
  float_t cur_mm_b = (float_t)axis[axis_b].position * axis[axis_b].pitch / axis[axis_b].microsteps;
  float_t delta_mm_a = target_pos_a - cur_mm_a;
  float_t delta_mm_b = target_pos_b - cur_mm_b;
  float_t path_mm = sqrtf(delta_mm_a * delta_mm_a + delta_mm_b * delta_mm_b);

  if (path_mm < 1e-6f)
    return -1;

  int32_t delta_pulse_a = target_pulse_a - axis[axis_a].position;
  int32_t delta_pulse_b = target_pulse_b - axis[axis_b].position;
  float_t path_pulse = sqrtf((float_t)delta_pulse_a * delta_pulse_a +
                             (float_t)delta_pulse_b * delta_pulse_b);

  /* mm/s → 路径脉冲频率 Hz 的转换系数 */
  float_t pulse_per_mm = path_pulse / path_mm;

  uint32_t start_f = (uint32_t)lroundf(fabsf(start_speed) * pulse_per_mm);
  uint32_t end_f   = (uint32_t)lroundf(fabsf(end_speed)   * pulse_per_mm);
  uint32_t max_f   = (uint32_t)lroundf(fabsf(max_speed)   * pulse_per_mm);
  uint32_t jerk_f  = (uint32_t)lroundf(fabsf(jerk)        * pulse_per_mm);
  uint32_t a_max_f = (uint32_t)lroundf(fabsf(a_max)       * pulse_per_mm);

  return xq_line_intp (axis_a, axis_b, target_pulse_a, target_pulse_b,
                        start_f, end_f, max_f, jerk_f, a_max_f, time_factor);
}
