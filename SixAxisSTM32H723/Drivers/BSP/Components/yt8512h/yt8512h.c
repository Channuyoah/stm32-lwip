/**
  ******************************************************************************
  * @file    yt8512h.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions needed to manage the YT8512H
  *          PHY devices.
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

/* Includes ------------------------------------------------------------------*/
#include "yt8512h.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup Component
  * @{
  */

/** @defgroup YT8512H YT8512H
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/** @defgroup YT8512H_Private_Defines YT8512H Private Defines
  * @{
  */
#define YT8512H_MAX_DEV_ADDR   ((uint32_t)31U)
/**
  * @}
  */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/** @defgroup YT8512H_Private_Functions YT8512H Private Functions
  * @{
  */

/**
  * @brief  Register IO functions to component object
  * @param  pObj: device object  of YT8512H_Object_t.
  * @param  ioctx: holds device IO functions.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_ERROR if missing mandatory function
  */
int32_t  YT8512H_RegisterBusIO(yt8512h_Object_t *pObj, yt8512h_IOCtx_t *ioctx)
{
  if(!pObj || !ioctx->ReadReg || !ioctx->WriteReg || !ioctx->GetTick)
  {
    return YT8512H_STATUS_ERROR;
  }

  pObj->IO.Init = ioctx->Init;
  pObj->IO.DeInit = ioctx->DeInit;
  pObj->IO.ReadReg = ioctx->ReadReg;
  pObj->IO.WriteReg = ioctx->WriteReg;
  pObj->IO.GetTick = ioctx->GetTick;

  return YT8512H_STATUS_OK;
}

/**
  * @brief  Initialize the yt8512h and configure the needed hardware resources
  * @param  pObj: device object YT8512H_Object_t.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_ADDRESS_ERROR if cannot find device address
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  */
 int32_t YT8512H_Init(yt8512h_Object_t *pObj)
 {
   uint32_t regvalue = 0, addr = 0;
   int32_t status = YT8512H_STATUS_OK;

   if(pObj->Is_Initialized == 0)
   {
     if(pObj->IO.Init != 0)
     {
       /* GPIO and Clocks initialization */
       pObj->IO.Init();
     }

     /* for later check */
     pObj->DevAddr = YT8512H_MAX_DEV_ADDR + 1;

     /* Get the device address from special mode register */
     for(addr = 0; addr <= YT8512H_MAX_DEV_ADDR; addr ++)
     {
       if(pObj->IO.ReadReg(addr, YT8512H_SMR, &regvalue) < 0)
       {
         status = YT8512H_STATUS_READ_ERROR;
         /* Can't read from this device address
            continue with next address */
         continue;
       }

       if((regvalue & YT8512H_SMR_PHY_ADDR) == addr)
       {
         pObj->DevAddr = addr;
         status = YT8512H_STATUS_OK;
         break;
       }
     }

     if(pObj->DevAddr > YT8512H_MAX_DEV_ADDR)
     {
       status = YT8512H_STATUS_ADDRESS_ERROR;
     }

     /* if device address is matched */
     if(status == YT8512H_STATUS_OK)
     {
       pObj->Is_Initialized = 1;
     }
   }

   return status;
 }

/**
  * @brief  De-Initialize the yt8512h and it's hardware resources
  * @param  pObj: device object YT8512H_Object_t.
  * @retval None
  */
int32_t YT8512H_DeInit(yt8512h_Object_t *pObj)
{
  if(pObj->Is_Initialized)
  {
    if(pObj->IO.DeInit != 0)
    {
      if(pObj->IO.DeInit() < 0)
      {
        return YT8512H_STATUS_ERROR;
      }
    }

    pObj->Is_Initialized = 0;
  }

  return YT8512H_STATUS_OK;
}

/**
  * @brief  Disable the YT8512H power down mode.
  * @param  pObj: device object YT8512H_Object_t.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_DisablePowerDownMode(yt8512h_Object_t *pObj)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &readval) >= 0)
  {
    readval &= ~YT8512H_BCR_POWER_DOWN;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_BCR, readval) < 0)
    {
      status =  YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Enable the YT8512H power down mode.
  * @param  pObj: device object YT8512H_Object_t.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_EnablePowerDownMode(yt8512h_Object_t *pObj)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &readval) >= 0)
  {
    readval |= YT8512H_BCR_POWER_DOWN;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_BCR, readval) < 0)
    {
      status =  YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Start the auto negotiation process.
  * @param  pObj: device object YT8512H_Object_t.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_StartAutoNego(yt8512h_Object_t *pObj)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &readval) >= 0)
  {
    readval |= YT8512H_BCR_AUTONEGO_EN;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_BCR, readval) < 0)
    {
      status =  YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Get the link state of YT8512H device.
  * @param  pObj: Pointer to device object.
  * @param  pLinkState: Pointer to link state
  * @retval YT8512H_STATUS_LINK_DOWN  if link is down
  *         YT8512H_STATUS_AUTONEGO_NOTDONE if Auto nego not completed
  *         YT8512H_STATUS_100MBITS_FULLDUPLEX if 100Mb/s FD
  *         YT8512H_STATUS_100MBITS_HALFDUPLEX if 100Mb/s HD
  *         YT8512H_STATUS_10MBITS_FULLDUPLEX  if 10Mb/s FD
  *         YT8512H_STATUS_10MBITS_HALFDUPLEX  if 10Mb/s HD
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_GetLinkState(yt8512h_Object_t *pObj)
{
  uint32_t readval = 0;

  /* Read Status register  */
  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BSR, &readval) < 0)
  {
    return YT8512H_STATUS_READ_ERROR;
  }

  /* Read Status register again */
  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BSR, &readval) < 0)
  {
    return YT8512H_STATUS_READ_ERROR;
  }

  if((readval & YT8512H_BSR_LINK_STATUS) == 0)
  {
    /* Return Link Down status */
    return YT8512H_STATUS_LINK_DOWN;
  }

  /* Check Auto negotiation */
  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &readval) < 0)
  {
    return YT8512H_STATUS_READ_ERROR;
  }

  if((readval & YT8512H_BCR_AUTONEGO_EN) != YT8512H_BCR_AUTONEGO_EN)
  {
    if(((readval & YT8512H_BCR_SPEED_SELECT) == YT8512H_BCR_SPEED_SELECT) && ((readval & YT8512H_BCR_DUPLEX_MODE) == YT8512H_BCR_DUPLEX_MODE))
    {
      return YT8512H_STATUS_100MBITS_FULLDUPLEX;
    }
    else if ((readval & YT8512H_BCR_SPEED_SELECT) == YT8512H_BCR_SPEED_SELECT)
    {
      return YT8512H_STATUS_100MBITS_HALFDUPLEX;
    }
    else if ((readval & YT8512H_BCR_DUPLEX_MODE) == YT8512H_BCR_DUPLEX_MODE)
    {
      return YT8512H_STATUS_10MBITS_FULLDUPLEX;
    }
    else
    {
      return YT8512H_STATUS_10MBITS_HALFDUPLEX;
    }
  }
  else /* Auto Nego enabled */
  {
    if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_PHYSCSR, &readval) < 0)
    {
      return YT8512H_STATUS_READ_ERROR;
    }

    /* Check if auto nego not done */
    if((readval & YT8512H_PHYSCSR_AUTONEGO_DONE) == 0)
    {
      return YT8512H_STATUS_AUTONEGO_NOTDONE;
    }

    if((readval & YT8512H_PHYSCSR_HCDSPEEDMASK) == YT8512H_PHYSCSR_100BTX_FD)
    {
      return YT8512H_STATUS_100MBITS_FULLDUPLEX;
    }
    else if ((readval & YT8512H_PHYSCSR_HCDSPEEDMASK) == YT8512H_PHYSCSR_100BTX_HD)
    {
      return YT8512H_STATUS_100MBITS_HALFDUPLEX;
    }
    else if ((readval & YT8512H_PHYSCSR_HCDSPEEDMASK) == YT8512H_PHYSCSR_10BT_FD)
    {
      return YT8512H_STATUS_10MBITS_FULLDUPLEX;
    }
    else
    {
      return YT8512H_STATUS_10MBITS_HALFDUPLEX;
    }
  }
}

/**
  * @brief  Set the link state of YT8512H device.
  * @param  pObj: Pointer to device object.
  * @param  pLinkState: link state can be one of the following
  *         YT8512H_STATUS_100MBITS_FULLDUPLEX if 100Mb/s FD
  *         YT8512H_STATUS_100MBITS_HALFDUPLEX if 100Mb/s HD
  *         YT8512H_STATUS_10MBITS_FULLDUPLEX  if 10Mb/s FD
  *         YT8512H_STATUS_10MBITS_HALFDUPLEX  if 10Mb/s HD
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_ERROR  if parameter error
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_SetLinkState(yt8512h_Object_t *pObj, uint32_t LinkState)
{
  uint32_t bcrvalue = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &bcrvalue) >= 0)
  {
    /* Disable link config (Auto nego, speed and duplex) */
    bcrvalue &= ~(YT8512H_BCR_AUTONEGO_EN | YT8512H_BCR_SPEED_SELECT | YT8512H_BCR_DUPLEX_MODE);

    if(LinkState == YT8512H_STATUS_100MBITS_FULLDUPLEX)
    {
      bcrvalue |= (YT8512H_BCR_SPEED_SELECT | YT8512H_BCR_DUPLEX_MODE);
    }
    else if (LinkState == YT8512H_STATUS_100MBITS_HALFDUPLEX)
    {
      bcrvalue |= YT8512H_BCR_SPEED_SELECT;
    }
    else if (LinkState == YT8512H_STATUS_10MBITS_FULLDUPLEX)
    {
      bcrvalue |= YT8512H_BCR_DUPLEX_MODE;
    }
    else
    {
      /* Wrong link status parameter */
      status = YT8512H_STATUS_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  if(status == YT8512H_STATUS_OK)
  {
    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_BCR, bcrvalue) < 0)
    {
      status = YT8512H_STATUS_WRITE_ERROR;
    }
  }

  return status;
}

/**
  * @brief  Enable loopback mode.
  * @param  pObj: Pointer to device object.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_EnableLoopbackMode(yt8512h_Object_t *pObj)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &readval) >= 0)
  {
    readval |= YT8512H_BCR_LOOPBACK;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_BCR, readval) < 0)
    {
      status = YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Disable loopback mode.
  * @param  pObj: Pointer to device object.
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_DisableLoopbackMode(yt8512h_Object_t *pObj)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_BCR, &readval) >= 0)
  {
    readval &= ~YT8512H_BCR_LOOPBACK;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_BCR, readval) < 0)
    {
      status =  YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Enable IT source.
  * @param  pObj: Pointer to device object.
  * @param  Interrupt: IT source to be enabled
  *         should be a value or a combination of the following:
  *         YT8512H_WOL_IT
  *         YT8512H_ENERGYON_IT
  *         YT8512H_AUTONEGO_COMPLETE_IT
  *         YT8512H_REMOTE_FAULT_IT
  *         YT8512H_LINK_DOWN_IT
  *         YT8512H_AUTONEGO_LP_ACK_IT
  *         YT8512H_PARALLEL_DETECTION_FAULT_IT
  *         YT8512H_AUTONEGO_PAGE_RECEIVED_IT
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_EnableIT(yt8512h_Object_t *pObj, uint32_t Interrupt)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_IMR, &readval) >= 0)
  {
    readval |= Interrupt;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_IMR, readval) < 0)
    {
      status =  YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Disable IT source.
  * @param  pObj: Pointer to device object.
  * @param  Interrupt: IT source to be disabled
  *         should be a value or a combination of the following:
  *         YT8512H_WOL_IT
  *         YT8512H_ENERGYON_IT
  *         YT8512H_AUTONEGO_COMPLETE_IT
  *         YT8512H_REMOTE_FAULT_IT
  *         YT8512H_LINK_DOWN_IT
  *         YT8512H_AUTONEGO_LP_ACK_IT
  *         YT8512H_PARALLEL_DETECTION_FAULT_IT
  *         YT8512H_AUTONEGO_PAGE_RECEIVED_IT
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  *         YT8512H_STATUS_WRITE_ERROR if cannot write to register
  */
int32_t YT8512H_DisableIT(yt8512h_Object_t *pObj, uint32_t Interrupt)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_IMR, &readval) >= 0)
  {
    readval &= ~Interrupt;

    /* Apply configuration */
    if(pObj->IO.WriteReg(pObj->DevAddr, YT8512H_IMR, readval) < 0)
    {
      status = YT8512H_STATUS_WRITE_ERROR;
    }
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Clear IT flag.
  * @param  pObj: Pointer to device object.
  * @param  Interrupt: IT flag to be cleared
  *         should be a value or a combination of the following:
  *         YT8512H_WOL_IT
  *         YT8512H_ENERGYON_IT
  *         YT8512H_AUTONEGO_COMPLETE_IT
  *         YT8512H_REMOTE_FAULT_IT
  *         YT8512H_LINK_DOWN_IT
  *         YT8512H_AUTONEGO_LP_ACK_IT
  *         YT8512H_PARALLEL_DETECTION_FAULT_IT
  *         YT8512H_AUTONEGO_PAGE_RECEIVED_IT
  * @retval YT8512H_STATUS_OK  if OK
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  */
int32_t  YT8512H_ClearIT(yt8512h_Object_t *pObj, uint32_t Interrupt)
{
  uint32_t readval = 0;
  int32_t status = YT8512H_STATUS_OK;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_ISFR, &readval) < 0)
  {
    status =  YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

/**
  * @brief  Get IT Flag status.
  * @param  pObj: Pointer to device object.
  * @param  Interrupt: IT Flag to be checked,
  *         should be a value or a combination of the following:
  *         YT8512H_WOL_IT
  *         YT8512H_ENERGYON_IT
  *         YT8512H_AUTONEGO_COMPLETE_IT
  *         YT8512H_REMOTE_FAULT_IT
  *         YT8512H_LINK_DOWN_IT
  *         YT8512H_AUTONEGO_LP_ACK_IT
  *         YT8512H_PARALLEL_DETECTION_FAULT_IT
  *         YT8512H_AUTONEGO_PAGE_RECEIVED_IT
  * @retval 1 IT flag is SET
  *         0 IT flag is RESET
  *         YT8512H_STATUS_READ_ERROR if cannot read register
  */
int32_t YT8512H_GetITStatus(yt8512h_Object_t *pObj, uint32_t Interrupt)
{
  uint32_t readval = 0;
  int32_t status = 0;

  if(pObj->IO.ReadReg(pObj->DevAddr, YT8512H_ISFR, &readval) >= 0)
  {
    status = ((readval & Interrupt) == Interrupt);
  }
  else
  {
    status = YT8512H_STATUS_READ_ERROR;
  }

  return status;
}

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
