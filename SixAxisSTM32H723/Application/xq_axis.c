#include "xq_axis.h"
#include "xq_encoder.h"
#include "cmsis_os.h"

#include "usart.h"
#include "lwip.h"
#include "tim.h"
#include "gpio.h"
#include "string.h"
#include "math.h"

#include "ht_client_task_motor.h"
#include <stdio.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"


#define MAX(a, b) ((a) > (b) ? (a) : (b))

XQ_SetAxisPara axis[AXIS_MAX_SUM];

float_t Prm_AxisVel[AXIS_MAX_SUM];  /* 轴速度 mm/s */
float_t Prm_PrmsPos[AXIS_MAX_SUM]; /* 轴规划位置 mm */

/**
 * @brief 由起始频率和总位移，通过卡尔达诺公式直接求解t1时间
 *
 * @param start_f   起始频率 (Hz)
 * @param jerk      加加速度 (Hz/s²)
 * @param pulse_sum 总位移（脉冲数）
 * @return float_t  t1时间 (s)
 *
 * @note 求解三次方程（5段S曲线对称，取半段位移 position = pulse_sum/2）:
 *
 *         jerk·t³ + 2·start_f·t = position
 *
 *       化为标准压缩三次方程 t³ + p·t + q = 0:
 *         p = 2·start_f / jerk ≥ 0
 *         q = -position / jerk < 0
 *
 *       因 p ≥ 0，判别式恒满足 D = (p/3)³ + (q/2)² ≥ 0，方程有唯一正实根。
 *       由卡尔达诺公式直接得到闭合解：
 *
 *         u = position / (2·j)
 *         v = start_f / j
 *         D = u² + 8·v³/27
 *         t = ∛(u + √D) + ∛(u - √D)
 *
 *       当 start_f > 0 时，√D > u，故第二个 cbrtf 参数为负；
 *       cbrtf() 对负数返回实数立方根（∛(-x) = -∛(x)），计算仍正确。
 *       当 start_f = 0 时，D = u²，√D = u，退化为 t = ∛(2u) = ∛(position/j)。
 */
float_t
xq_axis_get_t1_for_position (uint32_t start_f, uint32_t jerk, uint32_t pulse_sum) {

  uint32_t position = pulse_sum / 2;  /* 5段曲线对称性 */

  if (position == 0) return 0.0f;

  float_t jerk_f = (float_t)jerk;
  float_t u      = (float_t)position / (2.0f * jerk_f);  /* position / (2·j) */
  float_t v      = (float_t)start_f  / jerk_f;           /* start_f / j      */
  float_t D      = u * u + v * v * v * (8.0f / 27.0f);   /* (p/3)³ + (q/2)²  */
  float_t sqD    = sqrtf(D);

  return cbrtf(u + sqD) + cbrtf(u - sqD);
}


/**
 * @brief 计算5段S曲线（T1+T3）的总位移
 * 
 * @param start_f 起始频率 vs (Hz)
 * @param jerk 加加速度 J (Hz/s²)
 * @param t1 T1和T3的时间 (s)
 * @return 总位移（脉冲数）
 * 
 * @note 根据7段S曲线公式推导：
 *       T1: s₁ = vs*t₁ + J*t₁³/6
 *       T3: s₃ = v₁*t₁ + J*t₁³/3
 *       其中 v₁ = vs + J*t₁²/2
 *       
 *       展开T3：
 *       s₃ = (vs + J*t₁²/2)*t₁ + J*t₁³/3
 *          = vs*t₁ + J*t₁³/2 + J*t₁³/3
 *          = vs*t₁ + 5J*t₁³/6
 *       
 *       总位移：
 *       S = s₁ + s₃
 *         = (vs*t₁ + J*t₁³/6) + (vs*t₁ + 5J*t₁³/6)
 *         = 2*vs*t₁ + J*t₁³
 */
uint32_t
xq_axis_get_position_for_t1 (uint32_t start_f, uint32_t jerk, float_t t1) {

  float_t vs = (float_t)start_f;
  float_t J = (float_t)jerk;
  
  /* ✅ 正确公式：S = 2*vs*t₁ + J*t₁³ */
  float_t s_total = 2.0f * vs * t1 + J * powf(t1, 3.0f);
  
  uint32_t position = (uint32_t)s_total;
  
  return position;
}


/**
 * @brief 根据位移，计算t2时间
 * 
 * @param start_f 
 * @param pulse_sum 注意：要乘以时间因子
 * @return float_t 
 */
float_t
xq_axis_get_t2_for_position (uint32_t start_f, uint32_t _jerk, uint32_t _a_max, uint32_t pulse_sum) {

  float_t a_max = (float_t)_a_max;
  float_t jerk = (float_t)_jerk;

  float_t A, B, C, D, sqrt_D;
  float_t t1 = 0.0f;
  float_t t2 = 0.0f;
  float_t f1 = 0.0f;
  float_t s1 = 0.0f;

  /* 方程： (0.5*jerk*t1) * t2^2 + (f1 + a_max*t1) * t2 - (s3 - s1 - f1*t1 - (1.0/3.0) * jerk * pow(t1, 3)) */
  t1 = (a_max / jerk);
  f1 = start_f + 0.5f * jerk * t1 * t1;
  s1 = start_f * t1 + (1.0f/6.0f) * jerk * powf(t1, 3.0f);

  /* 构建二次方程系数 */
  A = 0.5f * jerk * t1;
  B = f1 + a_max * t1;
  C = -((pulse_sum/2.0f) - s1 - f1 * t1 - (1.0f / 3.0f) * jerk * powf(t1, 3.0f));
  
  /* 判别式 */
  D = B * B - 4.0f * A * C;

  if (D < 0.0f) {
    printf("%s Err: no real number solution\n",__func__);
  } else {
    sqrt_D = sqrtf(D);
    /**
     * 正数解： (-B + sqrt_D) / (2 * A)
     * 负数解： (-B - sqrt_D) / (2 * A)
     */
    t2 = (-B + sqrt_D) / (2.0f * A);
  }

  return t2;
}


/**
 * @brief 根据起始和目标速度，得到t1 t2 t3
 * 
 *       第一阶段（加加速）的速度公式：f = start_f + 0.5 * jerk * t * t
 * 
 *       如果不存在匀加速段：（由于对称性可知，加加速和减加速速度的增量是相同的）
 * 
 *                         0.5 * (target_f - start_f) = 0.5 * jerk * t * t 
 * 
 *                          (target_f - start_f) =  jerk * t * t 
 *                           
 *                          因此， t1 = sqrt ((target_f - start_f) / jerk)
 *       
 * @param start_f 起始频率（速度）
 * @param target_f 目标频率（速度）
 * @param t1 加加速阶段所需时间
 * @param t2 匀加速阶段所需时间
 * @param t3 减加速阶段所需时间
 * @return int8_t： 0表示计算成功
 */
int8_t
xq_axis_get_t_for_target_speed(uint32_t start_f, uint32_t target_f, 
                                uint32_t _jerk, uint32_t _a_max,
                                float_t *t1, float_t *t2, float_t *t3) {

  int8_t status = 0;
  float_t a_max = (float_t)_a_max;
  float_t jerk = (float_t)_jerk;
  int32_t delta_f = abs(target_f - start_f);

  if (target_f == start_f) { /* 速度无变化是匀速运动 */
    *t1 = 0.0f;
    *t2 = 0.0f;
    *t3 = 0.0f;
  } else if (delta_f > (a_max * a_max / jerk)) {  /* 判断是否存在匀加速段 */
    *t1 = a_max / jerk;
    *t2 = delta_f / a_max - *t1;
    *t3 = *t1;
  } else { /* 不存在匀加速阶段 */
    *t1 = sqrtf((float_t)delta_f / jerk); /* 0.5 * δf = 0.5 * j * t * t */  
    *t2 = 0.0f;
    *t3 = *t1;
  }

  return status;
}


/**
 * @brief 获取当前电机走完剩余曲线的需要的脉冲数
 *
 * @param id
 * @return int32_t
 */
int32_t
xq_axis_get_remain_pulse_num (AxisID id) {

  int32_t remain_pulses = 0;

  SCurve *iter = axis[id].running_curve;

  if (iter == NULL || axis[id].is_moving == false)
    return 0;

  uint32_t idx = iter->table_current_index + 1U;

  while (iter != NULL) {
    for (uint32_t j = idx; j <= iter->table_end_index; j++) {
      uint32_t st = iter->step_table[j];
      if (st == AXIS_STEP_FOREVER)
        st = 8;

      if (iter->dir == AXIS_DIR_CCW)
        remain_pulses += st;
      else
        remain_pulses -= st;
    }

    if (remain_pulses == AXIS_STEP_FOREVER)
      break;

    iter = iter->next_s;
    if (iter)
      idx = iter->table_current_index; /* 从下条曲线的当前索引开始 */
  }

  return remain_pulses;
}


/**
 * @brief 根据当前速度
 *        1. 创建一个加速度减为零的S曲线，添加新曲线到当前运行曲线的next_s
 *        2. 修改当前曲线的结束索引点，以便切入下一个曲线（加速度减为零的曲线）
 * 
 * @param id 
 * @return SCurve*  返回新创建的曲线（尾部曲线）
 */
SCurve *
xq_curve_acc_reduce_zero (AxisID id, SCurve *c_s) {

  if (axis[id].is_moving == false || c_s == NULL)
    return NULL;

  SCurve *s = NULL;

  /* 存放当前速度 */
  uint32_t f = F2TIME_PARA / c_s->time_table[c_s->table_current_index];

  /* 当前处于S曲线那一个阶段 */
  SCurvePhase phase = xq_curve_get_phase (c_s);

  /**
   * 加速度减为零（加速度减位零，使用的原曲线的加加速度和加速度）
   */

  switch (phase) {

    /**
     * 计算从此刻加速度变为0，需要走多少速度
     * 速度已经增加了多少：当前速度 - 起始速度：  f - c_s->start_f
     * 到达加速度减为0的时候的速度一共变化了多少： 2 * (f - c_s->start_f)
     * 此时的速度为：f + 2 * (f - c_s->start_f) = 2*f - c_s->start_f
     */
    case SCURVE_PHASE_T1: {
      s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, c_s->start_f, (uint32_t)(2*f - c_s->start_f), c_s->jerk, c_s->a_max, c_s->time_factor, true);
      s->table_current_index = s->each_stage_point + 1; /* 此时一定是5段曲线,因为当前点是f，所以加一 */
      c_s->table_end_index = c_s->table_current_index; /* 修改当前曲线的结束索引点 */
      break;
    }

    /**
     * 计算从此刻加速度变为0，需要走多少速度
     * 速度已经增加了多少：当前速度 - 起始速度：  f - c_s->start_f
     * 到达加速度减为0的时候的速度一共变化了多少： f - c_s->start_f + (c_s->f1_max - c_s->start_f)
     * 此时的速度为：f - c_s->start_f + (c_s->f1_max - c_s->start_f) + c_s->start_f
     */
    case SCURVE_PHASE_T2: {
      s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, c_s->start_f, (uint32_t)(f + c_s->f1_max - c_s->start_f), c_s->jerk, c_s->a_max, c_s->time_factor, true);
      s->table_current_index = 2 * s->each_stage_point + 1; /* 此时一定是7段曲线，因为当前点是f，所以加一 */
      c_s->table_end_index = c_s->table_current_index; /* 修改当前曲线的结束索引点 */
      break;
    }

    case SCURVE_PHASE_T3: {/* 需要等待加速度减到零 */
      c_s->table_end_index = c_s->table_max_speed_index;
      c_s->step_table[c_s->table_end_index] = c_s->step_table[c_s->table_end_index - 1]; /* 可以切换下一条曲线 */
      break;
    }
    
    case SCURVE_PHASE_T4: {/* 此时加速度已经是零 */
      c_s->table_end_index = c_s->table_current_index; /* 修改当前曲线的结束索引点 */
      break;
    }

    case SCURVE_PHASE_T5: {
      /**
       * 计算从此刻加速度变为0，需要走多少速度
       * 速度已经减小了多少：最大速度 - 当前速度：  c_s->f3_max - f
       * 加速度变为0，速度差： 2 * (c_s->f3_max - f)
       * 此时的速度为：c_s->f3_max - 2 * (c_s->f3_max - f) 
       */
      s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, c_s->f3_max, (uint32_t)(2 * f - c_s->f3_max), c_s->jerk, c_s->a_max, c_s->time_factor, true);
      s->table_current_index = 3 *s->each_stage_point + 1; /* 此时一定是5段曲线，加一跳过当前f速度点 */
      c_s->table_end_index = c_s->table_current_index; /* 修改当前曲线的结束索引点 */
      break;
    }

    case SCURVE_PHASE_T6: {

      /**
       * 计算从此刻加速度变为0，需要走多少速度
       * 速度已经减小了多少：最大速度 - 当前速度：  c_s->f3_max - f
       * 加速度变为0，需要再减少个δf1：c_s->f1_max - c_s->start_f
       * 此时的速度为：f - c_s->f1_max + c_s->start_f
       */
      s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, c_s->f3_max, (uint32_t)(f - c_s->f1_max + c_s->start_f),c_s->jerk, c_s->a_max, c_s->time_factor, true);
      s->table_current_index = 5*s->each_stage_point + 1; /* 此时一定是7段S曲线，加一跳过当前f速度点 */
      c_s->table_end_index = c_s->table_current_index; /* 修改当前曲线的结束索引点 */
      break;
    }

    case SCURVE_PHASE_T7: { /* 此时需要等待负加速度位零 */
      c_s->table_end_index = c_s->table_num - 1; /* 修改当前曲线的结束索引点 */

      /* 如果当前不是在最后位置 */
      if (c_s->table_current_index != c_s->table_num - 1)
        c_s->step_table[c_s->table_num - 1] = 8; /* 可以切换下一条曲线 */

      break;
    }
    
    default:
      printf ("%s Err unknown phase\n", __func__);
      break;
  }

  /* 将新曲线添加到当前曲线的后面 */
  if (s != NULL) {
    c_s->next_s = s;
  } else {
    s = c_s;
  }

  return s;
}


/**
 * @brief 创建从起始速度到目标速度的S曲线
 * 
 * @note 速度关系与曲线特性：
 *       1. start_f == target_f: 匀速运动，无加减速过程
 *       2. start_f <  target_f: 加速曲线（前半段），从起点运行到最大速度点
 *       3. start_f >  target_f: 减速曲线（后半段），从最大速度点运行到终点
 * 
 * @note 曲线切换控制：
 *       - next_curve = 1: 结束点脉冲数设为8，允许切换到下一条曲线
 *       - next_curve = 0: 结束点脉冲数设为AXIS_STEP_FOREVER，持续运行当前速度
 * 
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param dir 运动方向
 *            - AXIS_DIR_CCW (0): 逆时针/正向
 *            - AXIS_DIR_CW (1):  顺时针/反向
 * @param start_f 起始频率（Hz），运动的初始速度
 *                - 如果小于min_start_f且非匀速，会被自动修正为min_start_f
 * @param target_f 目标频率（Hz），运动的目标速度
 *                 - 不能为0，否则返回NULL
 *                 - 与start_f的大小关系决定是加速还是减速
 * @param jerk 加加速度（Hz/s³），控制加速度的变化率
 *             - 越大则加速度变化越快，运动越"激进"
 *             - 越小则加速度变化越平缓，运动越平滑
 * @param a_max 最大加速度（Hz/s²），限制加速度的峰值
 *              - 决定是否存在匀加速段（T2阶段）
 * @param time_factor 时间因子，控制离散化精度
 *                    - 值越大，曲线点数越多，精度越高，计算量越大
 *                    - 通常设为4~8
 * @param next_curve 曲线链接控制标志
 *                   - 1: 允许自动切换到next_s指向的下一条曲线
 *                   - 0: 在结束点持续运行（点动模式），不自动切换
 * 
 * @return SCurve* 成功返回S曲线指针，失败返回NULL
 *         返回的曲线包含：
 *         - time_table: 每个点的定时器周期值
 *         - step_table: 每个点需要发送的脉冲数
 *         - table_current_index: 当前执行索引（根据加速/减速设置）
 *         - table_end_index: 结束索引（根据加速/减速设置）
 *         - table_max_speed_index: 最大速度点索引
 * 
 * @note 算法流程：
 *       1. 参数检查和修正（target_f不能为0，start_f不能过小）
 *       2. 调用xq_axis_get_t_for_target_speed计算t1、t2、t3
 *       3. 根据t1、t2判断曲线类型（匀速/5段/7段）
 *       4. 创建S曲线对象并填充时间表和步数表
 *       5. 根据加速/减速设置起始和结束索引
 *       6. 设置结束点的脉冲数（8或AXIS_STEP_FOREVER）
 * 
 * @note 特殊处理：
 *       - 如果start_f < target_f（加速）：从索引0开始，到最大速度点结束
 *       - 如果start_f > target_f（减速）：从最大速度点开始，到最后一点结束
 *         并且如果原始start_f < min_start_f，修正最后一点的时间值
 *       - 如果start_f == target_f（匀速）：索引0开始和结束
 */
SCurve *
xq_axis_new_curve_for_target_speed (AxisID id, AxisDir dir, uint32_t start_f, uint32_t target_f, 
                                    uint32_t jerk, uint32_t a_max, uint32_t time_factor, uint8_t next_curve) {

  float_t t1 = 0.0f;
  float_t t2 = 0.0f;
  float_t t3 = 0.0f;
  SCurve *s = NULL;
  SCurveType type;
  uint32_t param_start_f = start_f;

  if (target_f == 0)
    return NULL;

  /**
   * 1. 判断是否小于最小启动速度（非匀速运行下，判断是否小于最小启动速度）
   */
  if (start_f != target_f && start_f < axis[id].min_start_f) {
    start_f = axis[id].min_start_f;
  }


  /**
   * 2. 计算t1、t2、t3，判断使用5段S曲线还是7段S曲线（计算达到目标速度，每个阶段所使用的时间）
   */
  xq_axis_get_t_for_target_speed (start_f, target_f, jerk, a_max, &t1, &t2, &t3);

  if (t1 == 0.0f) { /* 匀速 */
    type = SCURVE_TYPE_UV;
  }else if (t2 == 0.0f) { /* 5段曲线 */
    type = SCURVE_TYPE_FIVE;
  } else { /* 7段曲线 */
    type = SCURVE_TYPE_SEVEN;
  }

  SCurveAttr attr = {
    .type = type,
    .min_start_f = axis[id].min_start_f,
    .start_f = (target_f < start_f) ? target_f : start_f,
    .time_factor = time_factor,
    .jerk = jerk,
    .a_max = a_max,
    .dir = dir,
    .t1 = t1,
    .t2 = t2,
    .t3 = t3
  };

  s = xq_curve_new (&attr);

  /**
   * 3.根据开始和目标速度，计算从哪个索引点开始运行，以及设置目标速度无限脉冲数
   */

  if (start_f < target_f) { /* 加速曲线 */
    
    s->table_current_index = 0;
    s->table_end_index = s->table_max_speed_index; /* 超过结束索引点（可以切换下一个曲线），如果当前点脉冲数不是无限脉冲 */

  } else if (target_f < start_f) { /* 减速曲线 */
    
    s->table_current_index = s->table_max_speed_index;
    s->table_end_index = s->table_num - 1;

    if (param_start_f < axis[id].min_start_f) { /* 修改最后一个点的速度 */
      s->time_table[s->table_num - 1] = F2TIME_PARA / param_start_f;
    }

  } else { /* 匀速 */

    s->table_current_index = 0;
    s->table_end_index = 0;
  
  }

  if (next_curve) {
    s->step_table[s->table_end_index] = 8; /* 可以切换到下一个曲线 */
  } else {
    s->step_table[s->table_end_index] = AXIS_STEP_FOREVER;
  }

  return s;
}


/**
 * @brief 创建达到目标位置的S曲线
 * 
 * @note 1. start_f == max_f 时为匀速运动
 *       2. start_f >  max_f 时会被限制为匀速（max_f作为实际运行速度）
 *       3. 根据位移自动选择5段或7段S曲线（是否存在匀加速段）
 *       4. 自动计算t1、t2、t3时间，确保准确到达目标位置
 * 
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param start_f 起始频率（Hz），通常为最小启动频率
 * @param max_f 最大运行频率（Hz），限制最高速度
 * @param target_position 目标位置（脉冲数），正负值取决于pos_mode
 * @param jerk 加加速度（Hz/s³），控制加速度变化率
 * @param a_max 最大加速度（Hz/s²）
 * @param time_factor 时间因子，用于离散化时间
 * @param pos_mode 位置模式：
 *                 - AXIS_POS_MODE_ABS (0): 绝对位置，相对于原点
 *                 - AXIS_POS_MODE_REL (1): 相对位置，相对于当前位置
 * @return SCurve* 成功返回S曲线指针，失败返回NULL
 *         - 曲线已完成脉冲分配和时间表计算
 *         - table_max_speed_index处设置了匀速段脉冲数
 *         - table_pulse_num记录了总脉冲数
 * 
 * @note 算法流程：
 *       1. 计算相对位移和运动方向
 *       2. 根据max_f计算最大速度下的t1、t2、t3
 *       3. 判断5段曲线位移是否足够（决定是否需要匀加速段）
 *       4. 如果不够，使用牛顿迭代法求解t1
 *       5. 创建S曲线并设置匀速段脉冲数补偿误差
 * 
 * @fixme 已知限制：
 *        1. 没考虑t1+t3+t5+t7走过的位移已超过所需位移的情况
 *        2. 没考虑t1+t2+t3+t5+t6+t7走过的位移已超过所需位移的情况
 *        3. 由于浮点运算后转整数，每个离散点位移会累计小于连续位移
 *        4. 因此基本不会出现不需要t4阶段（匀速阶段）的情况
 *        5. 极短距离（relative_location <= 20）强制使用匀速
 * 
 * @todo: 1. 预先减去10个脉冲的位移，保证max_point_pulse_num不为负数（如果不预先减去，可能会出现max_point_pulse_num < 0的情况）
 */
SCurve *
xq_axis_new_curve_for_target_position (AxisID id, uint32_t start_f, uint32_t max_f, int32_t target_position, 
                                       uint32_t jerk, uint32_t a_max, uint32_t time_factor, AxisPosMode pos_mode) {

  float_t t1 = 0.0f;
  float_t t2 = 0.0f;
  float_t t3 = 0.0f;
  
  float_t max_t1 = 0.0f;
  float_t max_t2 = 0.0f;
  float_t max_t3 = 0.0f;
  
  SCurve *s = NULL;

  SCurveType type;

  AxisDir dir;
  
  /* 计算相对位置 */
  int32_t relative_location;

  if (pos_mode == AXIS_POS_MODE_ABS) { /* 绝对位置 */
    relative_location = abs(target_position - axis[id].position);
    dir = (target_position >= axis[id].position) ? AXIS_DIR_CCW : AXIS_DIR_CW;
  } else { /* 相对位置 */
    relative_location = abs(target_position);
    dir = (target_position >= 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
  }

  if (relative_location == 0)
    return NULL;

  /* 最大速度不能小于起始速度 */
  start_f = (max_f <= start_f) ? max_f : start_f;

  /* 计算最大速度下的t1、t2、t3 */
  if (max_f > start_f)
    xq_axis_get_t_for_target_speed (start_f, max_f, jerk, a_max, &max_t1, &max_t2, &max_t3);

  /* 判断使用5段还是7段曲线 */
  if (max_t1 != 0.0f) {

    /* 计算最大速度下，5段S曲线的位移 */
    uint32_t pos_for_max_t1 = xq_axis_get_position_for_t1 (start_f, jerk, max_t1);
    pos_for_max_t1 = (pos_for_max_t1 / time_factor); /* 除以时间因子 */

    /* 如果5段S曲线的位移大于相对位置，则说明不存在匀加速阶段 */
    if (pos_for_max_t1 > ((relative_location - 10) / 2)) { /* 不存在匀加速阶段 */

      /* 计算t1（减去10以保证max_point_pulse_num不为负数） */
      t1 = xq_axis_get_t1_for_position (start_f, jerk, time_factor * (relative_location - 10));

    } else { /* 存在匀加速阶段 */
      
      t1 = t3 = max_t1;

    }
  }

  if (relative_location <= 20 ||  start_f == max_f) { /* 匀速运动 */

    type = SCURVE_TYPE_UV;

    t1 = 0.0f;
    t2 = 0.0f;
    t3 = 0.0f;

  } else if (max_t2 == 0.0f || t1 < max_t1) { /* 不存在匀加速阶段，5段S曲线 */
    
    type = SCURVE_TYPE_FIVE;

    /* 限制超过最大速度 */
    t1 = (t1 >= max_t1) ? max_t1 : t1;
    t3 = t1;

  } else { /* 存在匀加速阶段，7段S曲线 */

    type = SCURVE_TYPE_SEVEN;

    t1 = max_t1;
    t3 = t1;

    t2 = xq_axis_get_t2_for_position (start_f, jerk, a_max, time_factor * (relative_location - 10));

    /* 比较是否超过最大速度 */
    t2 = (t2 >= max_t2) ? max_t2 : t2;
  }

  SCurveAttr attr = {
    .type = type,
    .min_start_f = axis[id].min_start_f,
    .start_f = start_f,
    .time_factor = time_factor,
    .jerk = jerk,
    .a_max = a_max,
    .dir = dir,
    .t1 = t1,
    .t2 = t2,
    .t3 = t3
  };

  s = xq_curve_new (&attr);

  /* 曲线按 relative_location-10 规划，加回10可保证 max_point_pulse_num >= 0 */
  int32_t max_point_pulse_num = relative_location - 2 * s->all_acc_pulse_num;
  if (max_point_pulse_num < 0) printf ("Err new pos max_point < 0\n");
  max_point_pulse_num = (max_point_pulse_num > 0) ? max_point_pulse_num : 0;
  s->step_table[s->table_max_speed_index] = max_point_pulse_num;

  s->table_pulse_num = relative_location;

  return s;
}


/**
 * @brief 动态修改电机运行中的目标速度（支持方向切换）
 * 
 * @note 核心特性：
 *       1. 实时修改：在电机运行过程中平滑切换到新的目标速度
 *       2. 方向切换：支持反向运动，自动经过零速度过渡
 *       3. 加速度平滑：通过加速度归零策略确保无冲击切换
 *       4. 原子操作：禁止任务切换确保曲线规划的原子性
 *       5. 智能判断：相同速度和方向时直接返回，避免不必要的计算
 * 
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param dir 新的目标方向
 *            - AXIS_DIR_CCW (0): 逆时针/正向
 *            - AXIS_DIR_CW (1):  顺时针/反向
 * @param target_f 新的目标频率（Hz）
 *                 - 0: 停止运动（加速度平滑减到零）
 *                 - >0: 新的运行速度
 * @param jerk 加加速度（Hz/s³），用于重新规划S曲线
 * @param a_max 最大加速度（Hz/s²），用于重新规划S曲线
 * @param time_factor 时间因子，用于重新规划S曲线
 * 
 * @return int8_t 执行状态
 *         - 0:  成功修改目标速度
 *         - -1: 失败（电机未运行）
 * 
 * @note 算法流程（6个核心步骤）：
 * 
 *       【步骤1：前置检查】
 *       - 检查电机是否在运行（未运行返回-1）
 *       - 判断方向和速度是否与当前目标相同（相同则直接返回）
 * 
 *       【步骤2：曲线锁定】
 *       - 禁止RTOS任务切换（确保曲线规划原子性）
 *       - 设置当前点脉冲数为AXIS_STEP_FOREVER（防止中断切换曲线）
 *       - 开放最大速度点和末尾点（允许后续曲线衔接）
 * 
 *       【步骤3：清理旧规划】
 *       - 删除当前曲线后的所有next_s链表（释放内存）
 *       - 仅保留正在执行的当前曲线
 * 
 *       【步骤4：加速度归零】
 *       - 调用xq_curve_acc_reduce_zero()创建减速曲线
 *       - 根据当前所处阶段（T1~T7）计算加速度归零所需的速度变化
 *       - 确保在加速度为零时进行速度/方向切换（避免冲击）
 * 
 *       【步骤5：方向切换处理】
 *       - 如果方向不同或target_f=0：
 *         * 继续减速到min_start_f（最小启动速度）
 *         * 创建减速曲线并添加到曲线链
 *       - 如果方向相同：
 *         * 直接从当前速度切换到新速度
 * 
 *       【步骤6：加速到目标】
 *       - 如果target_f != 0：
 *         * 创建从当前速度到target_f的加速曲线
 *         * 设置为最终曲线（next_curve=false，持续运行）
 *       - 恢复当前点脉冲数，允许曲线链执行
 *       - 恢复RTOS任务切换
 * 
 * @note 使用场景：
 *       1. 点动模式速度调节：用户动态改变点动速度
 *       2. 紧急减速：需要快速降低速度但保持平滑
 *       3. 方向反转：前进改为后退或后退改为前进
 *       4. 速度跟随：根据外部信号实时调整速度
 * 
 * @warning 重要注意事项：
 *          1. 必须在电机运行状态下调用，否则返回-1
 *          2. 函数内部会禁止任务切换，尽量减少调用频率
 *          3. 方向切换时会自动经过零速度，不会突变
 *          4. 加速度归零策略确保平滑过渡，但会增加切换时间
 *          5. 如果当前曲线已接近结束，可能无法完成切换
 * 
 */
int8_t
xq_axis_change_target_speed (AxisID id, AxisDir dir, uint32_t target_f, 
                              uint32_t jerk, uint32_t a_max, uint32_t time_factor) {

  /* ========== 步骤1：前置检查 ========== */
  
  /* 判断当前电机是否运行 */
  if (axis[id].is_moving != true)
    return -1;

  int8_t status = 0;
  double_t f = 0.0;
  SCurve *s = NULL;

  /* 当前电机运行的曲线 */
  SCurve *c_s = axis[id].running_curve;

  /* 最尾部的曲线指针 */
  SCurve *tail_s = NULL;

  /* 当前曲线的目标速度（结束点速度） */
  f = F2TIME_PARA / c_s->time_table[c_s->table_end_index];

  /* 如果方向和速度与当前目标完全相同，且已到达结束点，则无需修改 */                              
  if (dir == (AxisDir)c_s->dir && f == target_f && c_s->table_current_index == c_s->table_end_index) {
    return 0;
  }

  /* ========== 步骤2：曲线锁定（进入临界区）========== */
  
  /* 禁止RTOS任务切换，确保曲线规划的原子性 */
  if (__get_IPSR() == 0)  /* 仅在非中断上下文中禁止任务切换 */
    vTaskSuspendAll();

  /* 锁定当前点：设置为无限脉冲，防止定时器中断切换到下一个点 */
  c_s->step_table[c_s->table_current_index] = AXIS_STEP_FOREVER;

  /* 开放最大速度点：允许后续曲线从该点衔接 */
  if (c_s->table_current_index != c_s->table_max_speed_index)
    c_s->step_table[c_s->table_max_speed_index] = 8;

  /* 开放末尾点：允许曲线链在末尾衔接 */
  if (c_s->table_current_index != c_s->table_num - 1)
    c_s->step_table[c_s->table_num - 1] = 8;

  /* ========== 步骤3：清理旧规划 ========== */
  
  /* 删除当前曲线后面的所有next_s链表，释放内存 */
  xq_curve_free_after (c_s);

  /* ========== 步骤4：加速度归零（平滑过渡的关键）========== */
  
  /* 根据当前阶段（T1~T7）创建加速度减为零的曲线
   * 返回值tail_s指向曲线链的最后一条曲线 */
  tail_s = xq_curve_acc_reduce_zero (id, c_s);

  /* ========== 步骤5：方向切换处理 ========== */
  
  /* 如果需要反向或停止，必须先减速到零（最小启动速度） */
  if (xq_axis_read_dir(id) != dir || target_f == 0) {

    /* 获取加速度归零后的当前速度 */
    f = F2TIME_PARA / tail_s->time_table[tail_s->table_end_index];

    /* 如果当前速度大于最小启动速度，创建减速到min_start_f的曲线 */
    if (f > axis[id].min_start_f) {
      s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, (uint32_t)f, 
                                                axis[id].min_start_f, jerk, a_max, time_factor, true);
      tail_s = xq_curve_add_next (tail_s, s);  /* 添加到曲线链末尾 */
    }
  }

  /* ========== 步骤6：加速到目标速度 ========== */
  
  /* 获取曲线链末尾的当前速度 */
  f = F2TIME_PARA / tail_s->time_table[tail_s->table_end_index];
  
  /* 如果目标速度不为零，创建加速到target_f的曲线 */
  if (target_f != 0) {
    /* next_curve=false：在目标速度处持续运行，不自动切换下一条曲线 */
    s = xq_axis_new_curve_for_target_speed (id, dir, f, target_f, jerk, a_max, time_factor, false);
    tail_s = xq_curve_add_next (tail_s, s);  /* 添加到曲线链末尾 */
  }

  /* ========== 步骤7：解锁并恢复执行 ========== */
  
  /* 恢复当前点脉冲数，允许定时器中断继续执行曲线 */
  c_s->step_table[c_s->table_current_index] = c_s->every_step_pulse;

  /* 允许RTOS任务切换（退出临界区） */
  if (__get_IPSR() == 0)
    xTaskResumeAll();

  return status;
}


/**
 * @brief 动态修改电机运行中的目标位置（支持方向切换和位置追踪）
 * 
 * @note 核心特性：
 *       1. 实时重规划：在电机运行过程中改变目标位置
 *       2. 智能路径：自动判断是否需要反向运动
 *       3. 脉冲精度：通过脉冲补偿确保准确到达目标位置
 *       4. 平滑过渡：通过加速度归零策略确保无冲击切换
 *       5. 原子操作：禁止任务切换确保曲线规划的原子性
 *       6. 位置校正：末尾自动补偿脉冲误差
 * 
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param target_position 新的目标位置（脉冲数）
 *                        - 绝对位置：相对于原点的位置
 *                        - 值的符号不影响，函数内部会自动判断运动方向
 * @param max_f 运动的最大频率（Hz）
 *              - 限制重规划过程中的最高速度
 *              - 如果小于当前速度，会先减速到max_f
 * @param jerk 加加速度（Hz/s³），用于重新规划S曲线
 * @param a_max 最大加速度（Hz/s²），用于重新规划S曲线
 * @param time_factor 时间因子，用于重新规划S曲线
 * 
 * @return int8_t 执行状态
 *         - 0:  成功修改目标位置
 *         - -1: 失败（电机未运行）
 * 
 * @note 算法流程（8个核心步骤）：
 * 
 *       【步骤1：前置检查和初始化】
 *       - 检查电机是否在运行（未运行返回-1）
 *       - 初始化各种临时变量和指针
 * 
 *       【步骤2：曲线锁定】
 *       - 禁止RTOS任务切换（确保曲线规划原子性）
 *       - 设置当前点脉冲数为AXIS_STEP_FOREVER（防止中断切换曲线）
 *       - 开放最大速度点和末尾点（允许后续曲线衔接）
 * 
 *       【步骤3：清理旧规划】
 *       - 删除当前曲线后的所有next_s链表（释放内存）
 *       - 仅保留正在执行的当前曲线
 * 
 *       【步骤4：加速度归零】
 *       - 调用xq_curve_acc_reduce_zero()创建减速曲线
 *       - 根据当前所处阶段（T1~T7）计算加速度归零所需的速度变化
 * 
 *       【步骤5：预估减速脉冲】
 *       - 创建从当前速度到min_start_f的虚拟曲线（most_tail_curve）
 *       - 计算该曲线需要的脉冲数（tail_pulse）
 *       - 用于判断目标位置与当前运动方向的关系
 * 
 *       【步骤6：方向判断和路径规划】
 *       情况A：relative_position > 0（不需要反向）
 *         - 保持当前方向继续运动
 *         - 如果max_f < 当前速度：先减速到max_f
 *         - 否则：创建到target_position的位置曲线
 *         - 最后减速到min_start_f
 *       
 *       情况B：relative_position < 0（需要反向）
 *         - 使用预估曲线，先减速到min_start_f
 *         - 计算剩余脉冲数
 *         - 创建反向运动到target_position的位置曲线
 *       
 *       情况C：relative_position == 0（正好到达）
 *         - 使用预估曲线，减速到目标位置即可
 * 
 *       【步骤7：脉冲补偿】
 *       - 重新计算剩余脉冲数
 *       - 计算位置误差：target_position - (当前位置 + 剩余脉冲)
 *       - 补偿到最后一条曲线的匀速段
 *       - 确保准确到达目标位置
 * 
 *       【步骤8：解锁并恢复执行】
 *       - 恢复当前点脉冲数，允许定时器中断继续执行曲线
 *       - 恢复RTOS任务切换（退出临界区）
 * 
 * @note 使用场景：
 *       1. 轨迹跟随：根据外部传感器实时调整目标位置
 *       2. 位置修正：发现偏差后立即修正目标位置
 *       3. 动态避障：检测到障碍物后改变目标位置
 *       4. 多段运动：在运动过程中切换到新的目标点
 * 
 * @warning 重要注意事项：
 *          1. 必须在电机运行状态下调用，否则返回-1
 *          2. 函数内部会禁止任务切换，尽量减少调用频率
 *          3. 如果需要反向运动，会自动经过零速度过渡
 *          4. 脉冲补偿机制确保精度，但可能导致最后一段速度略有偏差
 *          5. most_tail_curve是临时曲线，用于预估，使用后会被释放
 *          6. 位置校正时有±1的调整，补偿当前点脉冲的影响
 */
int8_t
xq_axis_change_target_position (AxisID id, int32_t target_position, uint32_t max_f, 
                                 uint32_t jerk, uint32_t a_max, uint32_t time_factor) {

  int8_t status = 0;

  /* ========== 步骤1：前置检查和初始化 ========== */
  
  /* 判断当前电机是否运行 */
  if (axis[id].is_moving != true)
    return -1;

  /* 当前电机运行的曲线 */
  SCurve *c_s = axis[id].running_curve;

  /* 曲线链的尾部指针（用于追加新曲线） */
  SCurve *tail_s = axis[id].running_curve;

  /* 临时曲线指针（用于创建新曲线） */
  SCurve *s = NULL;

  /* 剩余需要走的脉冲数 */
  int32_t remaining_pulses = 0;

  /* 存放当前速度（Hz） */
  double_t f = 0.0;

  /* 相对位置（目标与当前的差值） */
  int32_t relative_position = 0;

  /* 虚拟曲线：用于预估减速到min_start_f所需的脉冲数 */
  SCurve *most_tail_curve = NULL;

  /* 剩余未运行曲线的脉冲数（带符号，根据方向确定正负） */
  int32_t tail_pulse = 0;

  /* ========== 步骤2：曲线锁定（进入临界区）========== */
  
  /* 禁止RTOS任务切换，确保曲线规划的原子性 */
  if (__get_IPSR() == 0)
    vTaskSuspendAll();

  /* 锁定当前点：设置为无限脉冲，防止定时器中断切换到下一个点 */
  c_s->step_table[c_s->table_current_index] = AXIS_STEP_FOREVER;

  /* 开放最大速度点：允许后续曲线从该点衔接 */
  if (c_s->table_current_index != c_s->table_max_speed_index)
    c_s->step_table[c_s->table_max_speed_index] = 8;

  /* 开放末尾点：允许曲线链在末尾衔接 */
  if (c_s->table_current_index != c_s->table_num - 1)
    c_s->step_table[c_s->table_num - 1] = 8;

  /* ========== 步骤3：清理旧规划 ========== */
  
  /* 删除当前曲线后面的所有next_s链表，释放内存 */
  xq_curve_free_after (c_s);

  /* ========== 步骤4：加速度归零（平滑过渡的关键）========== */
  
  /* 根据当前阶段（T1~T7）创建加速度减为零的曲线
   * 返回值tail_s指向曲线链的最后一条曲线 */
  tail_s = xq_curve_acc_reduce_zero (id, c_s);

  /* ========== 步骤5：预估减速脉冲（方向判断依据）========== */
  
  /* 获取加速度归零后的当前速度 */
  f = F2TIME_PARA / tail_s->time_table[tail_s->table_end_index];

  /* 创建虚拟曲线：从当前速度f减速到min_start_f
   * 该曲线仅用于预估所需脉冲数，不一定会被使用 */
  most_tail_curve = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, f, 
                                                         axis[id].min_start_f, jerk, a_max, time_factor, true);

  // printf ("most_tail_curve->type = %d, f1_max = %d, f2_max = %d, f3_max = %d\n", most_tail_curve->type, most_tail_curve->f1_max, most_tail_curve->f2_max, most_tail_curve->f3_max);

  // for (int i = 0; i < most_tail_curve->table_num; i++) {
  //   printf ("%d ", most_tail_curve->step_table[i]);
  // }
  // printf ("\n");
  
  /* 计算剩余未运行曲线的脉冲数（带符号）
   * CCW方向为正，CW方向为负 */
  tail_pulse = (c_s->dir == AXIS_DIR_CCW) ? most_tail_curve->all_acc_pulse_num : -most_tail_curve->all_acc_pulse_num;

  /* ========== 步骤6：方向判断和路径规划 ========== */
  
  /* 计算相对位置：目标位置 - 预估停止位置
   * 用于判断是否需要反向运动 */
  if (c_s->dir == AXIS_DIR_CCW) {
    /* 正向运动：目标 - (当前位置 + 继续走的脉冲) */
    relative_position = target_position - (axis[id].position + tail_pulse);
  } else {
    /* 反向运动：(当前位置 + 继续走的脉冲) - 目标 */
    relative_position = (axis[id].position + tail_pulse) - target_position;
  }
  
  /* ========== 情况A：不需要反向（relative_position > 0）========== */
  if (relative_position > 0) {

    /* 转换为带符号的相对位置（考虑运动方向） */
    relative_position = (c_s->dir == AXIS_DIR_CCW) ? relative_position : -relative_position;

    /* 子情况1：当前速度超过允许的最大速度 */
    if (max_f < f) {
      /* 先创建减速到max_f的曲线 */
      s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, (uint32_t)f, max_f, 
                                                jerk, a_max, time_factor, true);
      tail_s = xq_curve_add_next (tail_s, s);

    /* 子情况2：当前速度未超过max_f */
    } else {
      /* 创建到目标位置的位置曲线 */
      s = xq_axis_new_curve_for_target_position (id, f, max_f, relative_position, 
                                                   jerk, a_max, time_factor, AXIS_POS_MODE_REL);
      /* 曲线只执行到最大速度点（不包含减速段） */
      s->table_end_index = s->table_max_speed_index;
      s->step_table[s->table_end_index] = 8;  /* 允许切换到下一条曲线 */
      tail_s = xq_curve_add_next (tail_s, s);
    }

    /* 最后阶段：减速到min_start_f */
    f = F2TIME_PARA / tail_s->time_table[tail_s->table_end_index];
    s = xq_axis_new_curve_for_target_speed (id, (AxisDir)c_s->dir, (uint32_t)f, 
                                              axis[id].min_start_f, jerk, a_max, time_factor, true);
    tail_s = xq_curve_add_next (tail_s, s);

  /* ========== 情况B：需要反向（relative_position < 0）========== */
  } else if (relative_position < 0) {

    /* 使用之前创建的虚拟曲线most_tail_curve
     * 该曲线会让电机减速到min_start_f */
    tail_s = xq_curve_add_next (tail_s, most_tail_curve);
    most_tail_curve = NULL;  /* 标记已使用，防止后面被释放 */

    /* 重新计算剩余脉冲数（包括新添加的减速曲线） */
    remaining_pulses = xq_axis_get_remain_pulse_num (id);
    
    /* 计算反向运动的相对位置 */
    relative_position = target_position - (axis[id].position + remaining_pulses);

    /* 创建反向运动到目标位置的曲线 */
    s = xq_axis_new_curve_for_target_position (id, axis[id].min_start_f, max_f, relative_position, 
                                                 jerk, a_max, time_factor, AXIS_POS_MODE_REL);
    tail_s = xq_curve_add_next (tail_s, s);

  /* ========== 情况C：正好到达（relative_position == 0）========== */
  } else {

    /* 使用虚拟曲线减速，正好到达目标位置 */
    tail_s = xq_curve_add_next (tail_s, most_tail_curve);
    most_tail_curve = NULL;  /* 标记已使用，防止后面被释放 */
  }

  /* 如果虚拟曲线未被使用，释放其内存 */
  if (most_tail_curve != NULL)
    xq_curve_free (most_tail_curve);

  /* ========== 步骤7：脉冲补偿（确保位置精度）========== */
  
  /* 重新计算剩余脉冲数（基于最终的曲线链） */
  remaining_pulses = xq_axis_get_remain_pulse_num (id);

  /* 保存当前电机位置 */
  int32_t motor_pos = axis[id].position;
  
  /* 计算位置误差：目标 - (当前 + 剩余) */
  relative_position = target_position - (motor_pos + remaining_pulses);

  /* 补偿当前点的影响（±1） */
  if (c_s->dir == AXIS_DIR_CCW)
    relative_position -= 1;
  else
    relative_position += 1;

  /* 恢复当前点脉冲数 */
  c_s->step_table[c_s->table_current_index] = c_s->every_step_pulse;

  if (tail_s->dir == AXIS_DIR_CCW) {

    if (relative_position < 0) { /* 新加一条反向曲线 */
      
      s = xq_axis_new_curve_for_target_position (id, axis[id].min_start_f, max_f, relative_position, 
                                                 jerk, a_max, time_factor, AXIS_POS_MODE_REL);
      tail_s = xq_curve_add_next (tail_s, s);

    } else { /* 直接补偿到匀速段或当前点 */
      if (tail_s->table_current_index <= tail_s->table_max_speed_index) {
        /* 如果曲线未执行到匀速段，补偿到匀速段 */
        tail_s->step_table[tail_s->table_max_speed_index] += relative_position;
      } else {
        /* 如果已过匀速段，补偿到当前点 */
        tail_s->step_table[tail_s->table_current_index] += relative_position;
      }
    }
  } else {
    if (relative_position > 0) { /* 新加一条反向曲线 */
      
      s = xq_axis_new_curve_for_target_position (id, axis[id].min_start_f, max_f, relative_position, 
                                                 jerk, a_max, time_factor, AXIS_POS_MODE_REL);
      tail_s = xq_curve_add_next (tail_s, s);

    } else { /* 直接补偿到匀速段或当前点 */
      if (tail_s->table_current_index <= tail_s->table_max_speed_index) {
        /* 如果曲线未执行到匀速段，补偿到匀速段 */
        tail_s->step_table[tail_s->table_max_speed_index] -= relative_position;
      } else {
        /* 如果已过匀速段，补偿到当前点 */
        tail_s->step_table[tail_s->table_current_index] -= relative_position;
      }
    }
  }

  printf ("target_position = %ld\n", target_position);
  
  /* ========== 步骤8：解锁并恢复执行 ========== */
  
  /* 允许RTOS任务切换（退出临界区） */
  if (__get_IPSR() == 0)
    xTaskResumeAll();

  return status;
}


/**
 * @brief 指定相应的S曲线，使得电机运行
 * 
 * @param id 电机ID
 * @param s S曲线
 */
void
xq_axis_start_pwm(AxisID id, SCurve *s) {

  if (s == NULL)
    return;
  
  /* 设置电机方向 */
  xq_axis_set_dir (id, (AxisDir)s->dir);

  axis[id].running_curve = s;

  /* 清除可能残留的补偿状态 */
  axis[id].comp_request = 0;
  axis[id].comp_active = 0;
  axis[id].comp_position_adj = 0;
  axis[id].comp_override = 0;

  /* 如果匀速运动，默认索引就是0不变 */
  if (s->type == SCURVE_TYPE_UV)
    s->table_current_index = 0;

  /* 设置周期和PWM比较值 */
  while (s->table_current_index < s->table_num) {

    if (s->step_table[s->table_current_index]) {

      axis[id].tim->Instance->ARR = s->time_table[s->table_current_index] - 1;

      __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel, ( s->time_table[s->table_current_index] ) >> 1);

      break;
    }

    s->table_current_index++;
  }

  if (s->table_current_index >= s->table_num) {
    printf ("%s Err: no valid step found\n", __func__);
    return;
  }

  /* 插补从轴：用 Bresenham 决定第一个 PWM 周期是否输出脉冲 */
  if (axis[id].intp_is_slave) {
    uint64_t err = (uint64_t)axis[id].intp_gate_error + axis[id].intp_slave_total;
    if (err >= axis[id].intp_master_total) {
      axis[id].intp_gate_error = (uint32_t)(err - axis[id].intp_master_total);
      /* compare 保持 ARR/2，输出脉冲 */
    } else {
      axis[id].intp_gate_error = (uint32_t)err;
      __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel, 0);
    }
  }

  /* 如果定时器没有启动 */
  if (axis[id].is_moving == false) {

    axis[id].is_moving = true;

    /* 清空定时器更新中断 */
    __HAL_TIM_CLEAR_FLAG(axis[id].tim, TIM_FLAG_UPDATE);
    
    __HAL_TIM_ENABLE_IT(axis[id].tim, TIM_IT_UPDATE);

    HAL_TIM_PWM_Start(axis[id].tim, axis[id].channel);
  
  }
}


/**
 * @brief 电机对应的定时器停止PWM输出，释放曲线运行完成信号
 * 
 * @param id 
 */
void 
xq_axis_stop_pwm(AxisID id) {

  axis[id].is_moving = false;
  axis[id].intp_is_slave = 0;
  axis[id].comp_active = 0;
  axis[id].comp_request = 0;
  axis[id].comp_position_adj = 0;
  axis[id].comp_override = 0;

  HAL_TIM_PWM_Stop(axis[id].tim, axis[id].channel);
  
  __HAL_TIM_DISABLE_IT(axis[id].tim, TIM_IT_UPDATE);

  __HAL_TIM_DISABLE(axis[id].tim);

  printf ("Axis %d stopped. Final position: %ld\n", id, axis[id].position);
}


int8_t
xq_axis_comp_inject(AxisID id, int8_t delta) {
  if (axis[id].comp_request != 0)
    return -1;
  axis[id].comp_request = delta;
  return 0;
}


void 
xq_axis_timx_update(AxisID id) {
  
  SCurve *s = axis[id].running_curve;

  /* ════ 非从轴 ARR 补偿阶段 ════ */
  if (axis[id].comp_active) {
    axis[id].comp_count++;

    if (axis[id].comp_delta > 0) {
      /* ADD: 多发 1 脉冲, 共 COMP_WINDOW+1 个溢出 */
      if (axis[id].comp_count < axis[id].comp_total) {
        /* 前 COMP_WINDOW 个溢出：正常脉冲 */
        s->every_step_pulse++;
        if (xq_axis_read_dir(id) == AXIS_DIR_CCW)
          axis[id].position++;
        else
          axis[id].position--;
      } else {
        /* 最后 1 个溢出：补偿脉冲（物理输出但不计 position/step） */
        axis[id].comp_active = 0;
        axis[id].comp_encoder_dev--;
        axis[id].tim->Instance->ARR = s->time_table[s->table_current_index] - 1;
        __HAL_TIM_SET_COMPARE(axis[id].tim, axis[id].channel,
                              s->time_table[s->table_current_index] >> 1);
      }
    } else {
      /* REMOVE: 少发 1 脉冲, 共 COMP_WINDOW-1 个溢出 */
      s->every_step_pulse++;
      if (xq_axis_read_dir(id) == AXIS_DIR_CCW)
        axis[id].position++;
      else
        axis[id].position--;

      if (axis[id].comp_count >= axis[id].comp_total) {
        /* 溢出完毕：追加 1 个幻影步（step+position 推进但无物理脉冲） */
        s->every_step_pulse++;
        if (xq_axis_read_dir(id) == AXIS_DIR_CCW)
          axis[id].position++;
        else
          axis[id].position--;
        axis[id].comp_active = 0;
        axis[id].comp_encoder_dev++;
        axis[id].tim->Instance->ARR = s->time_table[s->table_current_index] - 1;
        __HAL_TIM_SET_COMPARE(axis[id].tim, axis[id].channel,
                              s->time_table[s->table_current_index] >> 1);
      }
    }
    return;
  }

  /* ════ 正常逻辑 ════ */

  /* 每个台阶发送的脉冲数 */
  s->every_step_pulse++;

  if (__HAL_TIM_GET_COMPARE(axis[id].tim, axis[id].channel) != 0) {
    /* 有物理脉冲输出 */
    if (axis[id].comp_override > 0) {
      /* 从轴覆写强制脉冲：不计入 position */
      axis[id].comp_override = 0;
      axis[id].comp_encoder_dev--;
    } else {
      if (xq_axis_read_dir(id) == AXIS_DIR_CCW)
        axis[id].position++;
      else
        axis[id].position--;
    }
  } else {
    /* 无物理脉冲输出 */
    if (axis[id].comp_override < 0) {
      /* 从轴覆写强制跳过：幻影 position 前进 */
      axis[id].comp_override = 0;
      axis[id].comp_encoder_dev++;
      if (xq_axis_read_dir(id) == AXIS_DIR_CCW)
        axis[id].position++;
      else
        axis[id].position--;
    }
  }

  /* 如果走完一个台阶，清空然后进行下一个台阶 （当前台阶非无限大情况下） */
  if (s->step_table[s->table_current_index] != AXIS_STEP_FOREVER && \
      s->every_step_pulse >= s->step_table[s->table_current_index]) {

    s->every_step_pulse = 0;
    s->table_current_index++;

    /* 防止脉冲表中连续为0的情况 */
    while (s->table_current_index < s->table_num) {

      if (s->step_table[s->table_current_index])
        break;

      s->table_current_index++;
    }

    /* 走完了整个S曲线，PWM和中断停止，发送完成信号 */
    if (s->table_current_index > s->table_end_index) {

      /* 释放当前曲线内存 */
      xq_curve_free (s);
      axis[id].running_curve = NULL;

      if (s->next_s == NULL) 
        xq_axis_stop_pwm (id);
      else 
        xq_axis_start_pwm (id, s->next_s);

      return;

    } else {

      /* 设置周期 */
      axis[id].tim->Instance->ARR = s->time_table[s->table_current_index] - 1;

      /* 非从轴：直接设置占空比 */
      if (axis[id].intp_is_slave == 0) {
        __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel, ( s->time_table[s->table_current_index] ) >> 1);
      }

    }

  }

  /* 插补从轴：Bresenham 脉冲门控，决定下一个 PWM 周期是否输出脉冲 */
  if (axis[id].intp_is_slave) {
    uint64_t err = (uint64_t)axis[id].intp_gate_error + axis[id].intp_slave_total;
    if (err >= axis[id].intp_master_total) {
      /* Bresenham 判定：输出脉冲 */
      axis[id].intp_gate_error = (uint32_t)(err - axis[id].intp_master_total);
      if (axis[id].comp_position_adj < 0) {
        /* REMOVE 补偿：强制跳过这个脉冲 */
        __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel, 0);
        axis[id].comp_override = -1;
        axis[id].comp_position_adj++;
      } else {
        __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel,
                               s->time_table[s->table_current_index] >> 1);
      }
    } else {
      /* Bresenham 判定：空转 */
      axis[id].intp_gate_error = (uint32_t)err;
      if (axis[id].comp_position_adj > 0) {
        /* ADD 补偿：强制输出一个脉冲 */
        __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel,
                               s->time_table[s->table_current_index] >> 1);
        axis[id].comp_override = +1;
        axis[id].comp_position_adj--;
      } else {
        __HAL_TIM_SET_COMPARE (axis[id].tim, axis[id].channel, 0);
      }
    }
  }

  /* ════ 补偿请求处理 ════ */
  if (axis[id].comp_request != 0) {

    if (axis[id].intp_is_slave) {
      /* 从轴：通过 Bresenham 门控覆写实现补偿
       * comp_position_adj > 0 → 下一个 Bresenham 空转被强制输出，该脉冲不计 position
       * comp_position_adj < 0 → 下一个 Bresenham 脉冲被强制跳过， position 幻影前进 */
      if (axis[id].comp_request > 0)
        axis[id].comp_position_adj++;
      else
        axis[id].comp_position_adj--;
      axis[id].comp_request = 0;

    } else {
      /* 非从轴：ARR 调节法 */
      uint32_t remaining;
      if (s->step_table[s->table_current_index] == AXIS_STEP_FOREVER)
        remaining = COMP_WINDOW + 1;
      else
        remaining = s->step_table[s->table_current_index] - s->every_step_pulse;

      if (remaining > COMP_WINDOW) {
        int8_t delta = axis[id].comp_request;
        uint16_t cur_arr = s->time_table[s->table_current_index];
        uint32_t new_arr;

        if (delta > 0) {
          /* ADD: N*ARR / (N+1) */
          new_arr = ((uint32_t)COMP_WINDOW * cur_arr) / (COMP_WINDOW + 1);
          if (new_arr < 2) new_arr = 2;
          axis[id].comp_total = COMP_WINDOW + 1;
        } else {
          /* REMOVE: N*ARR / (N-1) */
          new_arr = ((uint32_t)COMP_WINDOW * cur_arr) / (COMP_WINDOW - 1);
          if (new_arr > 65535) {
            axis[id].comp_request = 0;
            return;
          }
          axis[id].comp_total = COMP_WINDOW - 1;
        }

        axis[id].comp_active = 1;
        axis[id].comp_count = 0;
        axis[id].comp_delta = delta;
        axis[id].comp_request = 0;

        axis[id].tim->Instance->ARR = (uint16_t)new_arr - 1;
        __HAL_TIM_SET_COMPARE(axis[id].tim, axis[id].channel, (uint16_t)new_arr >> 1);
      }
    }
  }
}


void 
xq_axis_init(void) {

  for (AxisID i = AXIS_0; i < AXIS_SUM; i++) {

    memset (&axis[i], 0, sizeof(XQ_SetAxisPara));

    axis[i].axis = (AxisID)i;
    axis[i].max_running_f = AXIS_MAX_FRE;
    axis[i].min_start_f = AXIS_START_FRE;
    axis[i].max_acc = AXIS_ACC_MAX;
    axis[i].jerk = AXIS_JERK_MAX;
    axis[i].pitch = AXIS_PITCH;
    axis[i].microsteps = AXIS_MICROSTEPS;

    if (i == AXIS_1) {
      axis[i].tim = &htim2;
      axis[i].channel = TIM_CHANNEL_1;
      
      axis[i].DIR_GPIO_Port = M1_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M1_DIR_Pin;

      HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM2_IRQn);
    
    } else if (i == AXIS_0) {
      axis[i].tim = &htim15;
      axis[i].channel = TIM_CHANNEL_1;
      
      axis[i].DIR_GPIO_Port = M2_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M2_DIR_Pin;

      HAL_NVIC_SetPriority(TIM15_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM15_IRQn);

    } else if (i == AXIS_3) {
      axis[i].tim = &htim16;
      axis[i].channel = TIM_CHANNEL_1;
      
      axis[i].DIR_GPIO_Port = M3_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M3_DIR_Pin;

      HAL_NVIC_SetPriority(TIM16_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM16_IRQn);

    } else if (i == AXIS_2) {
      axis[i].tim = &htim17;
      axis[i].channel = TIM_CHANNEL_1;
      
      axis[i].DIR_GPIO_Port = M4_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M4_DIR_Pin;

      HAL_NVIC_SetPriority(TIM17_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM17_IRQn);

    } else if (i == AXIS_5) {
      axis[i].tim = &htim14;
      axis[i].channel = TIM_CHANNEL_1;
      
      axis[i].DIR_GPIO_Port = M5_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M5_DIR_Pin;

      HAL_NVIC_SetPriority(TIM8_TRG_COM_TIM14_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM8_TRG_COM_TIM14_IRQn);

    } else if (i == AXIS_4) {
      axis[i].tim = &htim8;
      axis[i].channel = TIM_CHANNEL_4;
      
      axis[i].DIR_GPIO_Port = M6_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M6_DIR_Pin;

      HAL_NVIC_SetPriority(TIM8_UP_TIM13_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM8_UP_TIM13_IRQn);
    } else if (i == AXIS_6) {

      axis[i].tim = &htim13;
      axis[i].channel = TIM_CHANNEL_1;
      
      axis[i].DIR_GPIO_Port = M7_DIR_GPIO_Port;
      axis[i].DIR_GPIO_Pin = M7_DIR_Pin;

      HAL_NVIC_SetPriority(TIM8_UP_TIM13_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(TIM8_UP_TIM13_IRQn);
    }
  }
}


/**
 * @brief 获取轴的状态
 * 
 * @param id 
 * @param status 
 * @return int8_t 
 * 
 * @fixme: 未完成，需要定义哪些状态？
 */
int8_t
XQ_GetStatus (AxisID id, int8_t *status) {

  

  return 0;
}


/**
 * @brief 设置轴规划位置
 * @param id: 轴ID (AXIS_0 ~ AXIS_5)
 * @param Prfmm: 规划位置值 (mm)
 * @return 0=成功
 * @note 将
 */
int8_t
XQ_SetPrfmm (AxisID id, float_t Prfmm) {
  

  


  return 0;
}


/**
 * @brief 读取轴规划位置
 * @param id: 轴ID (AXIS_0 ~ AXIS_5)，如果为AXIS_MAX_SUM则读取所有轴
 * @param Prfmm: 输出规划位置数组指针
 * @param num: 要读取的轴数量
 * @return 0=成功, -1=失败
 * @note 1. 如果id为AXIS_MAX_SUM，读取所有轴的规划位置
 *       2. 如果id为具体轴ID，只读取该轴的规划位置（Prfmm数组只需1个元素）
 */
int8_t
XQ_GetPrfmm (AxisID id, float_t *Prfmm, uint8_t num) {
  
  /* 参数检查 */
  if (Prfmm == NULL) {
    printf("Error: Prfmm pointer is NULL\r\n");
    return -1;
  }

  if (num == 0) {
    printf("Error: num is 0\r\n");
    return -1;
  }

  /* 读取所有轴的规划位置 */
  if (id == AXIS_MAX_SUM) {
    uint8_t count = (num > AXIS_SUM) ? AXIS_SUM : num;
    for (uint8_t i = 0; i < count; i++) {
      Prm_PrmsPos[i] = (axis[i].position * axis[i].pitch) / axis[i].microsteps;  /* 实际位置 mm */
      Prfmm[i] = Prm_PrmsPos[AXIS_0 + i];
    }
  } 
  /* 读取单个轴的规划位置 */
  else {
    if (id < AXIS_MAX_SUM) {
      Prm_PrmsPos[id] = (axis[id].position * axis[id].pitch) / axis[id].microsteps;  /* 实际位置 mm */
      Prfmm[0] = Prm_PrmsPos[id];
    } else {
      printf("Error: Invalid axis ID %d\r\n", id);
      return -1;
    }
  }

  return 0;
}


/**
 * @brief JOG点动运动 - 以指定速度持续运动
 * @param id: 轴ID (AXIS_0 ~ AXIS_5)
 * @param speed: 目标速度 (mm/s)，正值为正方向，负值为反方向，0为停止
 * @param jerk: 加加速度 (mm/s³)，控制加速度变化率
 * @param a_max: 最大加速度 (mm/s²)
 * @param time_factor: 时间因子 (插补周期的倍数)
 * @return 0=成功, -1=失败
 * @note 1. 用于手动点动控制，按住按钮加速到目标速度，松开按钮减速停止
 *       2. 如果轴正在运动，会平滑切换到新的目标速度
 *       3. 如果轴静止且speed=0，返回失败
 */
int8_t
XQ_JogMove (AxisID id, float_t speed, float_t jerk, float_t a_max, uint32_t time_factor) {

  XQ_Encoder_Sync_Position(id);
  XQ_Encoder_Notify_External_Move(id);

  int32_t target_f = lroundf(speed * axis[id].microsteps / axis[id].pitch);
  uint32_t jerk_f = abs(lroundf(jerk * axis[id].microsteps / axis[id].pitch));
  uint32_t a_max_f = abs(lroundf(a_max * axis[id].microsteps / axis[id].pitch));

  if (axis[id].is_moving != true && target_f == 0)
    return -1;

  if (axis[id].is_moving == true)  {
    /* 该函数中，加速度减为0后，需要判断目标速度方向  */
    xq_axis_change_target_speed (id, (target_f >= 0 ? AXIS_DIR_CCW : AXIS_DIR_CW), (uint32_t)(abs(target_f)), jerk_f, a_max_f, time_factor);
  } else {
    SCurve *s = xq_axis_new_curve_for_target_speed (id, (target_f >= 0 ? AXIS_DIR_CCW : AXIS_DIR_CW), axis[id].min_start_f,
                                                    (uint32_t)abs(target_f), jerk_f, a_max_f, time_factor, false);
    xq_axis_start_pwm (id, s);
  }

  return 0;
}


/**
 * @brief 绝对位置运动 - 运动到指定的绝对位置
 * @param id: 轴ID (AXIS_0 ~ AXIS_5)
 * @param target_position: 目标绝对位置 (mm)
 * @param max_speed: 最大运动速度 (mm/s)
 * @param jerk: 加加速度 (mm/s³)，控制加速度变化率
 * @param a_max: 最大加速度 (mm/s²)
 * @param time_factor: 时间因子 (插补周期的倍数)
 * @return 0=成功
 * @note 1. 运动到target_position指定的绝对坐标位置
 *       2. 如果轴正在运动，会先平滑减速到0，再规划到新位置的运动
 *       3. 使用S曲线规划，保证运动平滑无冲击
 *       4. 自动计算运动方向（根据当前位置和目标位置）
 */
int8_t
XQ_ABSMove (AxisID id, float_t target_position, float_t max_speed, float_t jerk, float_t a_max, uint32_t time_factor) {

  XQ_Encoder_Sync_Position(id);
  XQ_Encoder_Notify_External_Move(id);

  int32_t target_pos = lroundf(target_position * axis[id].microsteps / axis[id].pitch);
  uint32_t max_speed_f = abs(lroundf(max_speed * axis[id].microsteps / axis[id].pitch));
  uint32_t jerk_f = abs(lroundf(jerk * axis[id].microsteps / axis[id].pitch));
  uint32_t a_max_f = abs(lroundf(a_max * axis[id].microsteps / axis[id].pitch));

  if (axis[id].is_moving == true)  {
    /* 这里的位置是绝对位置，首先会根据当前运动，进行加速度减到零 */
    xq_axis_change_target_position (id, target_pos, max_speed_f, jerk_f, a_max_f, time_factor);
  } else {
    /* 根据相对位置创建S曲线 */
    SCurve *s = xq_axis_new_curve_for_target_position (id, axis[id].min_start_f, max_speed_f, target_pos, jerk_f, a_max_f, time_factor, AXIS_POS_MODE_ABS);
    xq_axis_start_pwm (id, s);
  }
  
  return 0;
}


/**
 * @brief 停止轴运动
 * @param id: 轴ID (AXIS_0 ~ AXIS_5)
 * @param EMG: 紧急停止标志
 *             - 0 (false): 平滑减速停止，使用S曲线减速到0
 *             - 1 (true):  紧急停止，立即切断PWM输出（可能有冲击）
 * @param jerk: 加加速度 (mm/s³)，用于平滑停止时的加速度变化率
 * @param a_max: 最大减速度 (mm/s²)，用于平滑停止
 * @param time_factor: 时间因子 (插补周期的倍数)
 * @return 0=成功
 * @note 1. EMG=false: 调用XQ_JogMove(speed=0)实现平滑减速停止，适合正常停止
 *       2. EMG=true:  直接停止PWM输出，可能造成机械冲击，仅用于紧急情况
 *       3. 平滑停止时，jerk和a_max参数控制减速过程的平滑度
 */
int8_t
XQ_Stop (AxisID id, uint8_t EMG, float_t jerk, float_t a_max, uint32_t time_factor) {

  XQ_Encoder_Notify_External_Move(id);

  if (EMG == true) { /* 急停，直接停止PWM输出 */
    /* 删除后面的曲线 */
    xq_curve_free_after (axis[id].running_curve);
    SCurve *s = axis[id].running_curve;
    s->table_end_index = s->table_current_index;
    s->step_table[s->table_current_index] = s->every_step_pulse;  /* 防止继续运动 */
  } else {
    XQ_JogMove (id, 0.0f, jerk, a_max, time_factor);
  }

  return 0;
}


/**
 * @brief 增量步进运动 - 在指定时间内匀速走指定距离
 * @param id: 轴ID (AXIS_0 ~ AXIS_5)
 * @param Incmm: 增量距离 (mm)，正值为正方向，负值为反方向
 * @param ms: 规定运行时间 (毫秒)
 * @return 0=成功, -1=失败
 * @note 1. 使用匀速运动（无加减速）
 *       2. 根据距离和时间自动计算频率：f = 脉冲数 / 时间(s)
 *       3. 如果轴正在运动，会直接退出
 */
int8_t
XQ_IncMoveStep (AxisID id, float_t Incmm, float_t ms) {

  XQ_Encoder_Sync_Position(id);
  XQ_Encoder_Notify_External_Move(id);

  /* 参数检查 */
  if (ms <= 0.0f || Incmm == 0.0f) {
    printf("Error: Invalid parameters (Incmm=%.3f, ms=%.3f)\r\n", Incmm, ms);
    return -1;
  }

  /* 如果轴正在运动，直接退出 */
  if (axis[id].is_moving == true) {
    return -1;
  }

  /* 转换为脉冲数 */
  int32_t inc_position = lroundf(Incmm * axis[id].microsteps / axis[id].pitch);
  AxisDir dir = (inc_position >= 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;
  uint32_t abs_pulse = abs(inc_position);

  /* 将时间转换为秒 */
  float_t time_s = ms / 1000.0f;

  /**
   * 计算频率（脉冲/秒）
   * 
   * 频率 = 总脉冲数 / 总时间
   * f = pulse / time (Hz)
   */
  float_t frequency = (float_t)abs_pulse / time_s;

  /* 检查频率是否在有效范围内 */
  if (frequency > axis[id].max_running_f) {
    printf("Error: Calculated frequency %.2f Hz exceeds max speed %lu Hz\r\n", 
           frequency, axis[id].max_running_f);
    printf("       (pulse=%lu, time=%.3fs)\r\n", abs_pulse, time_s);
    return -1;
  }

  if (frequency < axis[id].min_start_f) {
    printf("Error: Calculated frequency %.2f Hz is below min start speed %lu Hz\r\n", 
           frequency, axis[id].min_start_f);
    printf("       (pulse=%lu, time=%.3fs)\r\n", abs_pulse, time_s);
    return -1;
  }

  /* 转换为整数频率 */
  uint32_t target_f = (uint32_t)lround(frequency);

  /* 时间因子（默认使用8） */
  uint32_t time_factor = 8;

  /**
   * 创建匀速曲线
   * start_f = target_f，表示匀速运动（无加减速）
   */
  SCurve *s = xq_axis_new_curve_for_target_speed(id, dir, target_f, target_f, 
                                                  axis[id].jerk, axis[id].max_acc, 
                                                  time_factor, false);
  
  if (s == NULL) {
    printf("Error: Failed to create uniform speed curve\r\n");
    return -1;
  }

  /* 设置匀速段的脉冲数 */
  s->step_table[0] = abs_pulse;

  /* 启动PWM运行 */
  xq_axis_start_pwm(id, s);

  return 0;
}


/**
 * @brief 在指定时间内匀速发送指定数量的脉冲（带编码器闭环补偿）
 *
 * @param id       轴编号
 * @param pulses   增量脉冲数（正=CCW, 负=CW, 0=不动）
 * @param time_ms  目标时间（毫秒），必须 > 0
 * @return int8_t  0=成功, -1=参数错误/频率越界, -2=曲线分配失败, -3=轴正在运动
 *
 * @note 不使用 S 曲线，全程匀速。
 *       若轴绑定了编码器且编码器跟随任务已启动，
 *       运行期间将自动通过 comp_inject 进行闭环脉冲补偿。
 */
int8_t
xq_axis_pulse_timed(AxisID id, int32_t pulses, uint32_t time_ms) {

  if (pulses == 0 || time_ms == 0)
    return -1;

  if (axis[id].is_moving)
    return -3;

  /* 通知编码器跟随任务：即将开始外部运动 */
  XQ_Encoder_Sync_Position(id);
  XQ_Encoder_Notify_External_Move(id);

  uint32_t abs_pulse = (pulses > 0) ? (uint32_t)pulses : (uint32_t)(-pulses);
  AxisDir dir = (pulses > 0) ? AXIS_DIR_CCW : AXIS_DIR_CW;

  /* 频率 = 脉冲数 / 时间(秒) */
  float_t time_s = (float_t)time_ms / 1000.0f;
  float_t frequency = (float_t)abs_pulse / time_s;

  /* 检查频率范围：ARR = F2TIME_PARA / frequency，16 位定时器 ARR ∈ [2, 65535] */
  uint32_t arr = (uint32_t)(F2TIME_PARA / frequency);
  if (arr < 2 || arr > 65535)
    return -1;

  uint32_t target_f = (uint32_t)lroundf(frequency);

  /* 创建匀速曲线（start_f == target_f → UV 类型） */
  SCurve *s = xq_axis_new_curve_for_target_speed(id, dir, target_f, target_f,
                                                  axis[id].jerk, axis[id].max_acc,
                                                  8, false);
  if (s == NULL)
    return -2;

  /* 设置固定脉冲数（结束后自动停止） */
  s->step_table[0] = abs_pulse;

  axis[id].run_mode = AXIS_RUN_MODE_POSITION;

  xq_axis_start_pwm(id, s);

  return 0;
}


int8_t
XQ_Home (AxisID id, double_t max_speed, double_t jerk, double_t a_max, uint32_t time_factor) {
  
  XQ_ABSMove (id, 0.0, max_speed, jerk, a_max, time_factor);

  return 0;
}
