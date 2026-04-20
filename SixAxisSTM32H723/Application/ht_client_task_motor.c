#include "ht_client_task_motor.h"
#include "ht_client_task.h"
#include "xq_axis.h"
#include "tim.h"
#include "string.h"
#include "xq_curve.h"

#include <math.h>
#include <stdlib.h>

/**
 * @brief 
 * 
 * @note 光电开关接黑色线： 遮挡触发上升沿 （高电平）
 *                        遮挡物移开触发下降沿 （低电平）
 *        
 *       光电开关高角度不遮挡，0度后（负角度）开始遮挡
 * 
 * @param t 
 * @return uint8_t 
 */
int8_t 
ht_client_motor_task_find_zero (ServerData *t) {
  int8_t status = RET_STATUS_OK;

  return status;
}


/**
 * @brief 根据客户端命令设置电机位置
 * 
 * @param t 
 * @return uint8_t 
 */
int8_t 
ht_client_motor_task_set_target_position (ServerData *t) {

  int8_t status = RET_STATUS_OK;

  float_t max_speed;
  float_t target_position;
  float_t jerk;
  float_t acc_max;
  int32_t timer_factor;
  AxisID axis_id;

  /* 提取电机是否运行flag */
  axis_id = (AxisID)t->buf[CLIENT_FRAME_MOTOR_RUN_FLAG];

  /* 提取电机是否运行flag */
  // memcpy (&motor_fun_flag, t->buf + CLIENT_FRAME_MOTOR_RUN_FLAG, 2);

  /* 跳到数据开始位置 */
  uint8_t *motor_buf = t->buf + CLIENT_FRAME_DATA;

  memcpy (&max_speed, motor_buf, 4); motor_buf += 4;
  memcpy (&target_position, motor_buf, 4); motor_buf += 4;
  memcpy (&jerk, motor_buf, 4); motor_buf += 4;
  memcpy (&acc_max, motor_buf, 4); motor_buf += 4;
  memcpy (&timer_factor, motor_buf, 4);

  // printf ("%s\n", __func__);

  // uint32_t start_time = TIM13->CNT;

  XQ_ABSMove (axis_id, target_position, max_speed, jerk, acc_max, timer_factor);

  // uint32_t end_time = TIM13->CNT;

  // printf ("XQ_ABSMove time = %d us\n", end_time - start_time);

  return status;
}


int8_t 
ht_client_motor_task_set_target_speed (ServerData *t) {

  int8_t status = RET_STATUS_OK;

  float_t target_speed;
  float_t jerk;
  float_t acc_max;
  int32_t timer_factor;
  AxisID axis_id;

  /* 提取电机是否运行flag */
  axis_id = (AxisID)t->buf[CLIENT_FRAME_MOTOR_RUN_FLAG];

  /* 跳到数据开始位置 */
  uint8_t *motor_buf = t->buf + CLIENT_FRAME_DATA;

  memcpy (&target_speed, motor_buf, 4); motor_buf += 4;
  memcpy (&jerk, motor_buf, 4); motor_buf += 4;
  memcpy (&acc_max, motor_buf, 4); motor_buf += 4;
  memcpy (&timer_factor, motor_buf, 4);

  // printf ("%s\n", __func__);

  // uint32_t start_time = TIM13->CNT;

  XQ_JogMove (axis_id, (float_t)target_speed, (float_t)jerk, (float_t)acc_max, timer_factor);

  // uint32_t end_time = TIM13->CNT;

  // printf ("XQ_JogMove time = %d us\n", end_time - start_time);

  return status;
}



int8_t 
ht_client_motor_task_scan (ServerData *t) {
  int8_t status = RET_STATUS_OK;
  
  return status;
}



/**
 * @brief 获取电机当前位置
 * 
 * @param t 
 * @return int8_t 
 */
int8_t
ht_client_motor_task_get_location(ServerData *t) {

  int8_t status = 0;

  ht_client_task_send_frame (CFMC_MOTOR, CFMC_MOTOR_TASK_GET_MOTOR_POSITION + 0x80, (uint16_t)status, NULL, 0);

  return status;
}


/**
 * @brief 齿轮磨合
 * 
 * @param t 
 * @return int8_t 
 */
int8_t
ht_client_motor_task_gear_grinding(ServerData *t) {
  int8_t status = RET_STATUS_OK;
  
  return status;
}


/**
 * @brief 单步转动，可设定步宽
 * 
 * @param t 
 * @return int8_t 
 */
int8_t
ht_client_motor_one_step(ServerData *t) {
  int8_t status = RET_STATUS_OK;
  
  return status;
}



/**
 * @brief 校读时，光电输入信号中断
 * 
 * @param GPIO_Pin 
 */
void 
HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

}



