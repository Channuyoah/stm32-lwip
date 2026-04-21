/**
  ******************************************************************************
  * @file    yt8512.c
  * @brief   Minimal YT8512 PHY driver based on Clause 22 registers.
  ******************************************************************************
  */

#include "yt8512.h"
#include <stddef.h>

#define YT8512_MAX_DEV_ADDR                  ((uint32_t)31U)
#define YT8512_SW_RESET_TO                   ((uint32_t)500U)
#define YT8512_PHY_ID_INVALID                ((uint32_t)0x0000U)
#define YT8512_PHY_ID_INVALID_ALL_ONES       ((uint32_t)0xFFFFU)

static int32_t YT8512_GetLinkStateByAn(yt8512_Object_t *pObj)
{
  uint32_t anar = 0;
  uint32_t anlpar = 0;
  uint32_t common = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_ANAR, &anar) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_ANLPAR, &anlpar) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  common = anar & anlpar;

  if ((common & YT8512_ANAR_100BASE_TX_FD) != 0U)
  {
    return YT8512_STATUS_100MBITS_FULLDUPLEX;
  }
  if ((common & YT8512_ANAR_100BASE_TX) != 0U)
  {
    return YT8512_STATUS_100MBITS_HALFDUPLEX;
  }
  if ((common & YT8512_ANAR_10BASE_T_FD) != 0U)
  {
    return YT8512_STATUS_10MBITS_FULLDUPLEX;
  }
  if ((common & YT8512_ANAR_10BASE_T) != 0U)
  {
    return YT8512_STATUS_10MBITS_HALFDUPLEX;
  }

  return YT8512_STATUS_AUTONEGO_NOTDONE;
}

int32_t YT8512_RegisterBusIO(yt8512_Object_t *pObj, yt8512_IOCtx_t *ioctx)
{
  if (!pObj || !ioctx || !ioctx->ReadReg || !ioctx->WriteReg || !ioctx->GetTick)
  {
    return YT8512_STATUS_ERROR;
  }

  pObj->IO.Init = ioctx->Init;
  pObj->IO.DeInit = ioctx->DeInit;
  pObj->IO.ReadReg = ioctx->ReadReg;
  pObj->IO.WriteReg = ioctx->WriteReg;
  pObj->IO.GetTick = ioctx->GetTick;

  return YT8512_STATUS_OK;
}

int32_t YT8512_Init(yt8512_Object_t *pObj)
{
  uint32_t addr = 0;
  uint32_t id1 = 0;
  uint32_t id2 = 0;
  uint32_t bcr = 0;
  uint32_t tickstart = 0;

  if (pObj == NULL)
  {
    return YT8512_STATUS_ERROR;
  }

  if (pObj->Is_Initialized != 0U)
  {
    return YT8512_STATUS_OK;
  }

  if (pObj->IO.Init != 0)
  {
    pObj->IO.Init();
  }

  pObj->DevAddr = YT8512_MAX_DEV_ADDR + 1U;

  for (addr = 0; addr <= YT8512_MAX_DEV_ADDR; addr++)
  {
    if (pObj->IO.ReadReg(addr, YT8512_PHYI1R, &id1) < 0)
    {
      continue;
    }

    if (pObj->IO.ReadReg(addr, YT8512_PHYI2R, &id2) < 0)
    {
      continue;
    }

    if ((id1 != YT8512_PHY_ID_INVALID) && (id1 != YT8512_PHY_ID_INVALID_ALL_ONES) &&
        (id2 != YT8512_PHY_ID_INVALID) && (id2 != YT8512_PHY_ID_INVALID_ALL_ONES))
    {
      pObj->DevAddr = addr;
      break;
    }
  }

  if (pObj->DevAddr > YT8512_MAX_DEV_ADDR)
  {
    return YT8512_STATUS_ADDRESS_ERROR;
  }

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &bcr) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  bcr |= YT8512_BCR_SOFT_RESET;
  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, bcr) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  tickstart = pObj->IO.GetTick();
  do
  {
    if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &bcr) < 0)
    {
      return YT8512_STATUS_READ_ERROR;
    }

    if ((bcr & YT8512_BCR_SOFT_RESET) == 0U)
    {
      pObj->Is_Initialized = 1U;
      return YT8512_STATUS_OK;
    }
  } while ((pObj->IO.GetTick() - tickstart) <= YT8512_SW_RESET_TO);

  return YT8512_STATUS_RESET_TIMEOUT;
}

int32_t YT8512_DeInit(yt8512_Object_t *pObj)
{
  if (pObj == NULL)
  {
    return YT8512_STATUS_ERROR;
  }

  if (pObj->Is_Initialized != 0U)
  {
    if ((pObj->IO.DeInit != 0) && (pObj->IO.DeInit() < 0))
    {
      return YT8512_STATUS_ERROR;
    }

    pObj->Is_Initialized = 0U;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_DisablePowerDownMode(yt8512_Object_t *pObj)
{
  uint32_t val = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &val) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  val &= ~YT8512_BCR_POWER_DOWN;
  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, val) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_EnablePowerDownMode(yt8512_Object_t *pObj)
{
  uint32_t val = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &val) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  val |= YT8512_BCR_POWER_DOWN;
  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, val) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_StartAutoNego(yt8512_Object_t *pObj)
{
  uint32_t val = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &val) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  val |= (YT8512_BCR_AUTONEGO_EN | YT8512_BCR_RESTART_AUTONEGO);
  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, val) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_GetLinkState(yt8512_Object_t *pObj)
{
  uint32_t bsr = 0;
  uint32_t bcr = 0;
  uint32_t physcsr = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BSR, &bsr) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  /* BSR link bit is latch-low on some PHYs: read twice */
  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BSR, &bsr) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  if ((bsr & YT8512_BSR_LINK_STATUS) == 0U)
  {
    return YT8512_STATUS_LINK_DOWN;
  }

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &bcr) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  if ((bcr & YT8512_BCR_AUTONEGO_EN) != 0U)
  {
    if ((bsr & YT8512_BSR_AUTONEGO_CPLT) == 0U)
    {
      return YT8512_STATUS_AUTONEGO_NOTDONE;
    }

    /* Try PHY specific speed/duplex decode first */
    if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_PHYSCSR, &physcsr) >= 0)
    {
      uint32_t speed = physcsr & YT8512_PHYSCSR_SPEED_MASK;
      uint32_t fd = physcsr & YT8512_PHYSCSR_DUPLEX;

      if (speed == YT8512_PHYSCSR_SPEED_100M)
      {
        return (fd != 0U) ? YT8512_STATUS_100MBITS_FULLDUPLEX : YT8512_STATUS_100MBITS_HALFDUPLEX;
      }
      if (speed == YT8512_PHYSCSR_SPEED_10M)
      {
        return (fd != 0U) ? YT8512_STATUS_10MBITS_FULLDUPLEX : YT8512_STATUS_10MBITS_HALFDUPLEX;
      }
    }

    /* Fallback: derive from ANAR/ANLPAR common ability */
    return YT8512_GetLinkStateByAn(pObj);
  }

  if ((bcr & YT8512_BCR_SPEED_SELECT) != 0U)
  {
    return ((bcr & YT8512_BCR_DUPLEX_MODE) != 0U) ? YT8512_STATUS_100MBITS_FULLDUPLEX : YT8512_STATUS_100MBITS_HALFDUPLEX;
  }

  return ((bcr & YT8512_BCR_DUPLEX_MODE) != 0U) ? YT8512_STATUS_10MBITS_FULLDUPLEX : YT8512_STATUS_10MBITS_HALFDUPLEX;
}

int32_t YT8512_SetLinkState(yt8512_Object_t *pObj, uint32_t LinkState)
{
  uint32_t bcr = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &bcr) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  bcr &= ~(YT8512_BCR_AUTONEGO_EN | YT8512_BCR_SPEED_SELECT | YT8512_BCR_DUPLEX_MODE);

  switch (LinkState)
  {
    case YT8512_STATUS_100MBITS_FULLDUPLEX:
      bcr |= (YT8512_BCR_SPEED_SELECT | YT8512_BCR_DUPLEX_MODE);
      break;
    case YT8512_STATUS_100MBITS_HALFDUPLEX:
      bcr |= YT8512_BCR_SPEED_SELECT;
      break;
    case YT8512_STATUS_10MBITS_FULLDUPLEX:
      bcr |= YT8512_BCR_DUPLEX_MODE;
      break;
    case YT8512_STATUS_10MBITS_HALFDUPLEX:
      break;
    default:
      return YT8512_STATUS_ERROR;
  }

  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, bcr) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_EnableLoopbackMode(yt8512_Object_t *pObj)
{
  uint32_t val = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &val) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  val |= YT8512_BCR_LOOPBACK;
  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, val) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_DisableLoopbackMode(yt8512_Object_t *pObj)
{
  uint32_t val = 0;

  if (pObj->IO.ReadReg(pObj->DevAddr, YT8512_BCR, &val) < 0)
  {
    return YT8512_STATUS_READ_ERROR;
  }

  val &= ~YT8512_BCR_LOOPBACK;
  if (pObj->IO.WriteReg(pObj->DevAddr, YT8512_BCR, val) < 0)
  {
    return YT8512_STATUS_WRITE_ERROR;
  }

  return YT8512_STATUS_OK;
}

int32_t YT8512_EnableIT(yt8512_Object_t *pObj, uint32_t Interrupt)
{
  (void)pObj;
  (void)Interrupt;
  return YT8512_STATUS_OK;
}

int32_t YT8512_DisableIT(yt8512_Object_t *pObj, uint32_t Interrupt)
{
  (void)pObj;
  (void)Interrupt;
  return YT8512_STATUS_OK;
}

int32_t YT8512_ClearIT(yt8512_Object_t *pObj, uint32_t Interrupt)
{
  (void)pObj;
  (void)Interrupt;
  return YT8512_STATUS_OK;
}

int32_t YT8512_GetITStatus(yt8512_Object_t *pObj, uint32_t Interrupt)
{
  (void)pObj;
  (void)Interrupt;
  return 0;
}
