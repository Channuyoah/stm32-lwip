/**
  ******************************************************************************
  * @file    yt8512h.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the
  *          yt8512h.c PHY driver.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef YT8512H_H
#define YT8512H_H

#ifdef __cplusplus
 extern "C" {
#endif   
   
/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup Component
  * @{
  */
    
/** @defgroup YT8512H
  * @{
  */    
/* Exported constants --------------------------------------------------------*/
/** @defgroup YT8512H_Exported_Constants YT8512H Exported Constants
  * @{
  */ 
  
/** @defgroup YT8512H_Registers_Mapping YT8512H Registers Mapping
  * @{
  */ 
#define YT8512H_BCR      ((uint16_t)0x0000U)
#define YT8512H_BSR      ((uint16_t)0x0001U)
#define YT8512H_PHYI1R   ((uint16_t)0x0002U)
#define YT8512H_PHYI2R   ((uint16_t)0x0003U)
#define YT8512H_ANAR     ((uint16_t)0x0004U)
#define YT8512H_ANLPAR   ((uint16_t)0x0005U)
#define YT8512H_ANER     ((uint16_t)0x0006U)
#define YT8512H_ANNPTR   ((uint16_t)0x0007U)
#define YT8512H_ANNPRR   ((uint16_t)0x0008U)
#define YT8512H_MMDACR   ((uint16_t)0x000DU)
#define YT8512H_MMDAADR  ((uint16_t)0x000EU)
#define YT8512H_ENCTR    ((uint16_t)0x0010U)
#define YT8512H_MCSR     ((uint16_t)0x0011U)
#define YT8512H_SMR      ((uint16_t)0x0012U)
#define YT8512H_TPDCR    ((uint16_t)0x0018U)
#define YT8512H_TCSR     ((uint16_t)0x0019U)
#define YT8512H_SECR     ((uint16_t)0x001AU)
#define YT8512H_SCSIR    ((uint16_t)0x001BU)
#define YT8512H_CLR      ((uint16_t)0x001CU)
#define YT8512H_ISFR     ((uint16_t)0x001DU)
#define YT8512H_IMR      ((uint16_t)0x001EU)
#define YT8512H_PHYSCSR  ((uint16_t)0x001FU)
/**
  * @}
  */

/** @defgroup YT8512H_BCR_Bit_Definition YT8512H BCR Bit Definition
  * @{
  */    
#define YT8512H_BCR_SOFT_RESET         ((uint16_t)0x8000U)
#define YT8512H_BCR_LOOPBACK           ((uint16_t)0x4000U)
#define YT8512H_BCR_SPEED_SELECT       ((uint16_t)0x2000U)
#define YT8512H_BCR_AUTONEGO_EN        ((uint16_t)0x1000U)
#define YT8512H_BCR_POWER_DOWN         ((uint16_t)0x0800U)
#define YT8512H_BCR_ISOLATE            ((uint16_t)0x0400U)
#define YT8512H_BCR_RESTART_AUTONEGO   ((uint16_t)0x0200U)
#define YT8512H_BCR_DUPLEX_MODE        ((uint16_t)0x0100U) 
/**
  * @}
  */

/** @defgroup YT8512H_BSR_Bit_Definition YT8512H BSR Bit Definition
  * @{
  */   
#define YT8512H_BSR_100BASE_T4       ((uint16_t)0x8000U)
#define YT8512H_BSR_100BASE_TX_FD    ((uint16_t)0x4000U)
#define YT8512H_BSR_100BASE_TX_HD    ((uint16_t)0x2000U)
#define YT8512H_BSR_10BASE_T_FD      ((uint16_t)0x1000U)
#define YT8512H_BSR_10BASE_T_HD      ((uint16_t)0x0800U)
#define YT8512H_BSR_100BASE_T2_FD    ((uint16_t)0x0400U)
#define YT8512H_BSR_100BASE_T2_HD    ((uint16_t)0x0200U)
#define YT8512H_BSR_EXTENDED_STATUS  ((uint16_t)0x0100U)
#define YT8512H_BSR_AUTONEGO_CPLT    ((uint16_t)0x0020U)
#define YT8512H_BSR_REMOTE_FAULT     ((uint16_t)0x0010U)
#define YT8512H_BSR_AUTONEGO_ABILITY ((uint16_t)0x0008U)
#define YT8512H_BSR_LINK_STATUS      ((uint16_t)0x0004U)
#define YT8512H_BSR_JABBER_DETECT    ((uint16_t)0x0002U)
#define YT8512H_BSR_EXTENDED_CAP     ((uint16_t)0x0001U)
/**
  * @}
  */

/** @defgroup YT8512H_PHYI1R_Bit_Definition YT8512H PHYI1R Bit Definition
  * @{
  */
#define YT8512H_PHYI1R_OUI_3_18           ((uint16_t)0xFFFFU)
/**
  * @}
  */

/** @defgroup YT8512H_PHYI2R_Bit_Definition YT8512H PHYI2R Bit Definition
  * @{
  */
#define YT8512H_PHYI2R_OUI_19_24          ((uint16_t)0xFC00U)
#define YT8512H_PHYI2R_MODEL_NBR          ((uint16_t)0x03F0U)
#define YT8512H_PHYI2R_REVISION_NBR       ((uint16_t)0x000FU)
/**
  * @}
  */

/** @defgroup YT8512H_ANAR_Bit_Definition YT8512H ANAR Bit Definition
  * @{
  */
#define YT8512H_ANAR_NEXT_PAGE               ((uint16_t)0x8000U)
#define YT8512H_ANAR_REMOTE_FAULT            ((uint16_t)0x2000U)
#define YT8512H_ANAR_PAUSE_OPERATION         ((uint16_t)0x0C00U)
#define YT8512H_ANAR_PO_NOPAUSE              ((uint16_t)0x0000U)
#define YT8512H_ANAR_PO_SYMMETRIC_PAUSE      ((uint16_t)0x0400U)
#define YT8512H_ANAR_PO_ASYMMETRIC_PAUSE     ((uint16_t)0x0800U)
#define YT8512H_ANAR_PO_ADVERTISE_SUPPORT    ((uint16_t)0x0C00U)
#define YT8512H_ANAR_100BASE_TX_FD           ((uint16_t)0x0100U)
#define YT8512H_ANAR_100BASE_TX              ((uint16_t)0x0080U)
#define YT8512H_ANAR_10BASE_T_FD             ((uint16_t)0x0040U)
#define YT8512H_ANAR_10BASE_T                ((uint16_t)0x0020U)
#define YT8512H_ANAR_SELECTOR_FIELD          ((uint16_t)0x000FU)
/**
  * @}
  */

/** @defgroup YT8512H_ANLPAR_Bit_Definition YT8512H ANLPAR Bit Definition
  * @{
  */
#define YT8512H_ANLPAR_NEXT_PAGE            ((uint16_t)0x8000U)
#define YT8512H_ANLPAR_REMOTE_FAULT         ((uint16_t)0x2000U)
#define YT8512H_ANLPAR_PAUSE_OPERATION      ((uint16_t)0x0C00U)
#define YT8512H_ANLPAR_PO_NOPAUSE           ((uint16_t)0x0000U)
#define YT8512H_ANLPAR_PO_SYMMETRIC_PAUSE   ((uint16_t)0x0400U)
#define YT8512H_ANLPAR_PO_ASYMMETRIC_PAUSE  ((uint16_t)0x0800U)
#define YT8512H_ANLPAR_PO_ADVERTISE_SUPPORT ((uint16_t)0x0C00U)
#define YT8512H_ANLPAR_100BASE_TX_FD        ((uint16_t)0x0100U)
#define YT8512H_ANLPAR_100BASE_TX           ((uint16_t)0x0080U)
#define YT8512H_ANLPAR_10BASE_T_FD          ((uint16_t)0x0040U)
#define YT8512H_ANLPAR_10BASE_T             ((uint16_t)0x0020U)
#define YT8512H_ANLPAR_SELECTOR_FIELD       ((uint16_t)0x000FU)
/**
  * @}
  */

/** @defgroup YT8512H_ANER_Bit_Definition YT8512H ANER Bit Definition
  * @{
  */
#define YT8512H_ANER_RX_NP_LOCATION_ABLE    ((uint16_t)0x0040U)
#define YT8512H_ANER_RX_NP_STORAGE_LOCATION ((uint16_t)0x0020U)
#define YT8512H_ANER_PARALLEL_DETECT_FAULT  ((uint16_t)0x0010U)
#define YT8512H_ANER_LP_NP_ABLE             ((uint16_t)0x0008U)
#define YT8512H_ANER_NP_ABLE                ((uint16_t)0x0004U)
#define YT8512H_ANER_PAGE_RECEIVED          ((uint16_t)0x0002U)
#define YT8512H_ANER_LP_AUTONEG_ABLE        ((uint16_t)0x0001U)
/**
  * @}
  */

/** @defgroup YT8512H_ANNPTR_Bit_Definition YT8512H ANNPTR Bit Definition
  * @{
  */
#define YT8512H_ANNPTR_NEXT_PAGE         ((uint16_t)0x8000U)
#define YT8512H_ANNPTR_MESSAGE_PAGE      ((uint16_t)0x2000U)
#define YT8512H_ANNPTR_ACK2              ((uint16_t)0x1000U)
#define YT8512H_ANNPTR_TOGGLE            ((uint16_t)0x0800U)
#define YT8512H_ANNPTR_MESSAGGE_CODE     ((uint16_t)0x07FFU)
/**
  * @}
  */

/** @defgroup YT8512H_ANNPRR_Bit_Definition YT8512H ANNPRR Bit Definition
  * @{
  */
#define YT8512H_ANNPTR_NEXT_PAGE         ((uint16_t)0x8000U)
#define YT8512H_ANNPRR_ACK               ((uint16_t)0x4000U)
#define YT8512H_ANNPRR_MESSAGE_PAGE      ((uint16_t)0x2000U)
#define YT8512H_ANNPRR_ACK2              ((uint16_t)0x1000U)
#define YT8512H_ANNPRR_TOGGLE            ((uint16_t)0x0800U)
#define YT8512H_ANNPRR_MESSAGGE_CODE     ((uint16_t)0x07FFU)
/**
  * @}
  */

/** @defgroup YT8512H_MMDACR_Bit_Definition YT8512H MMDACR Bit Definition
  * @{
  */
#define YT8512H_MMDACR_MMD_FUNCTION       ((uint16_t)0xC000U) 
#define YT8512H_MMDACR_MMD_FUNCTION_ADDR  ((uint16_t)0x0000U)
#define YT8512H_MMDACR_MMD_FUNCTION_DATA  ((uint16_t)0x4000U)
#define YT8512H_MMDACR_MMD_DEV_ADDR       ((uint16_t)0x001FU)
/**
  * @}
  */

/** @defgroup YT8512H_ENCTR_Bit_Definition YT8512H ENCTR Bit Definition
  * @{
  */
#define YT8512H_ENCTR_TX_ENABLE             ((uint16_t)0x8000U)
#define YT8512H_ENCTR_TX_TIMER              ((uint16_t)0x6000U)
#define YT8512H_ENCTR_TX_TIMER_1S           ((uint16_t)0x0000U)
#define YT8512H_ENCTR_TX_TIMER_768MS        ((uint16_t)0x2000U)
#define YT8512H_ENCTR_TX_TIMER_512MS        ((uint16_t)0x4000U)
#define YT8512H_ENCTR_TX_TIMER_265MS        ((uint16_t)0x6000U)
#define YT8512H_ENCTR_RX_ENABLE             ((uint16_t)0x1000U)
#define YT8512H_ENCTR_RX_MAX_INTERVAL       ((uint16_t)0x0C00U)
#define YT8512H_ENCTR_RX_MAX_INTERVAL_64MS  ((uint16_t)0x0000U)
#define YT8512H_ENCTR_RX_MAX_INTERVAL_256MS ((uint16_t)0x0400U)
#define YT8512H_ENCTR_RX_MAX_INTERVAL_512MS ((uint16_t)0x0800U)
#define YT8512H_ENCTR_RX_MAX_INTERVAL_1S    ((uint16_t)0x0C00U)
#define YT8512H_ENCTR_EX_CROSS_OVER         ((uint16_t)0x0002U)
#define YT8512H_ENCTR_EX_MANUAL_CROSS_OVER  ((uint16_t)0x0001U)
/**
  * @}
  */

/** @defgroup YT8512H_MCSR_Bit_Definition YT8512H MCSR Bit Definition
  * @{
  */
#define YT8512H_MCSR_EDPWRDOWN        ((uint16_t)0x2000U)
#define YT8512H_MCSR_FARLOOPBACK      ((uint16_t)0x0200U)
#define YT8512H_MCSR_ALTINT           ((uint16_t)0x0040U)
#define YT8512H_MCSR_ENERGYON         ((uint16_t)0x0002U)
/**
  * @}
  */

/** @defgroup YT8512H_SMR_Bit_Definition YT8512H SMR Bit Definition
  * @{
  */
#define YT8512H_SMR_MODE       ((uint16_t)0x00E0U)
#define YT8512H_SMR_PHY_ADDR   ((uint16_t)0x001FU)
/**
  * @}
  */

/** @defgroup YT8512H_TPDCR_Bit_Definition YT8512H TPDCR Bit Definition
  * @{
  */
#define YT8512H_TPDCR_DELAY_IN                 ((uint16_t)0x8000U)
#define YT8512H_TPDCR_LINE_BREAK_COUNTER       ((uint16_t)0x7000U)
#define YT8512H_TPDCR_PATTERN_HIGH             ((uint16_t)0x0FC0U)
#define YT8512H_TPDCR_PATTERN_LOW              ((uint16_t)0x003FU)
/**
  * @}
  */

/** @defgroup YT8512H_TCSR_Bit_Definition YT8512H TCSR Bit Definition
  * @{
  */
#define YT8512H_TCSR_TDR_ENABLE           ((uint16_t)0x8000U)
#define YT8512H_TCSR_TDR_AD_FILTER_ENABLE ((uint16_t)0x4000U)
#define YT8512H_TCSR_TDR_CH_CABLE_TYPE    ((uint16_t)0x0600U)
#define YT8512H_TCSR_TDR_CH_CABLE_DEFAULT ((uint16_t)0x0000U)
#define YT8512H_TCSR_TDR_CH_CABLE_SHORTED ((uint16_t)0x0200U)
#define YT8512H_TCSR_TDR_CH_CABLE_OPEN    ((uint16_t)0x0400U)
#define YT8512H_TCSR_TDR_CH_CABLE_MATCH   ((uint16_t)0x0600U)
#define YT8512H_TCSR_TDR_CH_STATUS        ((uint16_t)0x0100U)
#define YT8512H_TCSR_TDR_CH_LENGTH        ((uint16_t)0x00FFU)
/**
  * @}
  */

/** @defgroup YT8512H_SCSIR_Bit_Definition YT8512H SCSIR Bit Definition
  * @{
  */
#define YT8512H_SCSIR_AUTO_MDIX_ENABLE    ((uint16_t)0x8000U)
#define YT8512H_SCSIR_CHANNEL_SELECT      ((uint16_t)0x2000U)
#define YT8512H_SCSIR_SQE_DISABLE         ((uint16_t)0x0800U)
#define YT8512H_SCSIR_XPOLALITY           ((uint16_t)0x0010U)
/**
  * @}
  */

/** @defgroup YT8512H_CLR_Bit_Definition YT8512H CLR Bit Definition
  * @{
  */
#define YT8512H_CLR_CABLE_LENGTH       ((uint16_t)0xF000U)
/**
  * @}
  */

/** @defgroup YT8512H_IMR_ISFR_Bit_Definition YT8512H IMR ISFR Bit Definition
  * @{
  */
#define YT8512H_INT_8       ((uint16_t)0x0100U)
#define YT8512H_INT_7       ((uint16_t)0x0080U)
#define YT8512H_INT_6       ((uint16_t)0x0040U)
#define YT8512H_INT_5       ((uint16_t)0x0020U)
#define YT8512H_INT_4       ((uint16_t)0x0010U)
#define YT8512H_INT_3       ((uint16_t)0x0008U)
#define YT8512H_INT_2       ((uint16_t)0x0004U)
#define YT8512H_INT_1       ((uint16_t)0x0002U)
/**
  * @}
  */

/** @defgroup YT8512H_PHYSCSR_Bit_Definition YT8512H PHYSCSR Bit Definition
  * @{
  */
#define YT8512H_PHYSCSR_AUTONEGO_DONE   ((uint16_t)0x1000U)
#define YT8512H_PHYSCSR_HCDSPEEDMASK    ((uint16_t)0x001CU)
#define YT8512H_PHYSCSR_10BT_HD         ((uint16_t)0x0004U)
#define YT8512H_PHYSCSR_10BT_FD         ((uint16_t)0x0014U)
#define YT8512H_PHYSCSR_100BTX_HD       ((uint16_t)0x0008U)
#define YT8512H_PHYSCSR_100BTX_FD       ((uint16_t)0x0018U) 
/**
  * @}
  */
    
/** @defgroup YT8512H_Status YT8512H Status
  * @{
  */    

#define  YT8512H_STATUS_READ_ERROR            ((int32_t)-5)
#define  YT8512H_STATUS_WRITE_ERROR           ((int32_t)-4)
#define  YT8512H_STATUS_ADDRESS_ERROR         ((int32_t)-3)
#define  YT8512H_STATUS_RESET_TIMEOUT         ((int32_t)-2)
#define  YT8512H_STATUS_ERROR                 ((int32_t)-1)
#define  YT8512H_STATUS_OK                    ((int32_t) 0)
#define  YT8512H_STATUS_LINK_DOWN             ((int32_t) 1)
#define  YT8512H_STATUS_100MBITS_FULLDUPLEX   ((int32_t) 2)
#define  YT8512H_STATUS_100MBITS_HALFDUPLEX   ((int32_t) 3)
#define  YT8512H_STATUS_10MBITS_FULLDUPLEX    ((int32_t) 4)
#define  YT8512H_STATUS_10MBITS_HALFDUPLEX    ((int32_t) 5)
#define  YT8512H_STATUS_AUTONEGO_NOTDONE      ((int32_t) 6)
/**
  * @}
  */

/** @defgroup YT8512H_IT_Flags YT8512H IT Flags
  * @{
  */     
#define  YT8512H_WOL_IT                        YT8512H_INT_8
#define  YT8512H_ENERGYON_IT                   YT8512H_INT_7
#define  YT8512H_AUTONEGO_COMPLETE_IT          YT8512H_INT_6
#define  YT8512H_REMOTE_FAULT_IT               YT8512H_INT_5
#define  YT8512H_LINK_DOWN_IT                  YT8512H_INT_4
#define  YT8512H_AUTONEGO_LP_ACK_IT            YT8512H_INT_3
#define  YT8512H_PARALLEL_DETECTION_FAULT_IT   YT8512H_INT_2
#define  YT8512H_AUTONEGO_PAGE_RECEIVED_IT     YT8512H_INT_1
/**
  * @}
  */

/**
  * @}
  */

/* Exported types ------------------------------------------------------------*/ 
/** @defgroup YT8512H_Exported_Types YT8512H Exported Types
  * @{
  */
typedef int32_t  (*yt8512h_Init_Func) (void); 
typedef int32_t  (*yt8512h_DeInit_Func) (void);
typedef int32_t  (*yt8512h_ReadReg_Func)   (uint32_t, uint32_t, uint32_t *);
typedef int32_t  (*yt8512h_WriteReg_Func)  (uint32_t, uint32_t, uint32_t);
typedef int32_t  (*yt8512h_GetTick_Func)  (void);

typedef struct 
{                   
  yt8512h_Init_Func      Init; 
  yt8512h_DeInit_Func    DeInit;
  yt8512h_WriteReg_Func  WriteReg;
  yt8512h_ReadReg_Func   ReadReg; 
  yt8512h_GetTick_Func   GetTick;   
} yt8512h_IOCtx_t;  

  
typedef struct 
{
  uint32_t            DevAddr;
  uint32_t            Is_Initialized;
  yt8512h_IOCtx_t     IO;
  void               *pData;
}yt8512h_Object_t;
/**
  * @}
  */ 

/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
/** @defgroup YT8512H_Exported_Functions YT8512H Exported Functions
  * @{
  */
int32_t YT8512H_RegisterBusIO(yt8512h_Object_t *pObj, yt8512h_IOCtx_t *ioctx);
int32_t YT8512H_Init(yt8512h_Object_t *pObj);
int32_t YT8512H_DeInit(yt8512h_Object_t *pObj);
int32_t YT8512H_DisablePowerDownMode(yt8512h_Object_t *pObj);
int32_t YT8512H_EnablePowerDownMode(yt8512h_Object_t *pObj);
int32_t YT8512H_StartAutoNego(yt8512h_Object_t *pObj);
int32_t YT8512H_GetLinkState(yt8512h_Object_t *pObj);
int32_t YT8512H_SetLinkState(yt8512h_Object_t *pObj, uint32_t LinkState);
int32_t YT8512H_EnableLoopbackMode(yt8512h_Object_t *pObj);
int32_t YT8512H_DisableLoopbackMode(yt8512h_Object_t *pObj);
int32_t YT8512H_EnableIT(yt8512h_Object_t *pObj, uint32_t Interrupt);
int32_t YT8512H_DisableIT(yt8512h_Object_t *pObj, uint32_t Interrupt);
int32_t YT8512H_ClearIT(yt8512h_Object_t *pObj, uint32_t Interrupt);
int32_t YT8512H_GetITStatus(yt8512h_Object_t *pObj, uint32_t Interrupt);
/**
  * @}
  */ 

#ifdef __cplusplus
}
#endif
#endif /* YT8512H_H */


/**
  * @}
  */ 

/**
  * @}
  */

/**
  * @}
  */ 

/**
  * @}
  */       
