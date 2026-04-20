#include "xq_encoder.h"
#include "xq_axis.h"
#include "usart.h"
#include "lptim.h"
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"


// 编码器定时器句柄数组
static void* encoder_timers[ENCODER_AXIS_COUNT] = {
  &htim4,     // 编码器轴0 - TIM4
  &hlptim2,   // 编码器轴1 - LPTIM2
  &hlptim1,   // 编码器轴2 - LPTIM1
  &htim3,     // 编码器轴3 - TIM3
  &htim23,    // 编码器轴4 - TIM23
};

int32_t overflow_count[ENCODER_AXIS_COUNT] = {0};  // 溢出次数（正向+1，反向-1）
int8_t encoder_direction[ENCODER_AXIS_COUNT] = {1, 1, 1, 1, 1};  // 编码器方向（1=正向，-1=反向）
double_t encoder_pulse_to_mm[ENCODER_AXIS_COUNT] = {1.0, 1.0, 1.0, 1.0, 1.0};  // 编码器脉冲转amm
double_t encoder_mm_to_pulse[ENCODER_AXIS_COUNT] = {1.0, 1.0, 1.0, 1.0, 1.0};  // mm转换编码器脉冲

/* 编码器跟随任务句柄 */
static TaskHandle_t encoder_follow_task_handle = NULL;

// LPTIM2虚拟计数偏移量
static int64_t lptim2_virtual_offset = 0;
// LPTIM1虚拟计数偏移量
static int64_t lptim1_virtual_offset = 0;

/**
 * @brief 启动所有编码器（使能更新中断）
 * @param dir0: 编码器轴0方向（1=正向，-1=反向）
 * @param dir1: 编码器轴1方向（1=正向，-1=反向）
 * @param dir2: 编码器轴2方向（1=正向，-1=反向）
 * @param dir3: 编码器轴3方向（1=正向，-1=反向）
 */
HAL_StatusTypeDef XQ_Encoder_Start(int8_t dir0, int8_t dir1, int8_t dir2, int8_t dir3, int8_t dir4)  {
  
  // 设置编码器方向
  encoder_direction[0] = (dir0 >= 0) ? 1 : -1;
  encoder_direction[1] = (dir1 >= 0) ? 1 : -1;
  encoder_direction[2] = (dir2 >= 0) ? 1 : -1;
  encoder_direction[3] = (dir3 >= 0) ? 1 : -1;
  encoder_direction[4] = (dir4 >= 0) ? 1 : -1;

  HAL_NVIC_SetPriority(LPTIM2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(LPTIM2_IRQn);

  HAL_NVIC_SetPriority(LPTIM1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(LPTIM1_IRQn);

  HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);

  HAL_NVIC_SetPriority(TIM4_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);

  HAL_NVIC_SetPriority(TIM23_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM23_IRQn);
  
  // 启动编码器
  HAL_LPTIM_Encoder_Start(&hlptim2, 60000 - 1);
  HAL_LPTIM_Encoder_Start(&hlptim1, 60000 - 1);
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim23, TIM_CHANNEL_ALL);
  
  // 清除更新中断标志
  __HAL_TIM_CLEAR_IT(&htim3, TIM_IT_UPDATE);
  __HAL_TIM_CLEAR_IT(&htim4, TIM_IT_UPDATE);
  __HAL_TIM_CLEAR_IT(&htim23, TIM_IT_UPDATE);

  // 手动使能更新中断（溢出中断）
  __HAL_LPTIM_ENABLE_IT(&hlptim2, LPTIM_IT_ARRM);
  __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_ARRM);
  __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);
  __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
  __HAL_TIM_ENABLE_IT(&htim23, TIM_IT_UPDATE);
  
  return HAL_OK;
}


/**
 * @brief TIM更新中断回调函数（TIM编码器溢出处理）
 * @param htim: TIM句柄指针
 * @note 当TIM编码器计数器溢出时触发（65535->0 或 0->65535）
 * @note 通过硬件方向检测自动更新溢出计数：
 *       - 向上计数：overflow_count++
 *       - 向下计数：overflow_count--
 * @note 轴编号映射：
 *       - TIM3  -> ENCODER_AXIS_1 -> overflow_count[1]
 *       - TIM4  -> ENCODER_AXIS_2 -> overflow_count[2]  
 *       - TIM23 -> ENCODER_AXIS_3 -> overflow_count[3]
 */
void XQ_Encoder_TIM_Callback(TIM_HandleTypeDef *htim)
{
    // TIM4溢出处理 (ENCODER_AXIS_0)
    if (htim->Instance == TIM4) {
      if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim)) {
        overflow_count[0]--;  // 反向溢出：0 -> 65535
      } else {
        overflow_count[0]++;  // 正向溢出：65535 -> 0
      }
    }
    // TIM3溢出处理 (ENCODER_AXIS_3)
    else if (htim->Instance == TIM3) {
      if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim)) {
        overflow_count[3]--;  // 反向溢出：0 -> 65535
      } else {
        overflow_count[3]++;  // 正向溢出：65535 -> 0
      }
    }
    // TIM23溢出处理 (ENCODER_AXIS_4)
    else if (htim->Instance == TIM23) {
      if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim)) {
          overflow_count[4]--;  // 反向溢出：0 -> 65535
      } else {
          overflow_count[4]++;  // 正向溢出：65535 -> 0
      }
    }
}


/**
 * @brief LPTIM更新中断回调函数（LPTIM编码器溢出处理）
 * @param hlptim: LPTIM句柄指针
 * @note 当LPTIM2编码器计数器溢出时触发（ARR匹配中断）
 * @note LPTIM不支持硬件方向检测，通过读取当前计数值判断溢出方向：
 *       - 计数值 < 1000：正向溢出（计数器刚从ARR翻转到0附近）
 *       - 计数值 >= 1000：反向溢出（计数器刚从0翻转到ARR附近）
 * @note 轴编号映射：LPTIM2 -> ENCODER_AXIS_0 -> overflow_count[0]
 */
void XQ_Encoder_LPTIM_Callback(LPTIM_HandleTypeDef *hlptim) {
  if (hlptim->Instance == LPTIM2) {
    if (HAL_LPTIM_ReadCounter(&hlptim2) < 1000) {
      overflow_count[1]++;  // 正向溢出：65535 -> 0
    } else {
      overflow_count[1]--;  // 反向溢出：0 -> 65535
    }
  }
  else if (hlptim->Instance == LPTIM1) {
    if (HAL_LPTIM_ReadCounter(&hlptim1) < 1000) {
      overflow_count[2]++;  // 正向溢出：65535 -> 0
    } else {
      overflow_count[2]--;  // 反向溢出：0 -> 65535
    }
  }
}


/**
 * @brief 读取编码器32位扩展计数值（支持LPTIM和TIM）
 * @param axis: 编码器轴编号
 * @return 32位扩展计数值 = 溢出次数 × 65536 + 当前计数
 */

int64_t XQ_Axis_Get_Encoder_Count(XQ_EncoderAxis_t axis) {
  
  if (axis >= ENCODER_AXIS_COUNT) {
      return 0;
  }
  
  uint16_t current_count;
  int64_t result;

  if (axis == ENCODER_AXIS_1) {
      // LPTIM2
      current_count = (uint16_t)HAL_LPTIM_ReadCounter((LPTIM_HandleTypeDef*)encoder_timers[axis]);
      int64_t extended_count = ((int64_t)overflow_count[axis] * 60000) + current_count;
      result = extended_count - lptim2_virtual_offset;
  } else if (axis == ENCODER_AXIS_2) {
      // LPTIM1
      current_count = (uint16_t)HAL_LPTIM_ReadCounter((LPTIM_HandleTypeDef*)encoder_timers[axis]);
      int64_t extended_count = ((int64_t)overflow_count[axis] * 60000) + current_count;
      result = extended_count - lptim1_virtual_offset;
  } else {
      // TIM4/TIM3/TIM23
      current_count = (uint16_t)__HAL_TIM_GET_COUNTER((TIM_HandleTypeDef*)encoder_timers[axis]);
      int64_t extended_count = ((int64_t)overflow_count[axis] * 60000) + current_count;
      result = extended_count;
  }

  // 根据方向设定返回正确的值
  return result * encoder_direction[axis];

}


/**
 * @brief 设置编码器扩展计数值（支持LPTIM和TIM）
 * @param axis: 编码器轴编号
 * @param Encmm: 要设置的位置值（毫米）
 */

void XQ_SetEncmm(XQ_EncoderAxis_t axis, double_t Encmm) {
  if (axis >= ENCODER_AXIS_COUNT) {
      return;
  }

  uint8_t index = (uint8_t)axis;
  /* 计算脉冲与毫米的转换关系 */
  int64_t value = llround(Encmm * encoder_mm_to_pulse[index]);
  /* 计算溢出次数和硬件计数值 - 所有编码器ARR都是59999，周期为60000 */
  overflow_count[index] = (int32_t)(value / 60000);
  uint16_t hardware_count = (uint16_t)(value % 60000);
  
  if (axis == ENCODER_AXIS_1) {
    // LPTIM2 - 设置虚拟偏移
    uint16_t current_count = (uint16_t)HAL_LPTIM_ReadCounter((LPTIM_HandleTypeDef*)encoder_timers[axis]);
    int64_t current_extended = ((int64_t)overflow_count[axis] * 60000) + current_count;
    lptim2_virtual_offset = current_extended - value;
  } else if (axis == ENCODER_AXIS_2) {
    // LPTIM1 - 设置虚拟偏移
    uint16_t current_count = (uint16_t)HAL_LPTIM_ReadCounter((LPTIM_HandleTypeDef*)encoder_timers[axis]);
    int64_t current_extended = ((int64_t)overflow_count[axis] * 60000) + current_count;
    lptim1_virtual_offset = current_extended - value;
  } else {
    // TIM4/TIM3/TIM23 - 可以直接设置
    __HAL_TIM_SET_COUNTER((TIM_HandleTypeDef*)encoder_timers[axis], hardware_count);
  }
  
}


/**
 * @brief 批量读取编码器位置（毫米单位）
 * @param axis: 起始编码器轴编号
 * @param Encmm: 输出数组指针，用于存储位置值（毫米）
 * @param num: 要读取的编码器数量
 * @note 从指定轴开始，连续读取num个编码器的位置
 * @note 支持LPTIM2和TIM编码器的混合读取
 * @note 自动应用脉冲到毫米的转换系数
 * @note 轴编号会自动递增：axis, axis+1, axis+2, ...
 */
void
XQ_GetEncmm(XQ_EncoderAxis_t axis, double_t* Encmm, uint8_t num) {

  if (axis >= ENCODER_AXIS_COUNT || Encmm == NULL || num <= 0) {
      return;
  }

  for (int i = 0; i < num; i++, axis++) {
    int64_t count = XQ_Axis_Get_Encoder_Count(axis);
    Encmm[i] = (double_t)count * encoder_pulse_to_mm[axis];
  }
}


/* ======================== 编码器-轴绑定 ======================== */

XQ_EncoderAxisBinding_t encoder_bindings[ENCODER_AXIS_COUNT] = {0};


/* ======================== 手脉跟随 ======================== */

/* 手脉默认使用最后一个编码器 */
#define HANDWHEEL_ENCODER_ID  (ENCODER_AXIS_COUNT - 1)

typedef struct {
  uint8_t   bound;          /* 是否已绑定 */
  uint8_t   axis_id;        /* 跟随的电机轴号 */
  int32_t   ratio_num;      /* 倍率分子（编码器计数 × ratio_num / ratio_den = 目标脉冲增量） */
  int32_t   ratio_den;      /* 倍率分母 */
  uint32_t  max_speed_f;    /* 跟随运动最大速度 (Hz) */
  uint32_t  jerk_f;         /* 跟随运动加加速度 (Hz/s^3) */
  uint32_t  a_max_f;        /* 跟随运动最大加速度 (Hz/s^2) */
  uint32_t  time_factor;    /* 时间因子 */
  int64_t   prev_enc_count; /* 上次采样的编码器原始计数 */
  int32_t   target_pulse;   /* 累积的目标脉冲位置 */
} XQ_Handwheel_t;

static XQ_Handwheel_t handwheel = {0};


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
 * @note 绑定后不可解绑，不可重新绑定
 */
int8_t
XQ_Encoder_Bindto_Axis(uint8_t axis_id, XQ_EncoderAxis_t encoder_id,
                        uint32_t encoder_counts_per_rev, uint32_t motor_counts_per_rev,
                        uint32_t max_speed_f, uint32_t jerk_f, uint32_t a_max_f,
                        uint32_t time_factor, uint32_t deadband) {

  if (encoder_id >= ENCODER_AXIS_COUNT || axis_id >= AXIS_MAX_SUM)
    return -1;

  if (encoder_bindings[encoder_id].bound)
    return -2;

  encoder_bindings[encoder_id].bound              = 1;
  encoder_bindings[encoder_id].axis_id            = axis_id;
  encoder_bindings[encoder_id].encoder_id         = encoder_id;
  encoder_bindings[encoder_id].encoder_counts_per_rev = encoder_counts_per_rev;
  encoder_bindings[encoder_id].motor_counts_per_rev   = motor_counts_per_rev;
  encoder_bindings[encoder_id].max_speed_f        = max_speed_f;
  encoder_bindings[encoder_id].jerk_f             = jerk_f;
  encoder_bindings[encoder_id].a_max_f            = a_max_f;
  encoder_bindings[encoder_id].time_factor        = time_factor;
  encoder_bindings[encoder_id].deadband           = deadband;

  /* 绑定时即激活位置保持，以当前电机位置为保持目标 */
  encoder_bindings[encoder_id].hold_motor_pulse   = axis[axis_id].position;
  encoder_bindings[encoder_id].hold_active        = 1;

  /* 设置轴的反向查找字段，用于用户运动函数快速通知跟随任务 */
  axis[axis_id].encoder_bound = 1;
  axis[axis_id].encoder_id    = (uint8_t)encoder_id;

  return 0;
}


/**
 * @brief 根据编码器当前位置同步轴的电机脉冲位置
 * @param axis_id 轴编号
 * @note 运动前调用，将编码器实际位置换算为电机脉冲数并赋值给 axis[].position，
 *       使电机位置与编码器真实位置一致，作为后续运动的起点。
 */
void
XQ_Encoder_Sync_Position(uint8_t axis_id) {

  if (axis_id >= AXIS_MAX_SUM || !axis[axis_id].encoder_bound)
    return;

  uint8_t enc_idx = axis[axis_id].encoder_id;
  XQ_EncoderAxisBinding_t *b = &encoder_bindings[enc_idx];

  /* 读取编码器当前计数值 */
  int64_t enc_count = XQ_Axis_Get_Encoder_Count((XQ_EncoderAxis_t)enc_idx);

  /* 编码器计数 → 电机脉冲（正向换算） */
  int32_t motor_pulse = encoder_count_to_motor_pulse(b, enc_count);

  /* 同步到轴位置 */
  axis[axis_id].position = motor_pulse;
}


/**
 * @brief 通知编码器跟随任务：该轴有外部运动指令
 * @note 直接清除补偿标志并暂停位置保持，
 *       停止沿会自动重新激活保持并更新目标位置。
 */
void
XQ_Encoder_Notify_External_Move(uint8_t axis_id) {
  if (axis_id < AXIS_MAX_SUM && axis[axis_id].encoder_bound) {
    XQ_EncoderAxisBinding_t *b = &encoder_bindings[axis[axis_id].encoder_id];
    b->is_compensating = 0;
    b->hold_active     = 0;
  }
}


/**
 * @brief 编码器跟随任务函数
 * @param argument 未使用
 *
 * @note 位置保持逻辑：
 *       1. 停止沿（运动 → 停止）：
 *          - 补偿运动结束：清标志，hold_motor_pulse 不变，允许步骤 4 重试。
 *          - 外部运动结束：以命令位置更新 hold_motor_pulse，激活保持。
 *          - 无论哪种，均用编码器同步 axis[].position。
 *       2. 快速补偿恢复：连续两周期未运动则清补偿标志并同步位置。
 *       3. 空闲重新激活：hold_active 被 Notify 清除后，运动结束同步并恢复保持。
 *       4. 位置保持：比对编码器与 hold_motor_pulse，超死区则同步起点后补偿到 hold_motor_pulse。
 *       补偿运动可被用户指令打断（Notify 清除标志），待用户运动停止后由步骤 1 重新激活保持。
 */
static void
XQ_Encoder_Follow_Task(void *argument) {

  (void)argument;

  /* 等待系统初始化完成 */
  osDelay(2000);

  for (;;) {

    for (uint8_t i = 0; i < ENCODER_AXIS_COUNT; i++) {

      XQ_EncoderAxisBinding_t *b = &encoder_bindings[i];

      if (!b->bound)
        continue;

      AxisID aid = (AxisID)b->axis_id;
      uint8_t cur_moving = axis[aid].is_moving;

      /* ---- 1. 停止沿（运动 → 停止） ---- */
      if (b->prev_is_moving && !cur_moving) {
        if (b->is_compensating) {
          /* 补偿运动结束：清标志，hold_motor_pulse 不变以允许重试 */
          b->is_compensating = 0;
        } else {
          /* 外部运动结束：以当前命令位置为新的保持目标 */
          b->hold_motor_pulse = axis[aid].position;
          b->hold_active      = 1;
        }
        /* 任何运动停止后，用编码器真实位置同步轴位置 */
        XQ_Encoder_Sync_Position(aid);
      }

      /* ---- 2. 快速补偿恢复 ---- */
      if (b->is_compensating && !cur_moving && !b->prev_is_moving) {
        b->is_compensating = 0;
        XQ_Encoder_Sync_Position(aid);
      }

      /* ---- 3. 空闲重新激活 ---- */
      /* 处理快速运动（启动+停止在一个50ms周期内完成，未检测到停止沿） */
      if (!b->hold_active && !cur_moving && !b->is_compensating) {
        XQ_Encoder_Sync_Position(aid);
        b->hold_motor_pulse = axis[aid].position;
        b->hold_active      = 1;
      }

      b->prev_is_moving = cur_moving;

      /* ---- 4a. 运行中脉冲注入：用户运动（非补偿运动）时检测编码器偏差 ---- */
      if (cur_moving && !b->is_compensating) {
        int64_t enc_count     = XQ_Axis_Get_Encoder_Count(b->encoder_id);
        int32_t encoder_pulse = encoder_count_to_motor_pulse(b, enc_count);
        int32_t dev           = encoder_pulse - axis[aid].position;
        axis[aid].comp_encoder_dev = dev;

        if (dev < -(int32_t)b->deadband) {
          /* 电机超前编码器 → 少发 1 个脉冲 */
          xq_axis_comp_inject(aid, -1);
        } else if (dev > (int32_t)b->deadband) {
          /* 电机滞后编码器 → 多发 1 个脉冲 */
          xq_axis_comp_inject(aid, +1);
        }
      }

      /* ---- 4b. 位置保持：检查编码器偏移并补偿 ---- */

      if (b->hold_active && !cur_moving && !b->is_compensating) {

        int64_t enc_count     = XQ_Axis_Get_Encoder_Count(b->encoder_id);
        int32_t encoder_pulse = encoder_count_to_motor_pulse(b, enc_count);
        int32_t drift         = encoder_pulse - b->hold_motor_pulse;

        if (abs(drift) > (int32_t)b->deadband) {
          /* 同步轴位置到编码器真实值，确保 S 曲线起点正确 */
          axis[aid].position = encoder_pulse;
          SCurve *s = xq_axis_new_curve_for_target_position(
                        aid, axis[aid].min_start_f, b->max_speed_f,
                        b->hold_motor_pulse,
                        b->jerk_f, b->a_max_f, b->time_factor,
                        AXIS_POS_MODE_ABS);
          if (s != NULL) {
            b->is_compensating = 1;
            xq_axis_start_pwm(aid, s);
          }
        }
      }
    }

    /* ======================== 手脉跟随 ======================== */
    if (handwheel.bound) {
      AxisID hw_aid = (AxisID)handwheel.axis_id;

      /* 读取手脉编码器当前计数 */
      int64_t hw_enc = XQ_Axis_Get_Encoder_Count((XQ_EncoderAxis_t)HANDWHEEL_ENCODER_ID);
      int64_t delta_enc = hw_enc - handwheel.prev_enc_count;
      handwheel.prev_enc_count = hw_enc;

      /* 编码器增量 → 目标脉冲增量 */
      if (delta_enc != 0) {
        int32_t delta_pulse = (int32_t)((delta_enc * handwheel.ratio_num) / handwheel.ratio_den);
        handwheel.target_pulse += delta_pulse;
      }

      /* 驱动电机跟随目标位置 */
      if (handwheel.target_pulse != axis[hw_aid].position) {
        if (axis[hw_aid].is_moving) {
          /* 运动中：动态改变目标位置 */
          xq_axis_change_target_position(hw_aid, handwheel.target_pulse,
                                          handwheel.max_speed_f,
                                          handwheel.jerk_f, handwheel.a_max_f,
                                          handwheel.time_factor);
        } else {
          /* 静止：创建 S 曲线并启动 */
          SCurve *s = xq_axis_new_curve_for_target_position(
                        hw_aid, axis[hw_aid].min_start_f,
                        handwheel.max_speed_f, handwheel.target_pulse,
                        handwheel.jerk_f, handwheel.a_max_f,
                        handwheel.time_factor, AXIS_POS_MODE_ABS);
          if (s != NULL)
            xq_axis_start_pwm(hw_aid, s);
        }
      }
    }

    osDelay(50);  /* 50ms周期 */
  }
}


/**
 * @brief 绑定手脉到指定轴
 * @param axis_id      要跟随的电机轴号
 * @param ratio_num    倍率分子（编码器增量 × ratio_num / ratio_den = 电机脉冲增量）
 * @param ratio_den    倍率分母
 * @param max_speed_f  跟随运动最大速度 (Hz)
 * @param jerk_f       加加速度 (Hz/s^3)
 * @param a_max_f      最大加速度 (Hz/s^2)
 * @param time_factor  时间因子
 * @return 0=成功, -1=参数错误, -2=已绑定
 */
int8_t
XQ_Handwheel_Bind(uint8_t axis_id, int32_t ratio_num, int32_t ratio_den,
                   uint32_t max_speed_f, uint32_t jerk_f, uint32_t a_max_f,
                   uint32_t time_factor) {

  if (axis_id >= AXIS_MAX_SUM || ratio_den == 0)
    return -1;

  if (handwheel.bound)
    return -2;

  handwheel.axis_id      = axis_id;
  handwheel.ratio_num    = ratio_num;
  handwheel.ratio_den    = ratio_den;
  handwheel.max_speed_f  = max_speed_f;
  handwheel.jerk_f       = jerk_f;
  handwheel.a_max_f      = a_max_f;
  handwheel.time_factor  = time_factor;

  /* 以当前编码器位置为起点，以当前电机位置为目标起点 */
  handwheel.prev_enc_count = XQ_Axis_Get_Encoder_Count((XQ_EncoderAxis_t)HANDWHEEL_ENCODER_ID);
  handwheel.target_pulse   = axis[axis_id].position;

  handwheel.bound = 1;
  return 0;
}


/**
 * @brief 解除手脉绑定
 * @return 0=成功, -1=未绑定
 */
int8_t
XQ_Handwheel_Unbind(void) {

  if (!handwheel.bound)
    return -1;

  handwheel.bound = 0;
  return 0;
}


/**
 * @brief 启动编码器跟随任务
 */
void
XQ_Encoder_Follow_Task_Start(void) {

  if (encoder_follow_task_handle != NULL)
    return;  /* 已启动，不重复创建 */

  xTaskCreate(XQ_Encoder_Follow_Task,
              "EncFollow",
              512,          /* 栈大小 (words) */
              NULL,
              osPriorityAboveNormal,
              &encoder_follow_task_handle);
}

