#include "xq_modbus.h"
#include "xq_reg_event.h"
#include <string.h>

// ========== 物理数据缓存 ==========
// 保持寄存器 (1000~1711)
uint16_t usRegHoldBuf[MB_HOLD_SIZE];

// 输入寄存器 (1200~1711)
uint16_t usRegInputBuf[MB_INPUT_SIZE];

// 线圈 (1642~1648)
uint8_t ucCoilsBuf[MB_COILS_SIZE / 8 + 1];   // 位数组

// 离散输入 (1642~)
uint8_t ucDiscBuf[MB_DISC_SIZE / 8 + 1];

// ========== 浮点数转换辅助 ==========
void SetFloatToReg(uint16_t *reg, float value) {
    union {
        float f;
        uint16_t u16[2];
    } conv;
    conv.f = value;
    reg[0] = conv.u16[1];   // 大端：高字在前
    reg[1] = conv.u16[0];
}

float GetFloatFromReg(const uint16_t *reg) {
    union {
        float f;
        uint16_t u16[2];
    } conv;
    conv.u16[1] = reg[0];
    conv.u16[0] = reg[1];
    return conv.f;
}

// ========== 保持寄存器回调 (功能码 03,06,16) ==========
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
                             USHORT usNRegs, eMBRegisterMode eMode)
{
    uint16_t offset = usAddress - MB_HOLD_START_ADDR - 1;
    // 地址范围检查
    if ((offset + usNRegs) > MB_HOLD_SIZE) {
        return MB_ENOREG;
    }
    if (eMode == MB_REG_WRITE) {
        uint16_t startAddr = usAddress - 1;
        uint16_t count = usNRegs;
        // 写操作：将 pucRegBuffer 中数据(大端)复制到 usRegHoldBuf
        while (usNRegs > 0) {
            usRegHoldBuf[offset] = (pucRegBuffer[0] << 8) | pucRegBuffer[1];
            pucRegBuffer += 2;
            offset++;
            usNRegs--;
        }
        // 通知每一个写入的地址
        for (uint16_t i = 0; i < count; i++) {
            printf("-------------i:%d, addr=%d\r\n", i, startAddr + i);
            printf("-------------Holding Reg Write: addr=%d, value=0x%X\r\n", startAddr + i, usRegHoldBuf[(startAddr + i) - MB_HOLD_START_ADDR]);
            xq_reg_notify(startAddr + i);
        }
    } else {
        uint16_t startAddr = usAddress - 1;
        uint16_t count = usNRegs;
        // 读操作：将 usRegHoldBuf 数据输出到 pucRegBuffer
        while (usNRegs > 0) {
            *pucRegBuffer++ = (UCHAR)(usRegHoldBuf[offset] >> 8);
            *pucRegBuffer++ = (UCHAR)(usRegHoldBuf[offset] & 0xFF);
            offset++;
            usNRegs--;
        }
    }
    return MB_ENOERR;
}

// ========== 输入寄存器回调 (功能码 04) ==========
eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
    uint16_t offset = usAddress - MB_INPUT_START_ADDR - 1;
    if ((offset + usNRegs) > MB_INPUT_SIZE) {
        return MB_ENOREG;
    }
    while (usNRegs > 0) {
        *pucRegBuffer++ = (UCHAR)(usRegInputBuf[offset] >> 8);
        *pucRegBuffer++ = (UCHAR)(usRegInputBuf[offset] & 0xFF);
        offset++;
        usNRegs--;
    }
    // // 通知每个线圈地址
    // for (int i = 0; i < usNCoils; i++) {
    //     xq_reg_notify(usAddress + i);
    // }
    return MB_ENOERR;
}

// ========== 线圈回调 (功能码 01,05,15) ==========
eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress,
                           USHORT usNCoils, eMBRegisterMode eMode)
{
    // 位地址从1开始，将地址转换为数组偏移
    uint16_t bitOffset = usAddress - MB_COILS_START_ADDR;
    if ((bitOffset + usNCoils) > MB_COILS_SIZE) {
        return MB_ENOREG;
    }

    if (eMode == MB_REG_WRITE) {
        // 写线圈：从 pucRegBuffer 中取出位，存入 ucCoilsBuf
        for (int i = 0; i < usNCoils; i++) {
            uint8_t byte = i >> 3;
            uint8_t bit = i & 0x07;
            uint8_t val = (pucRegBuffer[byte] >> bit) & 0x01;
            if (val)
                ucCoilsBuf[(bitOffset + i) >> 3] |= (1 << ((bitOffset + i) & 0x07));
            else
                ucCoilsBuf[(bitOffset + i) >> 3] &= ~(1 << ((bitOffset + i) & 0x07));
        }
    } else {
        // 读线圈：从 ucCoilsBuf 组装位值
        uint8_t byteCnt = (usNCoils + 7) / 8;
        for (int b = 0; b < byteCnt; b++) {
            pucRegBuffer[b] = 0;
        }
        for (int i = 0; i < usNCoils; i++) {
            uint8_t bitVal = (ucCoilsBuf[(bitOffset + i) >> 3] >> ((bitOffset + i) & 0x07)) & 0x01;
            if (bitVal)
                pucRegBuffer[i >> 3] |= (1 << (i & 0x07));
        }
    }
    return MB_ENOERR;
}

// ========== 离散输入回调 (功能码 02) ==========
eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete)
{
    uint16_t bitOffset = usAddress - MB_DISC_START_ADDR - 1;
    if ((bitOffset + usNDiscrete) > MB_DISC_SIZE) {
        return MB_ENOREG;
    }
    // 从 ucDiscBuf 读取位
    uint8_t byteCnt = (usNDiscrete + 7) / 8;
    memset(pucRegBuffer, 0, byteCnt);
    for (int i = 0; i < usNDiscrete; i++) {
        uint8_t bitVal = (ucDiscBuf[(bitOffset + i) >> 3] >> ((bitOffset + i) & 0x07)) & 0x01;
        if (bitVal)
            pucRegBuffer[i >> 3] |= (1 << (i & 0x07));
    }
    return MB_ENOERR;
}

// ========== 初始化默认值（示例） ==========
void InitModbusRegisters(void)
{
    // 轴参数 Axis[0] 地址1000
    // 每转脉冲数 1000, 实际地址 1001? 这里假设两个int16: 脉冲数、实际地址
    usRegHoldBuf[1000 - MB_HOLD_START_ADDR] = 1000;  // 每转脉冲数
    usRegHoldBuf[1001 - MB_HOLD_START_ADDR] = 2000;  // 实际地址

    // Axis[0] pitch (float) 地址1020
    SetFloatToReg(&usRegHoldBuf[1020 - MB_HOLD_START_ADDR], 5.0f);

    // 编码器位置(只读)放在输入寄存器，地址1224
    SetFloatToReg(&usRegInputBuf[1224 - MB_INPUT_START_ADDR], 0.0f);

    // DI初始(离散输入)地址1642，32位全0
    memset(ucDiscBuf, 0, sizeof(ucDiscBuf));
    // DO初始(线圈)地址1644
    memset(ucCoilsBuf, 0, sizeof(ucCoilsBuf));
}