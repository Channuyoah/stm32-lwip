/**
 * @file ht_client_task.c
 * @author lisibo
 * @brief 
 * @version 1.0
 * @date 2025-03-24
 * 
 * XD-2: 
 * XD-3: 
 * XD-6: 
 */

#include "ht_client_task.h"
#include "ht_client_task_motor.h"       /* 电机命令任务具体执行函数 */
#include "ht_client_task_interrupt.h"   /* 中断命令具体执行函数 */
#include "xq_axis.h"
#include "string.h"


osMessageQueueId_t    clientInterruptTaskQueueHandle = NULL;
osMessageQueueAttr_t  clientInterruptTaskQueue_attributes = {
  .name = "clientInterruptTaskQueue"
};


osMessageQueueId_t    clientMotorTaskQueueHandle = NULL;
osMessageQueueAttr_t  clientMotorTaskQueue_attributes = {
  .name = "clientMotorTaskQueue"
};


/**
 * @brief 客户端中断任务（停止、开关窗）等任务
 * @note 查询任务会立即返回查询结果，不会阻塞
 */
osThreadId_t clientInterruptTaskHandle;
const osThreadAttr_t clientInterruptTask_attributes = {
  .name = "clientInterruptTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityRealtime3,
};



/**
 * @brief 客户端设置电机，让电机执行相应操作的任务（校读、设置电机位置、叠扫等）
 * 
 */
osThreadId_t clientMotorTaskHandle;
const osThreadAttr_t clientMotorTask_attributes = {
  .name = "clientMotorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityRealtime1, 
};


/**
 * @brief 客户端数据处理函数
 *       
 *        1. 帧头帧尾、校验检查数据帧是否正确
 *        2. 根据命令类型，发送到相应的客户端执行任务Queue中
 * 
 * @note: 需要释放 frame 指针
 * 
 * @param frame 
 * @return int8_t 
 * 
 */
CfpStatus
ht_client_data_frame_process (ServerData *frame) {

  // UART_PRINT (frame->buf, frame->len);

  CfpStatus status = CFP_OK;
  uint16_t checkVal;

  /* 暂时未发现分包、粘包情况 */
	;

  /* 帧长度检查 */
  if (frame->len < CLIENT_FRAME_LEN) {
    status = CFP_LEN_ERR;
    goto exit;
  }

  /* 帧头、帧尾检查 */
  if (frame->buf[CLIENT_FRAME_HEAD] != 0x68 || frame->buf[CLIENT_FRAME_TAIL] != 0x16) {
    status = CFP_CHECK_ERR;
    goto exit;
  }

  /* 数据校验检查（从帧序列号到数据结束，也就是除了帧头、帧位、2位校验，这四个字节） */
  checkVal = (frame->buf[CLIENT_FRAME_CHECK] << 8) | frame->buf[CLIENT_FRAME_CHECK + 1];
  if (ht_client_get_frame_check(frame->buf + CLIENT_FRAME_SN, CLIENT_FRAME_LEN - 4) != checkVal) {
    status = CFP_CHECK_ERR;
    goto exit;
  }

  switch (frame->buf[CLIENT_FRAME_CMD_TYPE]) {

    /* 中断命令 */
    case CFMC_INTERRUPT: {
      osMessageQueuePut (clientInterruptTaskQueueHandle, &frame, 0, 0);
      break;
    }

    /* 电机命令（会有等待电机执行完成） */
    case CFMC_MOTOR: {
      osMessageQueuePut (clientMotorTaskQueueHandle, &frame, 0, 0);
      break;
    }

    /* 接收到无效指令 */
    default: {

      printf ("Error client frame main cmd\n");

      status = CFP_CMD_ERR;
      goto exit;
    }

  } 

exit:

  if (status != CFP_OK) {
    ht_udp_server_data_free (frame);
    printf ("rec udp error 0x%x\n", status);
  }

  return status;
}


void 
ht_client_interrupt_task (void *argument) {

  ServerData *t = NULL;

  for (;;) {

    osMessageQueueGet (clientInterruptTaskQueueHandle, &t, 0, osWaitForever);

    // UART_PRINT (t->buf, t->len);

    switch (t->buf[CLIENT_FRAME_CMD]) {

      case CFMC_INTERRUPT_TASK_STOP: {
        ht_client_interrupt_task_motor_stop (t);
        printf ("CFMC_DIRECT_MOTOR_STOP\n");
        break;
      }
        
      default: {
        break;
      }
    }

    /* 释放数据帧占用的内存 */
    if (t) {
      ht_udp_server_data_free (t);
      t = NULL;
    }

  }


}


void 
ht_client_motor_task (void *argument) {

  ServerData *t = NULL;
 
  for (;;) {

    osMessageQueueGet (clientMotorTaskQueueHandle, &t, 0, osWaitForever);

    // UART_PRINT (t->buf, t->len);

    switch (t->buf[CLIENT_FRAME_CMD]) {

      case CFMC_MOTOR_TASK_FIND_ZERO: {
        printf ("CFMC_MOTOR_TASK_FIND_ZERO\n");
        ht_client_motor_task_find_zero (t);
        break;
      }

      case CFMC_MOTOR_TASK_SET_POSITION: {
        // printf ("CFMC_MOTOR_TASK_SET_POSITION\n");
        ht_client_motor_task_set_target_position (t);
        break;
      }

      case CFMC_MOTOR_TASK_SET_SPEED: {
        // printf ("CFMC_MOTOR_TASK_SET_SPEED\n");
        ht_client_motor_task_set_target_speed (t);
        break;
      }
        
      default: {
        printf ("%s error cmd\n", __func__);
        break;
      }
    }

    /* 释放数据帧占用的内存 */
    if (t) {
      ht_udp_server_data_free (t);
      t = NULL;
    }

  }

}


int8_t
ht_client_task_init (void) {

  int8_t status = 0;
  
  clientInterruptTaskQueueHandle = osMessageQueueNew (20, sizeof (ServerData *), &clientInterruptTaskQueue_attributes);

  clientMotorTaskQueueHandle = osMessageQueueNew (20, sizeof (ServerData *), &clientMotorTaskQueue_attributes);

  clientInterruptTaskHandle = osThreadNew(ht_client_interrupt_task, NULL, &clientInterruptTask_attributes);

  clientMotorTaskHandle = osThreadNew(ht_client_motor_task, NULL, &clientMotorTask_attributes);

  return status;
}


/**
 * @brief 校验计算
 * 
 * @param buffer 
 * @param len 
 * @return uint16_t 
 */
uint16_t
ht_client_get_frame_check (uint8_t *buffer, uint16_t len) {

  uint16_t rev = 0;

  for (uint16_t i = 0; i < len; i++)
    rev += buffer[i];

  // printf ("%s rev = %X\n", __func__, rev);

  return rev;
}


int8_t
ht_client_task_send_frame(uint8_t cmd_type, uint8_t cmd, uint16_t err_code, const uint8_t *data, uint16_t data_size) {

  static uint64_t sn = 0;

  int8_t status = 0;

  ServerData *frame = ht_udp_server_data_new (CLIENT_FRAME_LEN);

  uint8_t *buf = frame->buf;
  uint16_t checkVal;

  buf[CLIENT_FRAME_HEAD] = 0x68;
  buf[CLIENT_FRAME_TAIL] = 0x16;
  // buf[CLIENT_FRAME_MACHINE_TYPE] = MACHINE_TYPE_XD_3;
  buf[CLIENT_FRAME_CMD_TYPE] = cmd_type;
  buf[CLIENT_FRAME_CMD]  = cmd;
  buf[CLIENT_FRAME_DATA_LEN] = 0xC8;

  
  buf[CLIENT_FRAME_SCAN_DATA_LEN] = (uint8_t)data_size;

  memcpy (buf + CLIENT_FRAME_ERROR_CODE, &err_code, 2);
  

  memcpy (buf + CLIENT_FRAME_SN, &sn, 8);
  sn++;

  if (data == NULL) {
    /* 计数、M1位置、M2位置 */
    // memcpy (buf +  CLIENT_FRAME_DATA, &counter, 4);
    memcpy (buf +  CLIENT_FRAME_DATA + 4, &axis[AXIS_0].position, 4);
    memcpy (buf +  CLIENT_FRAME_DATA + 8, &axis[AXIS_1].position, 4);
  } else {
    memcpy (buf + CLIENT_FRAME_DATA, data, data_size);
  }

  /* 校验计算 */
  checkVal = ht_client_get_frame_check(buf + CLIENT_FRAME_SN, CLIENT_FRAME_LEN - 4);

  memcpy (buf + CLIENT_FRAME_CHECK, &checkVal, 2);

  ht_udp_server_data_send (frame);

  return status;
}



