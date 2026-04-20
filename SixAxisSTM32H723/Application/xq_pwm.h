#ifndef __XQ_PWM_H__
#define __XQ_PWM_H__

#include "main.h"
#include "tim.h"

// PWM通道定义
#define PWM_CHANNEL_TIM5_CH1    0  // TIM5 CH1
#define PWM_CHANNEL_TIM1_CH2    1  // TIM1 CH2

/* PWM配置信息 */
typedef struct {
    TIM_HandleTypeDef* htim;    // 定时器句柄
    uint32_t channel;           // 通道编号
    const uint8_t* name;           // 通道名称
    uint32_t tim_clock_hz;      // 定时器时钟频率(Hz)
    uint8_t is_running;        // PWM运行状态标志
    
    uint16_t arr[2];            // PWM周期ARR值[0]=正常模式, [1]=停止模式
    uint16_t ccr[2];            // PWM占空比CCR值[0]=正常模式, [1]=停止模式
    uint32_t total_count[2];     // 发送次数[0]=正常模式, [1]=停止模式
    uint32_t current_count;     // 当前发送计数
} PWM_Config_t;


extern PWM_Config_t *pwm1_config;
extern PWM_Config_t *pwm2_config;

// 函数声明
HAL_StatusTypeDef 
XQ_setPWM(uint8_t cn, float_t ton, float_t T, uint32_t T_count, float_t idle_T);

// 中断回调函数声明
void TIM1_UpdateCallback(void);
void TIM5_UpdateCallback(void);


#endif // !__XQ_PWM_H__
