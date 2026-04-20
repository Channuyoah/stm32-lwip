#include "xq_modbus.h"
#include "mb.h"
#include "mbport.h"

// 十路输入寄存器
#define REG_INPUT_SIZE  43
uint16_t REG_INPUT_BUF[REG_INPUT_SIZE];


// 十路保持寄存器
#define REG_HOLD_SIZE   43
uint16_t REG_HOLD_BUF[REG_HOLD_SIZE] = {0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x000A};


// 十路线圈
#define REG_COILS_SIZE 43
uint8_t REG_COILS_BUF[REG_COILS_SIZE];


// 十路离散量
#define REG_DISC_SIZE  43
uint8_t REG_DISC_BUF[REG_DISC_SIZE];


/// CMD4
eMBErrorCode 
eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs ) {

  USHORT usRegIndex = usAddress - 1; 

  // 非法检测
  if((usRegIndex + usNRegs) > REG_INPUT_SIZE)
  {
      return MB_ENOREG;
  }

  // 循环读取
  while( usNRegs > 0 )
  {
      *pucRegBuffer++ = ( unsigned char )( REG_INPUT_BUF[usRegIndex] >> 8 );
      *pucRegBuffer++ = ( unsigned char )( REG_INPUT_BUF[usRegIndex] & 0xFF );
      usRegIndex++;
      usNRegs--;
  }

  // 模拟输入寄存器被改变
  for(usRegIndex = 0; usRegIndex < REG_INPUT_SIZE; usRegIndex++)
  {
      REG_INPUT_BUF[usRegIndex]++;
  }

  return MB_ENOERR;
}

/**
 * @brief Modbus保持寄存器回调函数
 * @param pucRegBuffer: 寄存器数据缓冲区指针
 * @param usAddress: 起始寄存器地址 (1-based)
 * @param usNRegs: 寄存器数量
 * @param eMode: 操作模式 (MB_REG_READ/MB_REG_WRITE)
 * @return eMBErrorCode: 错误代码
 * 
 * @note 支持的Modbus功能码:
 *       - CMD03 (0x03): 读保持寄存器
 *       - CMD06 (0x06): 写单个保持寄存器
 *       - CMD16 (0x10): 写多个保持寄存器
 * 
 * @details 数据格式:
 *          - 每个寄存器16位，大端格式 (高字节在前)
 *          - 寄存器地址范围: 1 ~ REG_HOLD_SIZE
 *          - 缓冲区中数据按字节存储: [高字节][低字节]
 * 
 * @example 读取操作:
 *          寄存器值0x1234 → 缓冲区: [0x12, 0x34]
 *          
 * @example 写入操作:
 *          缓冲区: [0x12, 0x34] → 寄存器值0x1234
 */
eMBErrorCode 
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode ) {
  
  USHORT usRegIndex = usAddress - 1;  // 转换为0-based索引

  // 地址范围合法性检测
  if((usRegIndex + usNRegs) > REG_HOLD_SIZE)
  {
      return MB_ENOREG;  // 寄存器地址超出范围
  }

  // 写寄存器操作 (CMD06, CMD16)
  if(eMode == MB_REG_WRITE)
  {
    while( usNRegs > 0 )
    {
      // 从缓冲区读取大端格式数据，组合成16位寄存器值
      REG_HOLD_BUF[usRegIndex] = (pucRegBuffer[0] << 8) | pucRegBuffer[1];
      pucRegBuffer += 2;      // 移动到下一个寄存器数据
      usRegIndex++;           // 移动到下一个寄存器
      usNRegs--;              // 减少剩余寄存器数量
    }
  }
      
  // 读寄存器操作 (CMD03)
  else
  {
    while( usNRegs > 0 )
    {
      // 将16位寄存器值拆分为大端格式字节存入缓冲区
      *pucRegBuffer++ = ( unsigned char )( REG_HOLD_BUF[usRegIndex] >> 8 );    // 高字节
      *pucRegBuffer++ = ( unsigned char )( REG_HOLD_BUF[usRegIndex] & 0xFF );  // 低字节
      usRegIndex++;           // 移动到下一个寄存器
      usNRegs--;              // 减少剩余寄存器数量
    }
  }

  return MB_ENOERR;  // 操作成功
}

/// CMD1、5、15
eMBErrorCode eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode )
{
  USHORT usRegIndex   = usAddress - 1;
  USHORT usCoilGroups = ((usNCoils - 1) / 8 + 1);
  UCHAR  ucStatus     = 0;
  UCHAR  ucBits       = 0;
  UCHAR  ucDisp       = 0;

  // 非法检测
  if((usRegIndex + usNCoils) > REG_COILS_SIZE)
  {
      return MB_ENOREG;
  }

  // 写线圈
  if(eMode == MB_REG_WRITE)
  {
    while(usCoilGroups--)
    {
      ucStatus = *pucRegBuffer++;
      ucBits   = 8;
      while((usNCoils--) != 0 && (ucBits--) != 0)
      {
          REG_COILS_BUF[usRegIndex++] = ucStatus & 0X01;
          ucStatus >>= 1;
      }
    }
  }

  // 读线圈
  else
  {
    while(usCoilGroups--)
    {
      ucDisp = 0;
      ucBits = 8;
      while((usNCoils--) != 0 && (ucBits--) != 0)
      {
          ucStatus |= (REG_COILS_BUF[usRegIndex++] << (ucDisp++));
      }
      *pucRegBuffer++ = ucStatus;
    }
  }
  return MB_ENOERR;
}


/// CMD4
eMBErrorCode 
eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete ) {

  USHORT usRegIndex   = usAddress - 1;
  USHORT usCoilGroups = ((usNDiscrete - 1) / 8 + 1);
  UCHAR  ucStatus     = 0;
  UCHAR  ucBits       = 0;
  UCHAR  ucDisp       = 0;

  // 非法检测
  if((usRegIndex + usNDiscrete) > REG_DISC_SIZE)
  {
    return MB_ENOREG;
  }

  // 读离散输入
  while(usCoilGroups--)
  {
    ucDisp = 0;
    ucBits = 8;
    while((usNDiscrete--) != 0 && (ucBits--) != 0)
    {
            if(REG_DISC_BUF[usRegIndex])
            {
                    ucStatus |= (1 << ucDisp);
            }
            ucDisp++;
    }
    *pucRegBuffer++ = ucStatus;
  }

  // 模拟改变
  for(usRegIndex = 0; usRegIndex < REG_DISC_SIZE; usRegIndex++)
  {
    REG_DISC_BUF[usRegIndex] = !REG_DISC_BUF[usRegIndex];
  }

  return MB_ENOERR;

}
