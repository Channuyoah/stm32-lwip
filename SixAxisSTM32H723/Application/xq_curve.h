#ifndef __XQ_CURVE_H__
#define __XQ_CURVE_H__


#include "main.h"

/* 连续的S曲线，每阶段有多少离散点 */
#define EACH_STAGE_POINT_NUM      30

/* 每个曲线离散点数量 */
#define TALBLE_POINT_NUM_MAX   (6 * EACH_STAGE_POINT_NUM + 1)

/* SCurve内存池大小（每个曲线最少可以分配5个 */
#define CURVE_POOL_SIZE (30)

typedef enum {
  SCURVE_TYPE_UV = 0,  /* 匀速，无S曲线 */
  SCURVE_TYPE_FIVE,    /* 5段S曲线 */
  SCURVE_TYPE_SEVEN,   /* 7段S曲线 */
}SCurveType;


/**
 * @brief 7段/5段 S曲线的每个阶段
 * 
 */
typedef enum {
  SCURVE_PHASE_T1,
  SCURVE_PHASE_T2,
  SCURVE_PHASE_T3,
  SCURVE_PHASE_T4,
  SCURVE_PHASE_T5,
  SCURVE_PHASE_T6,
  SCURVE_PHASE_T7
}SCurvePhase;


/**
 * @brief S曲线属性结构体
 * @note 该结构体成员必须与 SCurve 结构体开头部分成员保持一致
 *       对齐问题，否则会导致 memcpy 覆盖错误（一定要整形对齐）
 * 
 */
typedef struct _SCurveAttr SCurveAttr;
struct _SCurveAttr{

  SCurveType type; /* S曲线类型 */

  float_t t1; /* 加加速段时间 */

  float_t t2; /* 匀加速段时间 */

  float_t t3; /* 减加速段时间 */

  uint32_t min_start_f; /* 最小启动频率 */

  uint32_t start_f; /* 开始频率 */

  uint32_t jerk; /* 加加速度 */

  uint32_t a_max; /* 最大加速度 */

  uint32_t time_factor; /* 时间因子 */

  uint32_t dir; /* 0表示CCW，1表示CW */
};


typedef struct _SCurve SCurve;
struct _SCurve {

  /* 开头部分成员必须跟 SCurveAttr 结构体一样 */

  SCurveType type; /* S曲线类型 */

  float_t t1; /* 加加速段时间 */

  float_t t2; /* 匀加速段时间 */

  float_t t3; /* 减加速段时间 */

  uint32_t min_start_f; /* 最小启动频率 */

  uint32_t start_f; /* 开始频率 */

  uint32_t jerk; /* 加加速度 */

  uint32_t a_max; /* 最大加速度 */

  uint32_t time_factor; /* 时间因子 */

  uint32_t dir; /* 0表示CCW，1表示CW */

  /*--------------------------------------*/

  uint8_t used; /* 该曲线是否被使用 */

  SCurve *next_s; /* 仅接着要运行的下一个S曲线 */

  uint32_t have_pulse; /* 已经走了多少脉冲 */

  uint32_t each_stage_point; /* t1, t2, t3, t5, t6, t7每阶段离散点数量 */

  uint16_t time_table[TALBLE_POINT_NUM_MAX]; /* 赋值给TIMx->ARR */

  uint32_t step_table[TALBLE_POINT_NUM_MAX]; /* 每个频率对应要走多少步 */

  uint32_t table_num; /* 表的大小 */

  uint32_t table_end_index; /* 表的结束索引，步进电机超过该索引就停止或者切换到next_s */

  uint32_t table_pulse_num; /* 表中含有的脉冲数量 */
  
  uint32_t table_current_index; /* 表当前索引位置 */

  uint32_t table_max_speed_index; /* 最大速度索引（t4阶段索引点） */

  uint32_t every_step_pulse; /* 时间表里面，记录每一个台阶发送了多少脉冲数 */

  uint32_t all_acc_pulse_num; /* 所有加速阶段总脉冲数（加加速、匀加速、减加速） */
   
  uint32_t f1_max;

  uint32_t f2_max;

  uint32_t f3_max;
};


SCurve *
xq_curve_new (const SCurveAttr *attr);

void
xq_curve_free (SCurve *s);

void
xq_curve_free_from (SCurve *s);

void
xq_curve_free_after (SCurve *s);

SCurvePhase
xq_curve_get_phase (SCurve *s);

#endif // !__xq_curve_H__

