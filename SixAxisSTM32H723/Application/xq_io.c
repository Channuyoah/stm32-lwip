#include "xq_io.h"
#include "i2c.h"
#include "usart.h"
#include "cmsis_os2.h"
#include "xq_modbus.h"
#include <stdio.h>


uint64_t xqIO_Input = 0; /* 48个IO输入状态 */
uint64_t xqIO_Output = 0; /* 48个IO输出状态 */
static uint64_t xqIO_Output_Last = 0; /* 上一次的输出状态（初始值为0） */

osThreadId_t ioRefreshTaskHandle;
const osThreadAttr_t ioRefreshTask_attributes = {
  .name = "ioRefreshTsk",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};


// GPIO引脚定义结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} GPIO_Pin_Config_t;

// STM32 GPIO输出引脚配置表 (XQ_OUT_0 到 XQ_OUT_4)
static const GPIO_Pin_Config_t STM32_Output_Pins[5] = {
    {XQ_OUT_0_GPIO_Port, XQ_OUT_0_Pin},  // XQ_OUT_0
    {XQ_OUT_1_GPIO_Port, XQ_OUT_1_Pin},  // XQ_OUT_1
    {XQ_OUT_2_GPIO_Port, XQ_OUT_2_Pin},  // XQ_OUT_2
    {XQ_OUT_3_GPIO_Port, XQ_OUT_3_Pin},  // XQ_OUT_3
    {XQ_OUT_4_GPIO_Port, XQ_OUT_4_Pin}   // XQ_OUT_4
};

// STM32 GPIO输入引脚配置表 (XQ_IN_27 到 XQ_IN_31)
static const GPIO_Pin_Config_t STM32_Input_Pins[5] = {
    {XQ_IN_27_GPIO_Port, XQ_IN_27_Pin},  // XQ_IN_27 (引脚27)
    {XQ_IN_28_GPIO_Port, XQ_IN_28_Pin},  // XQ_IN_28 (引脚28)
    {XQ_IN_29_GPIO_Port, XQ_IN_29_Pin},  // XQ_IN_29 (引脚29)
    {XQ_IN_30_GPIO_Port, XQ_IN_30_Pin},  // XQ_IN_30 (引脚30)
    {XQ_IN_31_GPIO_Port, XQ_IN_31_Pin}   // XQ_IN_31 (引脚31)
};


// TCA9535 device address array  
static const uint16_t TCA9535_Addresses[TCA9535_DEV_COUNT] = {
    TCA9535_ADDR_100,  // Output device 1 (OUT5-15)
    TCA9535_ADDR_010,  // Output device 2 (OUT16-31)
    TCA9535_ADDR_001,  // Input device 1  (IN0-15)
    TCA9535_ADDR_011,  // Input device 2  (IN16-26)
    TCA9535_ADDR_110,  // Output device 3 (OUT32-47)
    TCA9535_ADDR_101   // Input device 3  (IN32-47)
};

// Device name array (for debugging)
static const char* TCA9535_DeviceNames[TCA9535_DEV_COUNT] = {
    "OUTPUT_1(A0A1A2=100)",
    "OUTPUT_2(A0A1A2=010)", 
    "INPUT_1(A0A1A2=001)",
    "INPUT_2(A0A1A2=011)",
    "OUTPUT_3(A0A1A2=110)",
    "INPUT_3(A0A1A2=101)"
};


void XQ_IO_Refresh_Task(void *argument);

/**
 * @brief 向TCA9535写入寄存器
 * @param device: 设备索引
 * @param reg: 寄存器地址
 * @param data: 要写入的数据
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_WriteReg(TCA9535_Device_t device, uint8_t reg, uint8_t data) {

  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, TCA9535_Addresses[device], reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 1000);
  
  if (status != HAL_OK) {
      printf("TCA9535 %s write reg 0x%02X failed: %d\r\n", TCA9535_DeviceNames[device], reg, status);
  }
  
  return status;
}

/**
 * @brief 从TCA9535读取寄存器
 * @param device: 设备索引
 * @param reg: 寄存器地址
 * @param data: 读取的数据指针
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_ReadReg(TCA9535_Device_t device, uint8_t reg, uint8_t* data) {
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, TCA9535_Addresses[device], reg, I2C_MEMADD_SIZE_8BIT, data, 1, 1000);
  
  if (status != HAL_OK) {
      printf("TCA9535 %s read reg 0x%02X failed: %d\r\n", TCA9535_DeviceNames[device], reg, status);
  }
  
  return status;
}


/**
 * @brief 初始化单个TCA9535设备
 * @param device: 设备索引
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_Init_Device(TCA9535_Device_t device) {

  HAL_StatusTypeDef status;

  // 检查设备是否在线
  status = HAL_I2C_IsDeviceReady(&hi2c1, TCA9535_Addresses[device], 3, 1000);
  if (status != HAL_OK) {
      printf("TCA9535 %s device not responding!\r\n", TCA9535_DeviceNames[device]);
      return status;
  }
  
  if (device == TCA9535_DEV_OUTPUT_1 || device == TCA9535_DEV_OUTPUT_2 || device == TCA9535_DEV_OUTPUT_3) {
    // 输出设备配置：所有引脚为输出 (0=输出)
    status = TCA9535_WriteReg(device, TCA9535_REG_CONFIG_PORT0, 0x00);
    if (status != HAL_OK) return status;
    
    status = TCA9535_WriteReg(device, TCA9535_REG_CONFIG_PORT1, 0x00);
    if (status != HAL_OK) return status;
    
    // 初始化输出为高电平
    status = TCA9535_WriteReg(device, TCA9535_REG_OUTPUT_PORT0, 0xFF);
    if (status != HAL_OK) return status;

    status = TCA9535_WriteReg(device, TCA9535_REG_OUTPUT_PORT1, 0xFF);
    if (status != HAL_OK) return status;
      
  } else {
    // 输入设备配置 (INPUT_1, INPUT_2, INPUT_3)。配置所有引脚为输入 (1=输入)
    status = TCA9535_WriteReg(device, TCA9535_REG_CONFIG_PORT0, 0xFF);
    if (status != HAL_OK) return status;
    
    status = TCA9535_WriteReg(device, TCA9535_REG_CONFIG_PORT1, 0xFF);
    if (status != HAL_OK) return status;
    
    // 设置极性为正常 (不反转)
    status = TCA9535_WriteReg(device, TCA9535_REG_POLARITY_PORT0, 0x00);
    if (status != HAL_OK) return status;
    
    status = TCA9535_WriteReg(device, TCA9535_REG_POLARITY_PORT1, 0x00);
    if (status != HAL_OK) return status;
  }
  
  return HAL_OK;
}


/**
 * @brief 写端口输出值
 * @param device: 设备索引
 * @param port: 端口号
 * @param value: 输出值
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_WritePort(TCA9535_Device_t device, TCA9535_Port_t port, uint8_t value)
{
  uint8_t reg = (port == TCA9535_PORT0) ? TCA9535_REG_OUTPUT_PORT0 : TCA9535_REG_OUTPUT_PORT1;
  return TCA9535_WriteReg(device, reg, value);
}

/**
 * @brief 读取端口输入值
 * @param device: 设备索引
 * @param port: 端口号
 * @param value: 读取值指针
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_ReadPort(TCA9535_Device_t device, TCA9535_Port_t port, uint8_t* value)
{
  uint8_t reg = (port == TCA9535_PORT0) ? TCA9535_REG_INPUT_PORT0 : TCA9535_REG_INPUT_PORT1;
  return TCA9535_ReadReg(device, reg, value);
}


/**
 * @brief 写16位输出值(两个端口合并)
 * @param device: 设备索引
 * @param value: 16位输出值 (高8位->Port1, 低8位->Port0)
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_WriteOutputs_16bit(TCA9535_Device_t device, uint16_t value) {

  HAL_StatusTypeDef status;
  uint8_t port0_value = value & 0xFF;         // 低8位给Port0
  uint8_t port1_value = (value >> 8) & 0xFF;  // 高8位给Port1
  
  // 写Port0
  status = TCA9535_WritePort(device, TCA9535_PORT0, port0_value);
  if (status != HAL_OK) return status;
  
  // 写Port1
  status = TCA9535_WritePort(device, TCA9535_PORT1, port1_value);
  if (status != HAL_OK) return status;
  
  return HAL_OK;
}

/**
 * @brief 读16位输入值(两个端口合并)
 * @param device: 设备索引
 * @param value: 16位输入值指针 (高8位<-Port1, 低8位<-Port0)
 * @return HAL状态
 */
static HAL_StatusTypeDef 
TCA9535_ReadInputs_16bit(TCA9535_Device_t device, uint16_t* value) {

  HAL_StatusTypeDef status;
  uint8_t port0_value, port1_value;
  
  if (value == NULL) return HAL_ERROR;
  
  // 读Port0
  status = TCA9535_ReadPort(device, TCA9535_PORT0, &port0_value);
  if (status != HAL_OK) return status;
  
  // 读Port1
  status = TCA9535_ReadPort(device, TCA9535_PORT1, &port1_value);
  if (status != HAL_OK) return status;
  
  // 合并为16位值 (Port1在高8位，Port0在低8位)
  *value = ((uint16_t)port1_value << 8) | port0_value;
  
  return HAL_OK;
}


/**
 * @brief 初始化所有TCA9535设备
 * @return HAL状态
 */
HAL_StatusTypeDef 
XQ_TCA9535_Init_All(void) {
  HAL_StatusTypeDef status;

  for (int i = 0; i < TCA9535_DEV_COUNT; i++) {
    status = TCA9535_Init_Device((TCA9535_Device_t)i);
    if (status != HAL_OK) {
        printf("TCA9535 device %d init failed!\r\n", i);
        return status;
    }
    HAL_Delay(1);  // 设备间延时
  }

  XQ_SetDO (xqIO_Output);  // 初始化输出全低电平
  XQ_GetDI (&xqIO_Input);  // 读取初始输入状态

  /* 创建IO刷新线程 */
  ioRefreshTaskHandle = osThreadNew(XQ_IO_Refresh_Task, NULL, &ioRefreshTask_attributes);

  return HAL_OK;
}


/**
 * @brief 设置单个输出引脚状态 (内部函数)
 * @param pin: 引脚编号 (0-48)
 * @param state: 引脚状态 (0=高电平, 1=低电平)
 * @return HAL状态
 */
static HAL_StatusTypeDef 
XQ_SetOutputPin_Internal(uint8_t pin, uint8_t state)
{
  if (pin > 47) return HAL_ERROR;
  
  // 处理前5个引脚 (0-4) - 使用STM32 GPIO
  if (pin < 5) {
    // state=1时输出低电平，state=0时输出高电平
    GPIO_PinState gpio_state = state ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(STM32_Output_Pins[pin].port, STM32_Output_Pins[pin].pin, gpio_state);
    return HAL_OK;
  }
  
  // 处理引脚5-48 - 使用TCA9535
  TCA9535_Device_t device;
  TCA9535_Port_t port;
  uint8_t pin_mask;
  
  if (pin <= 15) {
    // TCA9535设备1 (引脚5-15)
    device = TCA9535_DEV_OUTPUT_1;
    if (pin <= 7) {
        // 引脚5-7映射到Port0的5-7位
        port = TCA9535_PORT0;
        pin_mask = (1 << pin);
    } else {
        // 引脚8-15映射到Port1的0-7位
        port = TCA9535_PORT1;
        pin_mask = (1 << (pin - 8));
    }
  } else if (pin <= 31) {
    // TCA9535设备2 (引脚16-31)
    device = TCA9535_DEV_OUTPUT_2;
    if (pin <= 23) {
        // 引脚16-23映射到Port0的0-7位
        port = TCA9535_PORT0;
        pin_mask = (1 << (pin - 16));
    } else {
        // 引脚24-31映射到Port1的0-7位
        port = TCA9535_PORT1;
        pin_mask = (1 << (pin - 24));
    }
  } else if (pin <= 39) {
    // OUT32-39: OUTPUT_3 Port0 Bit0-7 (P00-P07)
    device = TCA9535_DEV_OUTPUT_3;
    port = TCA9535_PORT0;
    pin_mask = (1 << (pin - 32));
  } else {
    // OUT40-47: OUTPUT_3 Port1 Bit0-7 (P10-P17)
    device = TCA9535_DEV_OUTPUT_3;
    port = TCA9535_PORT1;
    pin_mask = (1 << (pin - 40));
  }
  
  // 读取当前端口值
  uint8_t current_value;
  HAL_StatusTypeDef status = TCA9535_ReadPort(device, port, &current_value);
  if (status != HAL_OK) return status;
  
  // 修改指定引脚 (state=1时输出低电平，state=0时输出高电平)
  if (state) {
    current_value &= ~pin_mask;  // state=1: 设置为低电平
  } else {
    current_value |= pin_mask;   // state=0: 设置为高电平
  }
  
  // 写回端口
  return TCA9535_WritePort(device, port, current_value);
}

/**
 * @brief 读取单个输入引脚状态 (内部函数)
 * @param pin: 引脚编号 (0-55)
 * @return 引脚状态 (0=高电平, 1=低电平, 0xFF=错误)
 */
static uint8_t 
XQ_GetInputPin_Internal(uint8_t pin)
{
  if (pin > 47) return 0xFF;
  
  // 处理扩展引脚 (32-47)
  if (pin >= 32) {
    TCA9535_Device_t device;
    TCA9535_Port_t port;
    uint8_t pin_mask;
    
    if (pin <= 39) {
      // INPUT_3 Port0: P07=IN32, P06=IN33, P05=IN34, P04=IN35, P03=IN36, P02=IN37, P01=IN38, P00=IN39 (字节内全部反序)
      device = TCA9535_DEV_INPUT_3;
      port = TCA9535_PORT0;
      pin_mask = (1 << (39 - pin));  // pin32->bit7, pin33->bit6, ...
    } else {
      // INPUT_3 Port1: P17=IN40, P16=IN41, P15=IN42, P14=IN43, P13=IN44, P12=IN45, P11=IN46, P10=IN47 (字节内全部反序)
      device = TCA9535_DEV_INPUT_3;
      port = TCA9535_PORT1;
      pin_mask = (1 << (47 - pin));  // pin40->bit7, pin41->bit6, ...
    }
    
    uint8_t port_value;
    HAL_StatusTypeDef status = TCA9535_ReadPort(device, port, &port_value);
    if (status != HAL_OK) return 0xFF;
    return (port_value & pin_mask) ? 1 : 0;
  }
  
  // 引脚编号位序反转：0-31反转成31-0
  uint8_t reversed_pin = 31 - pin;
  
  // 处理引脚27-31 - 使用STM32 GPIO
  if (reversed_pin >= 27 && reversed_pin <= 31) {
    GPIO_PinState gpio_state = HAL_GPIO_ReadPin(STM32_Input_Pins[reversed_pin - 27].port, STM32_Input_Pins[reversed_pin - 27].pin);
    return (gpio_state == GPIO_PIN_SET) ? 1 : 0;
  }
  
  // 处理引脚0-26 - 使用TCA9535
  TCA9535_Device_t device;
  TCA9535_Port_t port;
  uint8_t pin_mask;
  
  // 确定设备、端口和引脚掩码
  if (reversed_pin < 16) {
    // 输入设备1 (PIN 0-15)
    device = TCA9535_DEV_INPUT_1;
    if (reversed_pin < 8) {
        port = TCA9535_PORT0;
        pin_mask = (1 << reversed_pin);
    } else {
        port = TCA9535_PORT1;
        pin_mask = (1 << (reversed_pin - 8));
    }
  } else {
    // 输入设备2 (PIN 16-26)
    device = TCA9535_DEV_INPUT_2;
    if (reversed_pin < 24) {
      port = TCA9535_PORT0;
      pin_mask = (1 << (reversed_pin - 16));
    } else {
      port = TCA9535_PORT1;
      pin_mask = (1 << (reversed_pin - 24));
    }
  }
  
  // 读取端口值
  uint8_t port_value;
  HAL_StatusTypeDef status = TCA9535_ReadPort(device, port, &port_value);
  
  if (status != HAL_OK) return 0xFF;
  
  return (port_value & pin_mask) ? 1 : 0;
}



/**
 * @brief 读取所有64位数字输入状态 (混合控制架构)
 * @param gDI: 64位输入状态指针，每个bit对应一个引脚
 * @return HAL状态
 * 
 * 引脚映射说明:
 * - 引脚0-15:  TCA9535_DEV_INPUT_1 (I2C地址: 0x48)
 * - 引脚16-26: TCA9535_DEV_INPUT_2 (I2C地址: 0x4C)
 * - 引脚27-31: STM32 GPIO直接控制
 * - 引脚32-47: TCA9535_DEV_INPUT_3 (I2C地址: 0x4A, P07=IN32, P06=IN33, ..., P00=IN39, P17=IN40, P16=IN41, ..., P10=IN47)
 */
HAL_StatusTypeDef
XQ_GetDI (uint64_t *gDI) {
  uint16_t di1 = 0;
  uint16_t di2 = 0;
  uint32_t result32 = 0;
  uint32_t reversed_result32 = 0;
  uint16_t input3_16 = 0;
  uint64_t result64 = 0;

  // 读取TCA9535输入设备 (引脚0-26)
  TCA9535_ReadInputs_16bit (TCA9535_DEV_INPUT_1, &di1);
  TCA9535_ReadInputs_16bit (TCA9535_DEV_INPUT_2, &di2);

  // 合并TCA9535数据 (引脚0-26)
  result32 = ((uint32_t)di2 << 16) | di1;

  // 清除引脚27-31的位，准备填入STM32 GPIO数据
  result32 &= 0x07FFFFFF;

  // 读取STM32 GPIO输入引脚 (引脚27-31)
  for (int i = 0; i < 5; i++) {
    GPIO_PinState gpio_state = HAL_GPIO_ReadPin(STM32_Input_Pins[i].port, STM32_Input_Pins[i].pin);
    if (gpio_state == GPIO_PIN_SET) {
      result32 |= (1UL << (27 + i));
    }
  }

  // 位序反转：将前32位反转成逻辑引脚顺序
  for (int i = 0; i < 32; i++) {
    if (result32 & (1UL << i)) {
      reversed_result32 |= (1UL << (31 - i));
    }
  }
  result64 = reversed_result32;

  // 读取INPUT_3全部16位，对每字节做全部bit反序：P07=IN32, ..., P00=IN39, P17=IN40, ..., P10=IN47
  TCA9535_ReadInputs_16bit (TCA9535_DEV_INPUT_3, &input3_16);
  uint8_t port0 = (uint8_t)(input3_16 & 0xFF);
  uint8_t port1 = (uint8_t)((input3_16 >> 8) & 0xFF);
  uint8_t port0_rev = 0, port1_rev = 0;
  for (int i = 0; i < 8; i++) {
    if (port0 & (1 << i)) port0_rev |= (uint8_t)(1 << (7 - i));
    if (port1 & (1 << i)) port1_rev |= (uint8_t)(1 << (7 - i));
  }
  result64 |= ((uint64_t)port0_rev << 32);
  result64 |= ((uint64_t)port1_rev << 40);

  *gDI = result64;

  return HAL_OK;
}


/**
 * @brief 只读取STM32本地GPIO输入状态（引脚0-4）
 * @param gDI: 64位输入状态指针，只更新bit0-4（对应引脚27-31），其他位保持不变
 * @return HAL状态
 */
static HAL_StatusTypeDef
XQ_GetDI_STM32Only(uint64_t *gDI) {
  
  uint32_t result = 0;
  uint32_t reversed_result = 0;
  
  // 读取STM32 GPIO输入引脚 (引脚27-31)
  for (int i = 0; i < 5; i++) {
    GPIO_PinState gpio_state = HAL_GPIO_ReadPin(STM32_Input_Pins[i].port, STM32_Input_Pins[i].pin);
    if (gpio_state == GPIO_PIN_SET) {
      result |= (1UL << (27 + i));
    }
  }
  
  // 位序反转
  for (int i = 0; i < 32; i++) {
    if (result & (1UL << i)) {
      reversed_result |= (1UL << (31 - i));
    }
  }
  
  // 只更新gDI的bit0-4（对应反转后的引脚27-31），保留其他位不变
  *gDI = (*gDI & 0xFFFFFFFFFFFFFFE0ULL) | (reversed_result & 0x0000001F);
  
  return HAL_OK;
}

/**
 * @brief 设置所有64位数字输出状态 (混合控制架构)
 * @param gDO: 64位输出状态，每个bit对应一个引脚 (逻辑反转：0=高电平, 1=低电平)
 * @return HAL状态
 * 
 * 引脚映射说明:
 * - 引脚0-4:   STM32 GPIO (OUT0-4)
 * - 引脚5-15:  TCA9535_DEV_OUTPUT_1 (OUT5-15)
 * - 引脚16-31: TCA9535_DEV_OUTPUT_2 (OUT16-31)
 * - 引脚32-47: TCA9535_DEV_OUTPUT_3 (OUT32-47)
 * 
 * 注意：所有输出引脚都采用逻辑反转，bit=1输出低电平，bit=0输出高电平
 */
HAL_StatusTypeDef
XQ_SetDO (uint64_t gDO) {

  HAL_StatusTypeDef status;
  
  // 处理前5个引脚 (0-4) - 使用STM32 GPIO (应用逻辑反转)
  for (int i = 0; i < 5; i++) {
      // 逻辑反转：bit=1时输出低电平，bit=0时输出高电平
      GPIO_PinState state = ((gDO >> i) & 0x01) ? GPIO_PIN_RESET : GPIO_PIN_SET;
      HAL_GPIO_WritePin(STM32_Output_Pins[i].port, STM32_Output_Pins[i].pin, state);
  }
  
  // 处理TCA9535设备1 (引脚5-15)
  // 引脚5-7映射到Port0的5-7位，引脚8-15映射到Port1的0-7位
  uint16_t dev1_value = 0;
  for (int i = 5; i <= 15; i++) {
    // 逻辑反转：bit=0时设置硬件bit为1(高电平)，bit=1时设置硬件bit为0(低电平)
    if (((gDO >> i) & 0x01) == 0) {  // 当gDO的bit为0时，输出高电平
      if (i <= 7) {
        // Port0的5-7位
        dev1_value |= (1 << i);
      } else {
        // Port1的0-7位
        dev1_value |= (1 << (i - 8 + 8));  // 左移8位到高字节
      }
    }
  }
  status = TCA9535_WriteOutputs_16bit(TCA9535_DEV_OUTPUT_1, dev1_value);
  if (status != HAL_OK) return status;
  
  // 处理TCA9535设备2 (引脚16-31) (应用逻辑反转)
  uint16_t gDO_16_31 = (uint16_t)((gDO >> 16) & 0xFFFF);
  uint16_t dev2_value = ~gDO_16_31;
  status = TCA9535_WriteOutputs_16bit(TCA9535_DEV_OUTPUT_2, dev2_value);
  if (status != HAL_OK) return status;
  
  // 处理OUT32-OUT47 - OUTPUT_3 (引脚32-47) (应用逻辑反转)
  uint16_t gDO_32_47 = (uint16_t)((gDO >> 32) & 0xFFFF);
  uint16_t dev3_value = ~gDO_32_47;
  status = TCA9535_WriteOutputs_16bit(TCA9535_DEV_OUTPUT_3, dev3_value);
  if (status != HAL_OK) return status;
  
  return HAL_OK;
}


/**
 * @brief 设置单个输出引脚状态 (枚举版本)
 * @param pin: 输出引脚枚举
 * @param state: 引脚状态 (0=低电平, 1=高电平)
 * @return HAL状态
 */
HAL_StatusTypeDef XQ_SetOutputPin(XQ_OutputPin_t pin, uint8_t state)
{
  return XQ_SetOutputPin_Internal((uint8_t)pin, state);
}


/**
 * @brief 读取单个输入引脚状态 (枚举版本)
 * @param pin: 输入引脚枚举
 * @return 引脚状态 (0=低电平, 1=高电平, 0xFF=错误)
 */
uint8_t XQ_GetInputPin(XQ_InputPin_t pin)
{
  return XQ_GetInputPin_Internal((uint8_t)pin);
}


void XQ_IO_Refresh_Task(void *argument) {

  HAL_StatusTypeDef status;
  uint64_t input_state = 0;
  GPIO_PinState int1_state, int2_state, int3_state;
  
  for(;;) {

    /* 检查TCA9535中断引脚是否变为低电平（输入发生变化） */
    int1_state = HAL_GPIO_ReadPin(TCA9535_INPUT_1_GPIO_Port, TCA9535_INPUT_1_Pin);
    int2_state = HAL_GPIO_ReadPin(TCA9535_INPUT_2_GPIO_Port, TCA9535_INPUT_2_Pin);
    int3_state = HAL_GPIO_ReadPin(TCA9535_INPUT_3_GPIO_Port, TCA9535_INPUT_3_Pin);

    // TODO: 这里是32位输入状态的更新，需要测试超出32的输入
    // 把输入状态写入保持寄存器（DI只读区）
    usRegHoldBuf[1642 - MB_HOLD_START_ADDR] = (uint16_t)(xqIO_Input & 0xFFFF);        // DI低16位
    usRegHoldBuf[1643 - MB_HOLD_START_ADDR] = (uint16_t)((xqIO_Input >> 16) & 0xFFFF); // DI高16位
    /* 如果任一中断引脚为低电平，说明输入状态发生变化 */
    if (int1_state == GPIO_PIN_RESET || int2_state == GPIO_PIN_RESET || int3_state == GPIO_PIN_RESET) {
      /* 读取所有输入状态（读取操作会自动清除中断） */
      status = XQ_GetDI(&input_state);
      
      if (status == HAL_OK) {
        /* 更新全局输入状态 */
        xqIO_Input = input_state;
      }
    } else {
      /* TCA9535中断引脚空闲，但仍需轮询STM32本地GPIO输入（引脚27-31）*/
      /* 因为这些引脚不会触发TCA9535的INT信号 */
      XQ_GetDI_STM32Only(&xqIO_Input);
    }
    
    /* 检查输出是否发生变化 */
    if (xqIO_Output != xqIO_Output_Last) {
      /* 备份当前输出状态 */
      xqIO_Output_Last = xqIO_Output;
      
      /* 设置所有输出状态 */
      XQ_SetDO(xqIO_Output);
    }
    osDelay(1);  // 每1ms检查一次输入输出状态
  }
}
