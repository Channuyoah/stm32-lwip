#ifndef __XQ_IO_H__
#define __XQ_IO_H__

#include "main.h"


// TCA9535 基础地址: 0100 000 -> 0x20
// TCA9535 I2C地址配置 (左移1位用于HAL库)
#define TCA9535_BASE_ADDR           0x20


// 7个TCA9535设备地址定义 (按A0 A1 A2顺序，地址计算: (BASE | A0 | A1<<1 | A2<<2) << 1)
#define TCA9535_ADDR_100            ((TCA9535_BASE_ADDR | 0x01) << 1)  // A0=1, A1=0, A2=0 -> 输出设备1 (0x42)
#define TCA9535_ADDR_010            ((TCA9535_BASE_ADDR | 0x02) << 1)  // A0=0, A1=1, A2=0 -> 输出设备2 (0x44)
#define TCA9535_ADDR_001            ((TCA9535_BASE_ADDR | 0x04) << 1)  // A0=0, A1=0, A2=1 -> 输入设备1 (0x48)
#define TCA9535_ADDR_011            ((TCA9535_BASE_ADDR | 0x06) << 1)  // A0=0, A1=1, A2=1 -> 输入设备2 (0x4C)
#define TCA9535_ADDR_110            ((TCA9535_BASE_ADDR | 0x03) << 1)  // A0=1, A1=1, A2=0 -> 输出设备3 (0x46)
#define TCA9535_ADDR_101            ((TCA9535_BASE_ADDR | 0x05) << 1)  // A0=1, A1=0, A2=1 -> 输入设备3 (0x4A)


// 设备索引定义
typedef enum {
    TCA9535_DEV_OUTPUT_1 = 0,  // 地址100 (A0=1,A1=0,A2=0) - 输出设备1 (OUT5-15)
    TCA9535_DEV_OUTPUT_2 = 1,  // 地址010 (A0=0,A1=1,A2=0) - 输出设备2 (OUT16-31)
    TCA9535_DEV_INPUT_1  = 2,  // 地址001 (A0=0,A1=0,A2=1) - 输入设备1 (IN0-15)
    TCA9535_DEV_INPUT_2  = 3,  // 地址011 (A0=0,A1=1,A2=1) - 输入设备2 (IN16-26)
    TCA9535_DEV_OUTPUT_3 = 4,  // 地址110 (A0=1,A1=1,A2=0) - 输出设备3 (OUT32-47, 全部输出)
    TCA9535_DEV_INPUT_3  = 5,  // 地址101 (A0=1,A1=0,A2=1) - 输入设备3 (IN32-47, 全部输入)
    TCA9535_DEV_COUNT    = 6
} TCA9535_Device_t;


// TCA9535寄存器地址
#define TCA9535_REG_INPUT_PORT0     0x00  // 输入端口0寄存器
#define TCA9535_REG_INPUT_PORT1     0x01  // 输入端口1寄存器
#define TCA9535_REG_OUTPUT_PORT0    0x02  // 输出端口0寄存器
#define TCA9535_REG_OUTPUT_PORT1    0x03  // 输出端口1寄存器
#define TCA9535_REG_POLARITY_PORT0  0x04  // 极性反转端口0寄存器
#define TCA9535_REG_POLARITY_PORT1  0x05  // 极性反转端口1寄存器
#define TCA9535_REG_CONFIG_PORT0    0x06  // 配置端口0寄存器 (1=输入, 0=输出)
#define TCA9535_REG_CONFIG_PORT1    0x07  // 配置端口1寄存器 (1=输入, 0=输出)


// TCA9535端口定义
typedef enum {
    TCA9535_PORT0 = 0,
    TCA9535_PORT1 = 1
} TCA9535_Port_t;


// TCA9535引脚定义
typedef enum {
    TCA9535_PIN_0 = 0x01,
    TCA9535_PIN_1 = 0x02,
    TCA9535_PIN_2 = 0x04,
    TCA9535_PIN_3 = 0x08,
    TCA9535_PIN_4 = 0x10,
    TCA9535_PIN_5 = 0x20,
    TCA9535_PIN_6 = 0x40,
    TCA9535_PIN_7 = 0x80,
    TCA9535_PIN_ALL = 0xFF
} TCA9535_Pin_t;

// TCA9535引脚方向
typedef enum {
    TCA9535_OUTPUT = 0,
    TCA9535_INPUT = 1
} TCA9535_Direction_t;

// TCA9535引脚状态
typedef enum {
    TCA9535_PIN_RESET = 0,
    TCA9535_PIN_SET = 1
} TCA9535_PinState_t;

// 输出引脚枚举定义 (XQ_OUT_PIN_0 到 XQ_OUT_PIN_47)
// PIN_0-4:   STM32 GPIO直接控制
// PIN_5-15:  TCA9535_DEV_OUTPUT_1 (PIN_5-7->Port0的5-7位, PIN_8-15->Port1的0-7位)
// PIN_16-31: TCA9535_DEV_OUTPUT_2 (PIN_16-23->Port0的0-7位, PIN_24-31->Port1的0-7位)
// PIN_32-47: TCA9535_DEV_OUTPUT_3 (PIN_32-39->Port0的0-7位, PIN_40-47->Port1的0-7位)
typedef enum {
    XQ_OUT_PIN_0 = 0,   // STM32 GPIO (XQ_OUT_0)
    XQ_OUT_PIN_1 = 1,   // STM32 GPIO (XQ_OUT_1)
    XQ_OUT_PIN_2 = 2,   // STM32 GPIO (XQ_OUT_2)
    XQ_OUT_PIN_3 = 3,   // STM32 GPIO (XQ_OUT_3)
    XQ_OUT_PIN_4 = 4,   // STM32 GPIO (XQ_OUT_4)
    XQ_OUT_PIN_5 = 5,   // TCA9535_DEV_OUTPUT_1, Port0, Bit5
    XQ_OUT_PIN_6 = 6,   // TCA9535_DEV_OUTPUT_1, Port0, Bit6
    XQ_OUT_PIN_7 = 7,   // TCA9535_DEV_OUTPUT_1, Port0, Bit7
    XQ_OUT_PIN_8 = 8,   // TCA9535_DEV_OUTPUT_1, Port1, Bit0
    XQ_OUT_PIN_9 = 9,   // TCA9535_DEV_OUTPUT_1, Port1, Bit1
    XQ_OUT_PIN_10 = 10, // TCA9535_DEV_OUTPUT_1, Port1, Bit2
    XQ_OUT_PIN_11 = 11, // TCA9535_DEV_OUTPUT_1, Port1, Bit3
    XQ_OUT_PIN_12 = 12, // TCA9535_DEV_OUTPUT_1, Port1, Bit4
    XQ_OUT_PIN_13 = 13, // TCA9535_DEV_OUTPUT_1, Port1, Bit5
    XQ_OUT_PIN_14 = 14, // TCA9535_DEV_OUTPUT_1, Port1, Bit6
    XQ_OUT_PIN_15 = 15, // TCA9535_DEV_OUTPUT_1, Port1, Bit7
    XQ_OUT_PIN_16 = 16, // TCA9535_DEV_OUTPUT_2, Port0, Bit0
    XQ_OUT_PIN_17 = 17, // TCA9535_DEV_OUTPUT_2, Port0, Bit1
    XQ_OUT_PIN_18 = 18, // TCA9535_DEV_OUTPUT_2, Port0, Bit2
    XQ_OUT_PIN_19 = 19, // TCA9535_DEV_OUTPUT_2, Port0, Bit3
    XQ_OUT_PIN_20 = 20, // TCA9535_DEV_OUTPUT_2, Port0, Bit4
    XQ_OUT_PIN_21 = 21, // TCA9535_DEV_OUTPUT_2, Port0, Bit5
    XQ_OUT_PIN_22 = 22, // TCA9535_DEV_OUTPUT_2, Port0, Bit6
    XQ_OUT_PIN_23 = 23, // TCA9535_DEV_OUTPUT_2, Port0, Bit7
    XQ_OUT_PIN_24 = 24, // TCA9535_DEV_OUTPUT_2, Port1, Bit0
    XQ_OUT_PIN_25 = 25, // TCA9535_DEV_OUTPUT_2, Port1, Bit1
    XQ_OUT_PIN_26 = 26, // TCA9535_DEV_OUTPUT_2, Port1, Bit2
    XQ_OUT_PIN_27 = 27, // TCA9535_DEV_OUTPUT_2, Port1, Bit3
    XQ_OUT_PIN_28 = 28, // TCA9535_DEV_OUTPUT_2, Port1, Bit4
    XQ_OUT_PIN_29 = 29, // TCA9535_DEV_OUTPUT_2, Port1, Bit5
    XQ_OUT_PIN_30 = 30, // TCA9535_DEV_OUTPUT_2, Port1, Bit6
    XQ_OUT_PIN_31 = 31, // TCA9535_DEV_OUTPUT_2, Port1, Bit7
    XQ_OUT_PIN_32 = 32, // TCA9535_DEV_OUTPUT_3, Port0, Bit0 (P00)
    XQ_OUT_PIN_33 = 33, // TCA9535_DEV_OUTPUT_3, Port0, Bit1 (P01)
    XQ_OUT_PIN_34 = 34, // TCA9535_DEV_OUTPUT_3, Port0, Bit2 (P02)
    XQ_OUT_PIN_35 = 35, // TCA9535_DEV_OUTPUT_3, Port0, Bit3 (P03)
    XQ_OUT_PIN_36 = 36, // TCA9535_DEV_OUTPUT_3, Port0, Bit4 (P04)
    XQ_OUT_PIN_37 = 37, // TCA9535_DEV_OUTPUT_3, Port0, Bit5 (P05)
    XQ_OUT_PIN_38 = 38, // TCA9535_DEV_OUTPUT_3, Port0, Bit6 (P06)
    XQ_OUT_PIN_39 = 39, // TCA9535_DEV_OUTPUT_3, Port0, Bit7 (P07)
    XQ_OUT_PIN_40 = 40, // TCA9535_DEV_OUTPUT_3, Port1, Bit0 (P10)
    XQ_OUT_PIN_41 = 41, // TCA9535_DEV_OUTPUT_3, Port1, Bit1 (P11)
    XQ_OUT_PIN_42 = 42, // TCA9535_DEV_OUTPUT_3, Port1, Bit2 (P12)
    XQ_OUT_PIN_43 = 43, // TCA9535_DEV_OUTPUT_3, Port1, Bit3 (P13)
    XQ_OUT_PIN_44 = 44, // TCA9535_DEV_OUTPUT_3, Port1, Bit4 (P14)
    XQ_OUT_PIN_45 = 45, // TCA9535_DEV_OUTPUT_3, Port1, Bit5 (P15)
    XQ_OUT_PIN_46 = 46, // TCA9535_DEV_OUTPUT_3, Port1, Bit6 (P16)
    XQ_OUT_PIN_47 = 47, // TCA9535_DEV_OUTPUT_3, Port1, Bit7 (P17)
    XQ_OUT_PIN_COUNT = 48
} XQ_OutputPin_t;

// 输入引脚枚举定义 (XQ_IN_PIN_0 到 XQ_IN_PIN_47)
// PIN_0-15:  TCA9535_DEV_INPUT_1 (PIN_0-7->Port0的0-7位, PIN_8-15->Port1的0-7位)
// PIN_16-26: TCA9535_DEV_INPUT_2 (PIN_16-23->Port0的0-7位, PIN_24-26->Port1的0-2位)
// PIN_27-31: STM32 GPIO直接控制 (XQ_IN_27 到 XQ_IN_31)
// PIN_32-39: TCA9535_DEV_INPUT_3 Port0 (P07=IN32, P06=IN33, ..., P00=IN39 - 字节内全部反序)
// PIN_40-47: TCA9535_DEV_INPUT_3 Port1 (P17=IN40, P16=IN41, ..., P10=IN47 - 字节内全部反序)
typedef enum {
    XQ_IN_PIN_0 = 0,   // TCA9535_DEV_INPUT_1, Port0, Bit0
    XQ_IN_PIN_1 = 1,   // TCA9535_DEV_INPUT_1, Port0, Bit1
    XQ_IN_PIN_2 = 2,   // TCA9535_DEV_INPUT_1, Port0, Bit2
    XQ_IN_PIN_3 = 3,   // TCA9535_DEV_INPUT_1, Port0, Bit3
    XQ_IN_PIN_4 = 4,   // TCA9535_DEV_INPUT_1, Port0, Bit4
    XQ_IN_PIN_5 = 5,   // TCA9535_DEV_INPUT_1, Port0, Bit5
    XQ_IN_PIN_6 = 6,   // TCA9535_DEV_INPUT_1, Port0, Bit6
    XQ_IN_PIN_7 = 7,   // TCA9535_DEV_INPUT_1, Port0, Bit7
    XQ_IN_PIN_8 = 8,   // TCA9535_DEV_INPUT_1, Port1, Bit0
    XQ_IN_PIN_9 = 9,   // TCA9535_DEV_INPUT_1, Port1, Bit1
    XQ_IN_PIN_10 = 10, // TCA9535_DEV_INPUT_1, Port1, Bit2
    XQ_IN_PIN_11 = 11, // TCA9535_DEV_INPUT_1, Port1, Bit3
    XQ_IN_PIN_12 = 12, // TCA9535_DEV_INPUT_1, Port1, Bit4
    XQ_IN_PIN_13 = 13, // TCA9535_DEV_INPUT_1, Port1, Bit5
    XQ_IN_PIN_14 = 14, // TCA9535_DEV_INPUT_1, Port1, Bit6
    XQ_IN_PIN_15 = 15, // TCA9535_DEV_INPUT_1, Port1, Bit7
    XQ_IN_PIN_16 = 16, // TCA9535_DEV_INPUT_2, Port0, Bit0
    XQ_IN_PIN_17 = 17, // TCA9535_DEV_INPUT_2, Port0, Bit1
    XQ_IN_PIN_18 = 18, // TCA9535_DEV_INPUT_2, Port0, Bit2
    XQ_IN_PIN_19 = 19, // TCA9535_DEV_INPUT_2, Port0, Bit3
    XQ_IN_PIN_20 = 20, // TCA9535_DEV_INPUT_2, Port0, Bit4
    XQ_IN_PIN_21 = 21, // TCA9535_DEV_INPUT_2, Port0, Bit5
    XQ_IN_PIN_22 = 22, // TCA9535_DEV_INPUT_2, Port0, Bit6
    XQ_IN_PIN_23 = 23, // TCA9535_DEV_INPUT_2, Port0, Bit7
    XQ_IN_PIN_24 = 24, // TCA9535_DEV_INPUT_2, Port1, Bit0
    XQ_IN_PIN_25 = 25, // TCA9535_DEV_INPUT_2, Port1, Bit1
    XQ_IN_PIN_26 = 26, // TCA9535_DEV_INPUT_2, Port1, Bit2
    XQ_IN_PIN_27 = 27, // STM32 GPIO (XQ_IN_27)
    XQ_IN_PIN_28 = 28, // STM32 GPIO (XQ_IN_28)
    XQ_IN_PIN_29 = 29, // STM32 GPIO (XQ_IN_29)
    XQ_IN_PIN_30 = 30, // STM32 GPIO (XQ_IN_30)
    XQ_IN_PIN_31 = 31, // STM32 GPIO (XQ_IN_31)
    XQ_IN_PIN_32 = 32, // TCA9535_DEV_INPUT_3, Port0, Bit7 (P07)
    XQ_IN_PIN_33 = 33, // TCA9535_DEV_INPUT_3, Port0, Bit6 (P06)
    XQ_IN_PIN_34 = 34, // TCA9535_DEV_INPUT_3, Port0, Bit5 (P05)
    XQ_IN_PIN_35 = 35, // TCA9535_DEV_INPUT_3, Port0, Bit4 (P04)
    XQ_IN_PIN_36 = 36, // TCA9535_DEV_INPUT_3, Port0, Bit3 (P03)
    XQ_IN_PIN_37 = 37, // TCA9535_DEV_INPUT_3, Port0, Bit2 (P02)
    XQ_IN_PIN_38 = 38, // TCA9535_DEV_INPUT_3, Port0, Bit1 (P01)
    XQ_IN_PIN_39 = 39, // TCA9535_DEV_INPUT_3, Port0, Bit0 (P00)
    XQ_IN_PIN_40 = 40, // TCA9535_DEV_INPUT_3, Port1, Bit7 (P17)
    XQ_IN_PIN_41 = 41, // TCA9535_DEV_INPUT_3, Port1, Bit6 (P16)
    XQ_IN_PIN_42 = 42, // TCA9535_DEV_INPUT_3, Port1, Bit5 (P15)
    XQ_IN_PIN_43 = 43, // TCA9535_DEV_INPUT_3, Port1, Bit4 (P14)
    XQ_IN_PIN_44 = 44, // TCA9535_DEV_INPUT_3, Port1, Bit3 (P13)
    XQ_IN_PIN_45 = 45, // TCA9535_DEV_INPUT_3, Port1, Bit2 (P12)
    XQ_IN_PIN_46 = 46, // TCA9535_DEV_INPUT_3, Port1, Bit1 (P11)
    XQ_IN_PIN_47 = 47, // TCA9535_DEV_INPUT_3, Port1, Bit0 (P10)
    XQ_IN_PIN_COUNT = 48
} XQ_InputPin_t;


extern uint64_t xqIO_Input; /* 48个IO输入状态 */
extern uint64_t xqIO_Output; /* 48个IO输出状态 */

// 函数声明
HAL_StatusTypeDef XQ_TCA9535_Init_All(void);

// 64位输入输出操作
HAL_StatusTypeDef XQ_GetDI (uint64_t *gDI);
HAL_StatusTypeDef XQ_SetDO (uint64_t gDO);

// 单个引脚操作函数
HAL_StatusTypeDef XQ_SetOutputPin(XQ_OutputPin_t pin, uint8_t state);  // 设置单个输出引脚
uint8_t XQ_GetInputPin(XQ_InputPin_t pin);                             // 读取单个输入引脚 (返回: 0/1)


#endif // !__XQ_IO_H__
