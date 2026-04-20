#ifndef __XQ_AXIS_H__
#define __XQ_AXIS_H__

#include "main.h"
#include "xq_curve.h"
#include "cmsis_os.h"

/* 最大可以有16个电机 */
#define AXIS_MAX_SUM 16

/* 目前拥有7个电机 */
#define AXIS_SUM 7


/**
 * 定时器时钟频率，通过该值除以频率f可以计算得到ARR的值
 * STM32H7系列，275MHz定时器频率需要除以25，也就是24分频
 * 使用10000000定时器主频，16位定时器65535，步进电机最低频率 153Hz
 * 
 * 频率与ARR计算关系：
 * - ARR = 定时器时钟频率 / 目标频率
 * - 目标频率 = 定时器时钟频率 / ARR
 * 
 * 16位定时器最低频率计算：
 * - 16位定时器ARR最大值 = 65535 (2^16 - 1)
 * - 最低频率 = F2TIME_PARA / 65535
 * - 最低频率 = 10,000,000 / 65535 ≈ 152.59 Hz ≈ 153 Hz
 * 
 * 注意：频率越低，ARR值越大；频率越高，ARR值越小
 */
#define F2TIME_PARA				     10000000

/* 电机的启动频率 */
#define AXIS_START_FRE 					 200

/* 电机频率的最大加速度 */
#define AXIS_ACC_MAX             40000

/* 电机频率的加加速度 */
#define AXIS_JERK_MAX						 25000

/* 电机最大运行频率 */
#define AXIS_MAX_FRE             100000

/* 电机细分（一圈多少个脉冲数） */
#define AXIS_MICROSTEPS          500

/* 螺距 */
#define AXIS_PITCH         1.0

/* 时间因子 */
#define TIME_FACTOR               4

/* 永远运行的脉冲数 */
#define AXIS_STEP_FOREVER        0xFFFFFFFFU

/* 补偿窗口：用 COMP_WINDOW 个脉冲的时间均摊补偿 */
#define COMP_WINDOW              8


/**
 * @brief 轴编号，从0开始。
 * 
 */
typedef enum {
  AXIS_0,
  AXIS_1,
  AXIS_2,
  AXIS_3,
  AXIS_4,
  AXIS_5,
  AXIS_6,
  AXIS_7,
  AXIS_8,
  AXIS_9,
  AXIS_10,
  AXIS_11,
  AXIS_12,
  AXIS_13,
  AXIS_14,
  AXIS_15
}AxisID;


/**
 * @brief 电机当前状态
 * 
 */
typedef enum {
  AXIS_STATUS_DISABLE = 0,
  AXIS_STATUS_ENABLE, /* 电机使能 */
} AxisStatus;


typedef enum {
  AXIS_DIR_CCW = 0,  /* 正转，面朝电机，逆时针转为正，DIR=0低电平 */
  AXIS_DIR_CW, /* 反转，面朝电机，顺时针为负，DIR=1高电平 */
}AxisDir;


typedef enum {
  AXIS_RUN_MODE_IDLE = 0, /* 空闲状态 */
  AXIS_RUN_MODE_SPEED,    /* 速度模式 */
  AXIS_RUN_MODE_POSITION,  /* 位置模式 */
  AXIS_RUN_MODE_INTP       /* 插补模式 */
}AxisRunMode;


typedef enum{
  AXIS_POS_MODE_ABS = 0, /* 绝对位置模式 */
  AXIS_POS_MODE_REL      /* 相对位置模式 */
}AxisPosMode;


typedef struct _XQ_SetAxisPara XQ_SetAxisPara;
struct _XQ_SetAxisPara {

  AxisID axis; /* 电机轴ID，从零开始 */

  uint32_t microsteps; /* 步进电机细分 = 每转脉冲数 */

  float_t pitch; /* 螺距（电机转一圈丝杆前进距离），单位mm */

  uint8_t stop_flag; /* 请求停止标记 */

  uint8_t is_moving; /* 电机是否正在运行 */

  AxisRunMode run_mode; /* 电机当前运行模式 */

  int32_t position; /* 电机位置（脉冲位置） */

  uint32_t min_start_f; /* 最小启动频率 */

  uint32_t max_running_f; /* 最大运行频率 */

  uint32_t max_acc; /* 最大加速度（脉冲/s^2） */

  uint32_t jerk; /* 加加速度（脉冲/s^3） */

  uint32_t time_factor; /* 时间因子 */

  uint8_t encoder_bound; /* 是否绑定编码器 */

  uint8_t encoder_id; /* 绑定的编码器ID */

  /* 插补从轴脉冲门控（Bresenham） */
  uint8_t  intp_is_slave;      /* 是否为插补从轴 */
  uint32_t intp_slave_total;   /* 从轴总脉冲数 */
  uint32_t intp_master_total;  /* 主轴总脉冲数（= 从轴定时器溢出总数） */
  uint32_t intp_gate_error;    /* Bresenham 累积误差 */

  /* 编码器偏差补偿 */
  volatile int32_t  comp_encoder_dev;   /* 与编码器的累积偏差（脉冲数） */
  volatile int8_t   comp_request;       /* 补偿请求: +1=多发, -1=少发, 0=无 */
  volatile uint8_t  comp_active;        /* 正在 ARR 补偿阶段（非从轴用） */
  volatile uint8_t  comp_count;         /* ARR 补偿已溢出次数 */
  volatile uint8_t  comp_total;         /* ARR 补偿总溢出数 */
  volatile int8_t   comp_delta;         /* 当前补偿方向: +1 或 -1 */
  volatile int8_t   comp_position_adj;  /* 从轴补偿标记: >0 跳过position计入, <0 幻影position */
  volatile int8_t   comp_override;      /* Bresenham 实际覆写标记: +1=强制脉冲, -1=强制跳过 */

  /* 电机当前运行（执行的曲线） */
  SCurve *running_curve;

  /* 对应的定时器进行PWM输出 */
  TIM_HandleTypeDef *tim;

  /* PWM输出的通道 */
  uint32_t channel;

  /* 电机方向IO控制 */
  GPIO_TypeDef *DIR_GPIO_Port;
  uint16_t DIR_GPIO_Pin;
};


extern XQ_SetAxisPara axis[AXIS_MAX_SUM];
extern float_t Prm_AxisVel[AXIS_MAX_SUM];  /* 轴速度 mm/s */
extern float_t Prm_PrmsPos[AXIS_MAX_SUM]; /* 轴规划位置 mm */

/**
 * @brief 设置电机旋转方向
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param dir 旋转方向（AXIS_DIR_CCW=逆时针，AXIS_DIR_CW=顺时针）
 * @note 通过控制DIR引脚的电平来设置电机方向
 *       DIR=0(低电平)为逆时针，DIR=1(高电平)为顺时针
 */
static inline void
xq_axis_set_dir (AxisID id, AxisDir dir) {
  HAL_GPIO_WritePin (axis[id].DIR_GPIO_Port, axis[id].DIR_GPIO_Pin, (GPIO_PinState)dir);
}

/**
 * @brief 切换电机旋转方向
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @note 翻转DIR引脚状态，实现方向反转
 */
static inline void
xq_axis_toggle_dir (AxisID id) {
  HAL_GPIO_TogglePin (axis[id].DIR_GPIO_Port, axis[id].DIR_GPIO_Pin);
}

/**
 * @brief 读取电机当前旋转方向
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @return AxisDir 当前方向（AXIS_DIR_CCW或AXIS_DIR_CW）
 */
static inline AxisDir
xq_axis_read_dir (AxisID id) {
  return (AxisDir)HAL_GPIO_ReadPin (axis[id].DIR_GPIO_Port, axis[id].DIR_GPIO_Pin);
}

/**
 * @brief 将下一个S曲线连接到当前曲线
 * @param c_s 当前S曲线指针
 * @param next_s 下一个S曲线指针
 * @return SCurve* 返回next_s指针，方便链式调用
 * @note 用于构建S曲线链表，实现多段运动的连续执行
 */
static inline SCurve *
xq_curve_add_next (SCurve *c_s, SCurve *next_s) {
  c_s->next_s = next_s;
  return next_s;
}

/**
 * @brief 初始化所有轴
 * @note 配置各轴的定时器、GPIO、初始参数等
 *       必须在使用任何轴控制功能前调用
 */
void 
xq_axis_init(void);

/**
 * @brief 启动指定轴的PWM输出
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param s S曲线指针，包含运动参数
 * @note 根据S曲线参数配置定时器并启动PWM输出
 *       PWM频率对应步进电机的脉冲频率
 */
void
xq_axis_start_pwm(AxisID id, SCurve *s);

/**
 * @brief 停止指定轴的PWM输出
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @note 立即停止定时器的PWM输出，电机停止运动
 */
void 
xq_axis_stop_pwm(AxisID id);

/**
 * @brief 定时器更新中断处理函数
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @note 在定时器更新中断中调用，用于更新S曲线参数
 *       实现速度的实时调整和位置的累加计数
 */
void 
xq_axis_timx_update(AxisID id);

/**
 * @brief 向轴注入编码器偏差补偿（从1ms定时任务调用）
 * @param id    轴ID
 * @param delta +1=多发1个脉冲（电机滞后），-1=少发1个脉冲（电机超前）
 * @return int8_t 0=已接受，-1=上次请求未处理完
 */
int8_t
xq_axis_comp_inject(AxisID id, int8_t delta);

/**
 * @brief 创建一个达到目标速度的S曲线
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param dir 运动方向（AXIS_DIR_CCW或AXIS_DIR_CW）
 * @param start_f 起始频率（Hz）
 * @param target_f 目标频率（Hz）
 * @param jerk 加加速度（Hz/s^3），控制加速度变化率
 * @param a_max 最大加速度（Hz/s^2）
 * @param time_factor 时间因子，用于计算更新周期
 * @param next_curve 是否自动连接下一段曲线（0=否，1=是）
 * @return SCurve* 生成的S曲线指针，失败返回NULL
 * @note 用于速度模式运动，生成七段式S曲线实现平滑加减速
 */
SCurve *
xq_axis_new_curve_for_target_speed (AxisID id, AxisDir dir, 
                                     uint32_t start_f, uint32_t target_f, 
                                     uint32_t jerk, uint32_t a_max, uint32_t time_factor, uint8_t next_curve);

/**
 * @brief 改变轴的目标速度
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param dir 运动方向（AXIS_DIR_CCW或AXIS_DIR_CW）
 * @param target_f 新的目标频率（Hz）
 * @param jerk 加加速度（Hz/s^3）
 * @param a_max 最大加速度（Hz/s^2）
 * @param time_factor 时间因子
 * @return int8_t 0=成功，负数=失败
 * @note 在电机运行过程中动态改变目标速度
 */
int8_t
xq_axis_change_target_speed (AxisID id, AxisDir dir, uint32_t target_f, 
                              uint32_t jerk, uint32_t a_max, uint32_t time_factor);

/**
 * @brief 创建一个达到目标位置的S曲线
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param start_f 起始频率（Hz）
 * @param max_f 最大运行频率（Hz）
 * @param target_position 目标位置（脉冲数）
 * @param jerk 加加速度（Hz/s^3）
 * @param a_max 最大加速度（Hz/s^2）
 * @param time_factor 时间因子
 * @param pos_mode 位置模式（AXIS_POS_MODE_ABS=绝对位置，AXIS_POS_MODE_REL=相对位置）
 * @return SCurve* 生成的S曲线指针，失败返回NULL
 * @note 用于位置模式运动，自动规划加速、匀速、减速段
 */
SCurve *
xq_axis_new_curve_for_target_position (AxisID id, uint32_t start_f, uint32_t max_f, int32_t target_position, 
                                        uint32_t jerk, uint32_t a_max, uint32_t time_factor, AxisPosMode pos_mode);

/**
 * @brief 改变轴的目标位置
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param target_position 新的目标位置（脉冲数）
 * @param max_f 最大运行频率（Hz）
 * @param jerk 加加速度（Hz/s^3）
 * @param a_max 最大加速度（Hz/s^2）
 * @param time_factor 时间因子
 * @return int8_t 0=成功，负数=失败
 * @note 在电机运行过程中动态改变目标位置
 */
int8_t
xq_axis_change_target_position (AxisID id, int32_t target_position, uint32_t max_f, 
                                 uint32_t jerk, uint32_t a_max, uint32_t time_factor);

/**
 * @brief 点动运动（Jog模式）
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param speed 运动速度（mm/s），正值为正向，负值为反向
 * @param jerk 加加速度（mm/s^3）
 * @param a_max 最大加速度（mm/s^2）
 * @param time_factor 时间因子
 * @return int8_t 0=成功，负数=失败
 * @note 以指定速度持续运动，直到调用停止函数
 *       常用于手动调试和测试
 */
int8_t
XQ_JogMove (AxisID id, float_t speed, float_t jerk, float_t a_max, uint32_t time_factor);

/**
 * @brief 绝对位置运动
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param target_position 目标绝对位置（mm）
 * @param max_speed 最大运行速度（mm/s）
 * @param jerk 加加速度（mm/s^3）
 * @param a_max 最大加速度（mm/s^2）
 * @param time_factor 时间因子
 * @return int8_t 0=成功，负数=失败
 * @note 运动到指定的绝对位置坐标
 *       使用S曲线规划实现平滑运动
 */
int8_t
XQ_ABSMove (AxisID id, float_t target_position, float_t max_speed, float_t jerk, float_t a_max, uint32_t time_factor);

/**
 * @brief 停止轴运动
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param EMG 紧急停止标志（0=正常减速停止，1=紧急停止）
 * @param jerk 减速过程的加加速度（mm/s^3），EMG=0时有效
 * @param a_max 减速过程的最大加速度（mm/s^2），EMG=0时有效
 * @param time_factor 时间因子
 * @return int8_t 0=成功，负数=失败
 * @note EMG=0时使用S曲线平滑减速到停止
 *       EMG=1时立即停止PWM输出（急停）
 */
int8_t
XQ_Stop (AxisID id, uint8_t EMG, float_t jerk, float_t a_max, uint32_t time_factor);

/**
 * @brief 增量运动（步进模式）
 * @param id 轴编号（AXIS_0 ~ AXIS_15）
 * @param Incmm 增量距离（mm），正值为正向，负值为反向
 * @param ms 运动时间（毫秒）
 * @return int8_t 0=成功，负数=失败
 * @note 在指定时间内完成指定距离的相对运动
 *       速度由距离和时间自动计算
 */
int8_t
XQ_IncMoveStep (AxisID id, float_t Incmm, float_t ms);

/**
 * @brief 在指定时间内匀速发送指定数量的脉冲（带编码器闭环补偿）
 * @param id       轴编号（AXIS_0 ~ AXIS_15）
 * @param pulses   增量脉冲数（正=CCW, 负=CW）
 * @param time_ms  目标时间（毫秒）
 * @return int8_t  0=成功, -1=参数错误/频率越界, -2=曲线分配失败, -3=轴正在运动
 */
int8_t
xq_axis_pulse_timed(AxisID id, int32_t pulses, uint32_t time_ms);

#endif  

/* __XQ_AXIS_H__ */

