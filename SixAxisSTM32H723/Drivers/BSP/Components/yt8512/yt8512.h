/**
  ******************************************************************************
  * @file    yt8512.h
  * @brief   YT8512 PHY driver interface for STM32 ETH MDIO access.
  ******************************************************************************
  */

#ifndef YT8512_H
#define YT8512_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Standard Clause 22 register map */
#define YT8512_BCR      ((uint16_t)0x0000U)
#define YT8512_BSR      ((uint16_t)0x0001U)
#define YT8512_PHYI1R   ((uint16_t)0x0002U)
#define YT8512_PHYI2R   ((uint16_t)0x0003U)
#define YT8512_ANAR     ((uint16_t)0x0004U)
#define YT8512_ANLPAR   ((uint16_t)0x0005U)
#define YT8512_ANER     ((uint16_t)0x0006U)

/* PHY specific status register (datasheet: register 0x11 / 11h) */
#define YT8512_PHYSCSR  ((uint16_t)0x0011U)

/* BCR bits */
#define YT8512_BCR_SOFT_RESET         ((uint16_t)0x8000U)
#define YT8512_BCR_LOOPBACK           ((uint16_t)0x4000U)
#define YT8512_BCR_SPEED_SELECT       ((uint16_t)0x2000U)
#define YT8512_BCR_AUTONEGO_EN        ((uint16_t)0x1000U)
#define YT8512_BCR_POWER_DOWN         ((uint16_t)0x0800U)
#define YT8512_BCR_ISOLATE            ((uint16_t)0x0400U)
#define YT8512_BCR_RESTART_AUTONEGO   ((uint16_t)0x0200U)
#define YT8512_BCR_DUPLEX_MODE        ((uint16_t)0x0100U)

/* BSR bits */
#define YT8512_BSR_100BASE_TX_FD      ((uint16_t)0x4000U)
#define YT8512_BSR_100BASE_TX_HD      ((uint16_t)0x2000U)
#define YT8512_BSR_10BASE_T_FD        ((uint16_t)0x1000U)
#define YT8512_BSR_10BASE_T_HD        ((uint16_t)0x0800U)
#define YT8512_BSR_AUTONEGO_CPLT      ((uint16_t)0x0020U)
#define YT8512_BSR_AUTONEGO_ABILITY   ((uint16_t)0x0008U)
#define YT8512_BSR_LINK_STATUS        ((uint16_t)0x0004U)

/* ANAR/ANLPAR bits */
#define YT8512_ANAR_100BASE_TX_FD     ((uint16_t)0x0100U)
#define YT8512_ANAR_100BASE_TX        ((uint16_t)0x0080U)
#define YT8512_ANAR_10BASE_T_FD       ((uint16_t)0x0040U)
#define YT8512_ANAR_10BASE_T          ((uint16_t)0x0020U)

/* PHY specific status speed/duplex hints (fallback to ANAR/ANLPAR if unknown) */
#define YT8512_PHYSCSR_SPEED_MASK     ((uint16_t)0xC000U)
#define YT8512_PHYSCSR_SPEED_10M      ((uint16_t)0x0000U)
#define YT8512_PHYSCSR_SPEED_100M     ((uint16_t)0x4000U)
#define YT8512_PHYSCSR_DUPLEX         ((uint16_t)0x2000U)

/* Status codes (kept compatible with ethernetif switch logic) */
#define  YT8512_STATUS_READ_ERROR             ((int32_t)-5)
#define  YT8512_STATUS_WRITE_ERROR            ((int32_t)-4)
#define  YT8512_STATUS_ADDRESS_ERROR          ((int32_t)-3)
#define  YT8512_STATUS_RESET_TIMEOUT          ((int32_t)-2)
#define  YT8512_STATUS_ERROR                  ((int32_t)-1)
#define  YT8512_STATUS_OK                     ((int32_t) 0)
#define  YT8512_STATUS_LINK_DOWN              ((int32_t) 1)
#define  YT8512_STATUS_100MBITS_FULLDUPLEX    ((int32_t) 2)
#define  YT8512_STATUS_100MBITS_HALFDUPLEX    ((int32_t) 3)
#define  YT8512_STATUS_10MBITS_FULLDUPLEX     ((int32_t) 4)
#define  YT8512_STATUS_10MBITS_HALFDUPLEX     ((int32_t) 5)
#define  YT8512_STATUS_AUTONEGO_NOTDONE       ((int32_t) 6)

typedef int32_t  (*yt8512_Init_Func) (void);
typedef int32_t  (*yt8512_DeInit_Func) (void);
typedef int32_t  (*yt8512_ReadReg_Func)   (uint32_t, uint32_t, uint32_t *);
typedef int32_t  (*yt8512_WriteReg_Func)  (uint32_t, uint32_t, uint32_t);
typedef int32_t  (*yt8512_GetTick_Func)  (void);

typedef struct
{
  yt8512_Init_Func      Init;
  yt8512_DeInit_Func    DeInit;
  yt8512_WriteReg_Func  WriteReg;
  yt8512_ReadReg_Func   ReadReg;
  yt8512_GetTick_Func   GetTick;
} yt8512_IOCtx_t;

typedef struct
{
  uint32_t            DevAddr;
  uint32_t            Is_Initialized;
  yt8512_IOCtx_t      IO;
  void               *pData;
} yt8512_Object_t;

int32_t YT8512_RegisterBusIO(yt8512_Object_t *pObj, yt8512_IOCtx_t *ioctx);
int32_t YT8512_Init(yt8512_Object_t *pObj);
int32_t YT8512_DeInit(yt8512_Object_t *pObj);
int32_t YT8512_DisablePowerDownMode(yt8512_Object_t *pObj);
int32_t YT8512_EnablePowerDownMode(yt8512_Object_t *pObj);
int32_t YT8512_StartAutoNego(yt8512_Object_t *pObj);
int32_t YT8512_GetLinkState(yt8512_Object_t *pObj);
int32_t YT8512_SetLinkState(yt8512_Object_t *pObj, uint32_t LinkState);
int32_t YT8512_EnableLoopbackMode(yt8512_Object_t *pObj);
int32_t YT8512_DisableLoopbackMode(yt8512_Object_t *pObj);
int32_t YT8512_EnableIT(yt8512_Object_t *pObj, uint32_t Interrupt);
int32_t YT8512_DisableIT(yt8512_Object_t *pObj, uint32_t Interrupt);
int32_t YT8512_ClearIT(yt8512_Object_t *pObj, uint32_t Interrupt);
int32_t YT8512_GetITStatus(yt8512_Object_t *pObj, uint32_t Interrupt);

#ifdef __cplusplus
}
#endif

#endif /* YT8512_H */
