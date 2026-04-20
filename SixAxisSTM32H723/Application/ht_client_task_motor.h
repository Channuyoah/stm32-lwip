#ifndef __HT_CLIENT_TASK_MOTOR_H__
#define __HT_CLIENT_TASK_MOTOR_H__

#include "ht_udp_server.h"

typedef enum {   
  RET_STATUS_OK = 0,
  RET_STATUS_ERR_UN_CALIBRATION = 1,  /* 错误：未校读 */
  RET_STATUS_ERR_REQUEST_STOP = 2,    /* 错误：收到急停，指令中断执行，立即退出 */
  RET_STATUS_ERR_RATE_TABLE_UN_RUNNING = 3 /* 错误：率表未运行，不可进行单步转动 */
}RetStatus;

int8_t 
ht_client_motor_task_find_zero (ServerData *t);

int8_t 
ht_client_motor_task_set_target_position (ServerData *t);

int8_t 
ht_client_motor_task_set_target_speed (ServerData *t);

#endif // !__HT_CLIENT_MOTOR_CMD_H__
