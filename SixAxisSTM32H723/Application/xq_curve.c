#include "xq_curve.h"
#include "xq_axis.h"
#include "usart.h"
#include "string.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdbool.h>


SCurve curve_pool[CURVE_POOL_SIZE];

static void
xq_curve_calc_table(SCurve *s);


/**
 * @brief 创建新的S曲线对象
 * 
 * @param attr S曲线属性参数
 * @return SCurve* 成功返回S曲线对象指针，失败返回NULL
 * 
 * @note 此函数从静态内存池中分配曲线对象，避免动态内存分配
 * @note 如果内存池已满（所有CURVE_POOL_SIZE个对象都在使用），返回NULL
 * @note 分配成功后会自动计算S曲线的时间表和步进表
 */
SCurve *
xq_curve_new (const SCurveAttr *attr) {

  SCurve *s = NULL;

  /* 从曲线内存池中分配新的曲线对象 */
  for (uint32_t i = 0; i < CURVE_POOL_SIZE; i++) {
    if (curve_pool[i].used == false) {
      s = &curve_pool[i];
      break;
    }
  }

  /* 如果内存池已满，返回NULL */
  if (s == NULL) {
    printf ("%s: Curve pool exhausted!\n", __func__);
    return NULL;
  }

  memset (s, 0, sizeof (SCurve));

  /* 拷贝属性数据到曲线对象中 */
  memcpy (s, attr, sizeof (SCurveAttr));


  /* 标记该曲线内存已经被使用 */
  s->used = true;

  s->each_stage_point = EACH_STAGE_POINT_NUM;

  /* 保证T1阶段第一个step_table >= 4：
   * 约束：start_f * (t1/each_stage_point) / time_factor >= 4
   * => each_stage_point <= start_f * t1 / (4 * time_factor)    */
  if (s->type != SCURVE_TYPE_UV && s->time_factor > 0) {
    uint32_t max_pts = (uint32_t)((float_t)s->start_f * s->t1 / (4.0f * (float_t)s->time_factor));
    if (max_pts < 1) max_pts = 1;
    if (s->each_stage_point > max_pts)
      s->each_stage_point = max_pts;
  }

  if (s->type == SCURVE_TYPE_UV){ /* 匀速（无加减速） */

    s->table_num = 1;
    s->table_max_speed_index = 0;
    s->each_stage_point = 0; /* 匀速没有离散点，计算曲线的时候，就不会对其计算 */
    s->time_factor = 0;
    s->jerk = 0;

  } else if (s->type == SCURVE_TYPE_FIVE) { /* 5段S曲线 */
  
    s->table_num = 4 * s->each_stage_point + 1;
    s->table_max_speed_index = 2 * s->each_stage_point;

  } else if (s->type == SCURVE_TYPE_SEVEN) { /* 7段S曲线 */
    
    s->table_num = 6 * s->each_stage_point + 1;
    s->table_max_speed_index = 3 * s->each_stage_point;

  }

  /* 默认情况下，表结束索引等于表大小减一 */
  s->table_end_index = s->table_num - 1;

  /* 计算S曲线 */
  xq_curve_calc_table(s);

  return s;
}



/**
 * @brief 释放S曲线占用的内存空间
 * 
 * @param s 
 */
void
xq_curve_free (SCurve *s) {
  if (s == NULL) return;
  s->used = false;
}


/**
 * @brief 释放S当前曲线以及后续S曲线占用的内存空间 (包括自身)
 * 
 * @param s 
 */
void
xq_curve_free_from (SCurve *s) {

  if (s == NULL) return;

  for (SCurve *p = s; p != NULL; ) {
    SCurve *next = p->next_s;
    p->used = false;
    p = next;
  }

}


/**
 * @brief 释放S曲线后续S曲线占用的内存空间 (不包括自身)
 * 
 * @param s 
 */
void
xq_curve_free_after (SCurve *s) {

  if (s == NULL) return;

  for (SCurve *p = s->next_s; p != NULL; ) {
    SCurve *next = p->next_s;
    p->used = 0;
    p = next;
  }

  s->next_s = NULL;
}



/**
 * @brief 通过当前索引，计算运行到曲线那一阶段
 * 
 * @param s 
 * @return SCurvePhase 
 */
SCurvePhase
xq_curve_get_phase (SCurve *s) {

  SCurvePhase phase;

  if (s->type == SCURVE_TYPE_UV) { /* 匀速 */
    
    phase = SCURVE_PHASE_T4;
  
  } else if (s->type == SCURVE_TYPE_FIVE) { /* 5段S曲线 */

    if (s->table_current_index < s->each_stage_point)
      phase = SCURVE_PHASE_T1;
    else if (s->table_current_index < 2 * s->each_stage_point)
      phase = SCURVE_PHASE_T3;
    else if (s->table_current_index == 2 * s->each_stage_point)
      phase = SCURVE_PHASE_T4;
    else if (s->table_current_index < 3 * s->each_stage_point + 1)
      phase = SCURVE_PHASE_T5;
    else
      phase = SCURVE_PHASE_T7;

  } else if (s->type == SCURVE_TYPE_SEVEN) {  /* 7段S曲线 */

    if (s->table_current_index < s->each_stage_point)
      phase = SCURVE_PHASE_T1;
    else if (s->table_current_index < 2 * s->each_stage_point)
      phase = SCURVE_PHASE_T2;
    else if (s->table_current_index < 3 * s->each_stage_point)
      phase = SCURVE_PHASE_T3;
    else if (s->table_current_index == 3 * s->each_stage_point)
      phase = SCURVE_PHASE_T4;
    else if (s->table_current_index < 4 * s->each_stage_point + 1)
      phase = SCURVE_PHASE_T5;
    else if (s->table_current_index < 5 * s->each_stage_point + 1)
      phase = SCURVE_PHASE_T6;
    else
      phase = SCURVE_PHASE_T7;

  }
 
  return phase;
}


/**
 * @brief 计算S曲线的时间表和步进表
 * 
 * @note s->each_stage_point = 15，计算完成需要3us
 *       s->each_stage_point = 30，计算完成需要6us
 *       
 * @param s 
 */
static void
xq_curve_calc_table(SCurve *s) {

  uint32_t i = 0;
  float_t f = 0.0f; /* 计算当前离散点频率（速度） */
  float_t inv_f = 0.0f; /* 计算当前离散点频率倒数 */
  float_t t1_a_max = 0.0f; /* 计算当前离散点最大加速度 */
  
  /* 预计算常量（每个阶段离散点需要运行多长时间） */
  float_t delta_t1 = s->t1 / (s->each_stage_point);
  float_t delta_t2 = s->t2 / (s->each_stage_point);
  float_t delta_t3 = s->t3 / (s->each_stage_point);

  float_t half_jerk = 0.5f * s->jerk; /* 预计算常量 */

  /* 预计算每个阶段需要运行的实际时间（t1 t2 t3经过时间因子缩放后实际运行时间） */
  float_t delta_t1_inv_tf = delta_t1 / s->time_factor;
  float_t delta_t2_inv_tf = delta_t2 / s->time_factor;
  float_t delta_t3_inv_tf = delta_t3 / s->time_factor;

  uint32_t t1_point_sum = 0;
  uint32_t t2_point_sum = 0;
  
  // uint16_t timer_start = TIM13->CNT;

  t1_a_max = s->jerk * s->t1;
  s->f1_max = s->start_f + half_jerk * s->t1 * s->t1;
  s->f2_max = s->f1_max + t1_a_max * s->t2;
  s->f3_max = s->f2_max + half_jerk * s->t1 * s->t1;

  /**
   * T1阶段：加加速阶段
   * 物理公式： f = f0 + 0.5·j·t²
   * 优化：使用增量法优化二次项计算
   */
  float_t t_offset_sq = 0.0f;
  float_t t_offset_sq_increment = delta_t1 * delta_t1;
  float_t t_offset_sq_step = 2.0f * delta_t1 * delta_t1;
  
  for(i = 0; i < s->each_stage_point; i++) {
    
    /* 直接使用增量计算的平方值 */
    f = s->start_f + half_jerk * t_offset_sq;
    
    /* 使用倒数+乘法代替大数除法 */
    inv_f = 1.0f / f;
    s->time_table[i] = (uint16_t)(F2TIME_PARA * inv_f);
    s->step_table[i] = (uint32_t)(f * delta_t1_inv_tf);
    
    /* 增量更新：(t+Δt)² = t² + 2t·Δt + Δt² */
    t_offset_sq += t_offset_sq_increment; /* Δt² */
    t_offset_sq_increment += t_offset_sq_step; /* 2t·Δt */
  }

  // s->f2_max = s->f1_max = f;

  t1_point_sum = i;

  /**
   * T2阶段：匀加速阶段（7段S曲线）
   */
  if (s->type == SCURVE_TYPE_SEVEN) {
    
    f = s->f1_max;
    float_t f_increment = t1_a_max * delta_t2;
    
    for(i = t1_point_sum; i < t1_point_sum + s->each_stage_point; i++) {
      
      inv_f = 1.0f / f;
      s->time_table[i] = (uint16_t)(F2TIME_PARA * inv_f);
      s->step_table[i] = (uint32_t)(f * delta_t2_inv_tf);

      f += f_increment;
    }
  }

  t2_point_sum = i;
  
  /**
   * T3阶段：减加速阶段
   * 公式： f = f2_max + t1_a_max·t - 0.5·j·t²
   * 优化：使用增量法优化二次项计算
   */
  t_offset_sq_increment = delta_t3 * delta_t3;
  t_offset_sq_step = 2.0f * delta_t3 * delta_t3;
  t_offset_sq = 0.0f;

  /* 线性项也使用增量 */
  float_t linear_increment = t1_a_max * delta_t3;
  float_t linear_term = 0.0f;

  for(i = t2_point_sum; i < t2_point_sum + s->each_stage_point; i++) {
    
    /* 使用增量计算避免重复乘法 */
    f = s->f2_max + linear_term - half_jerk * t_offset_sq;
    
    inv_f = 1.0f / f;
    s->time_table[i] = (uint16_t)(F2TIME_PARA * inv_f);
    s->step_table[i] = (uint32_t)(f * delta_t3_inv_tf);
    
    /* 增量更新 */
    linear_term += linear_increment;

    /* 增量更新：(t+Δt)² = t² + 2t·Δt + Δt² */
    t_offset_sq += t_offset_sq_increment; /* Δt² */
    t_offset_sq_increment += t_offset_sq_step; /* 2t·Δt */
  }
  
  /* 最大速度计算AAR */
  inv_f = 1.0f / s->f3_max;
  s->time_table[s->table_max_speed_index] = (uint32_t)(F2TIME_PARA * inv_f);

  /* 设置最大速度台阶脉冲数 */
  if (s->table_max_speed_index != 0)
    s->step_table[s->table_max_speed_index] = s->step_table[s->table_max_speed_index - 1];
  else
    s->step_table[s->table_max_speed_index] = AXIS_STEP_FOREVER;
  
  /* 对称复制到减速阶段 */
  uint32_t decel_start = s->table_max_speed_index + 1;
  uint32_t accel_points = s->table_max_speed_index;
  
  for(i = 0; i < accel_points; i++) {
    s->time_table[decel_start + i] = s->time_table[accel_points - 1 - i];
    s->step_table[decel_start + i] = s->step_table[accel_points - 1 - i];
  }

  /* 计算总脉冲数 */
  s->all_acc_pulse_num = 0;
  for (i = 0; i < s->table_max_speed_index; i++) {
    s->all_acc_pulse_num += s->step_table[i];
  }

  s->table_pulse_num = 2 * s->all_acc_pulse_num + s->step_table[s->table_max_speed_index];

  // uint16_t timer_end = TIM13->CNT;
  // printf ("Calc SCurve Table time: %d us\n", 
  //         (timer_end >= timer_start ? (timer_end - timer_start) : (0xFFFF - timer_start + timer_end)));

  for (i = 0; i < s->table_num; i++) {
    printf ("Index: %3d, Time: %5d us, Step: %8d\n", i, s->time_table[i], s->step_table[i]);
  }
}
