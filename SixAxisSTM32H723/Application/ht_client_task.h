#ifndef __HT_CONTROL_DATA_PROCESS_H__
#define __HT_CONTROL_DATA_PROCESS_H__

#include "main.h"
#include "stdio.h"
#include "ht_udp_server.h"
#include "cmsis_os2.h"

#define UART_PRINT(buffer, len) \
printf("%s\n", __func__);\
printf ("len = %d\n", len);\
for (int i = 0; i < len; i++){\
printf ("%X ", buffer[i]);}\
printf ("\n");\


/* 客户端每帧的长度 */
#define CLIENT_FRAME_LEN 256

/**
 * @brief 客户端数据帧每部分长度
 * 
 */
#define CLIENT_FRAME_LEN_SN  8
#define CLIENT_FRAME_LEN_MACHINE_TYPE  2
#define CLIENT_FRAME_LEN_ERROR_CODE  2
#define CLIENT_FRAME_LEN_DATA  200
#define CLIENT_FRAME_LEN_CHECK  2


/**
 * @brief 客户端数据帧在buffer中的起始位置
 * 
 */
typedef enum {
  CLIENT_FRAME_HEAD = 0,
  CLIENT_FRAME_SN = 1, /* 帧序列号，8字节 */
  CLIENT_FRAME_MACHINE_TYPE = 9, /* 机器类型，2字节 */
  CLIENT_FRAME_CMD_TYPE = 11, /* 帧命令类型 */ 
  CLIENT_FRAME_CMD = 12, /* 帧子命令 */
  CLIENT_FRAME_ERROR_CODE = 13, /* 错误码，2字节 */
  CLIENT_FRAME_SCAN_DATA_LEN = 15, /* 扫描数据的有效长度，1字节 */
  CLIENT_FRAME_MOTOR_RUN_FLAG = 16, /* 电机是否运行标志位 */
  CLIENT_FRAME_DATA_LEN = 52, /* 表示数据固定占多少字节（其实没多大用），固定就是200字节 */
  CLIENT_FRAME_DATA = 53, /* 数据位开始位置，200字节 */
  CLIENT_FRAME_CHECK = 253, /* 帧校验 */
  CLIENT_FRAME_TAIL = 255
}ClientFrameEnum;



/**
 * @brief Client frame main cmd type
 * 
 */
typedef enum {
  CFMC_COUNTER,   /* 计数命令 */
  CFMC_INTERRUPT, /* 中断命令 */
  CFMC_MOTOR      /* 电机命令 */
}CfmcType;


/**
 * @brief Client frame main cmd of direct type
 * 
 */
typedef enum {
  CFMC_INTERRUPT_TASK_STOP,                           /* 停止 */
}CfmcInterruptTaskType;


/**
 * @brief Client frame main cmd of Motor type
 *
 */
typedef enum {
    CFMC_MOTOR_TASK_FIND_ZERO,            /* 电机校读 */
    CFMC_MOTOR_TASK_SET_POSITION,         /* 电机转动到设定位置 */
    CFMC_MOTOR_TASK_SET_SPEED,            /* 电机设置速度 */
    CFMC_MOTOR_TASK_GET_MOTOR_POSITION
}CfmcMotorTaskType;


/**
 * @brief Client Frame Process 情况
 * 
 */
typedef enum {
  CFP_OK = 0,
  CFP_LEN_ERR,
  CFP_CHECK_ERR,
  CFP_CMD_ERR
}CfpStatus;

extern osMessageQueueId_t clientInterruptTaskQueueHandle;
extern osMessageQueueId_t clientMotorTaskQueueHandle;

int8_t
ht_client_task_init (void);

CfpStatus
ht_client_data_frame_process (ServerData *frame);

uint16_t
ht_client_get_frame_check (uint8_t *buffer, uint16_t len);

int8_t
ht_client_task_send_frame(uint8_t cmd_type, uint8_t cmd, uint16_t err_code, const uint8_t *data, uint16_t data_size);


#endif // !__HT_CONTROL_DATA_PROCESS_H__

