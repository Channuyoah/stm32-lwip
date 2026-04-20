#include "ht_client_task_interrupt.h"
#include "ht_client_task_motor.h"
#include "ht_client_task.h"
#include "xq_axis.h"
#include "tim.h"
#include "string.h"

int8_t
ht_client_interrupt_task_motor_stop(ServerData *t) {

  int8_t status = 0;

  ServerData *queue_t = NULL;

  printf ("start stop\n");
  
  /* 释放电机命令队列 */
  while (osMessageQueueGet (clientMotorTaskQueueHandle, &queue_t, 0, 0) == osOK) {
    ht_udp_server_data_free (queue_t);
  }

  for (AxisID i = AXIS_0; i < AXIS_SUM; i++) {
    /* 只有电机在运动模式下，才会设定停止事件 */
    // if (motor[i].status > MOTOR_STATUS_CALIBRATION_FINISH) {
      axis[i].stop_flag = 1;
      /* 发送请求电机停止信号 */
      // osSemaphoreRelease (axis[i].request_stop_sem);
    // }
  }

  for (AxisID i = AXIS_0; i < AXIS_SUM; i++) {
    while (axis[i].stop_flag == 1) {
      osDelay (100);
      // printf ("motor[%d].stop_fla = %d\n", i, motor[i].stop_flag);
      // osSemaphoreRelease (motor[i].running_curve_finish_sem);
    }
  }

  ht_client_task_send_frame (CFMC_INTERRUPT, CFMC_INTERRUPT_TASK_STOP + 0x80, (uint16_t)status, NULL, 0);

  printf ("stop finish\n");

  return status;
}


int8_t
ht_client_interrupt_task_window(ServerData *t) {

  int8_t status = 0;

  return status;
}


int8_t
ht_client_interrupt_task_stop_and_close_window(ServerData *t) {

  int8_t status = 0;

  return status;
}




