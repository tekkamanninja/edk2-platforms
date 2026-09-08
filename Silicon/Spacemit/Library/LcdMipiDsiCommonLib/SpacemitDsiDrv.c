/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "SpacemitDsiCommon.h"
#include "SpacemitDphy.h"
#include "SpacemitDsiDrv.h"
#include "SpacemitDsiHw.h"

#define SPACEMIT_DSI_NAME     L"SpacemitDsi"
#define SPACEMIT_DSI_VERSION  L"1.0.0"

#define SPACEMIT_ESC_CLK_DEFAULT  51200000
#define SPACEMIT_BIT_CLK_DEFAULT  614400000
#define MIPI_CLK_MIN              800000000

#define SPACEMIT_DSI_MAX_TX_FIFO_BYTES   256
#define SPACEMIT_DSI_MAX_RX_FIFO_BYTES   64
#define SPACEMIT_DSI_MAX_CMD_FIFO_BYTES  1024

// Default values for various settings
#define LPM_FRAME_EN_DEFAULT           0
#define LAST_LINE_TURN_DEFAULT         0
#define HEX_SLOT_EN_DEFAULT            0
#define HSA_PKT_EN_DEFAULT_SYNC_PULSE  1
#define HSA_PKT_EN_DEFAULT_OTHER       0
#define HSE_PKT_EN_DEFAULT_SYNC_PULSE  1
#define HSE_PKT_EN_DEFAULT_OTHER       0
#define HBP_PKT_EN_DEFAULT             1
#define HFP_PKT_EN_DEFAULT             0
#define HEX_PKT_EN_DEFAULT             0
#define HLP_PKT_EN_DEFAULT             0
#define AUTO_DLY_DIS_DEFAULT           0
#define TIMING_CHECK_DIS_DEFAULT       0
#define HACT_WC_EN_DEFAULT             1
#define AUTO_WC_DIS_DEFAULT            0
#define VSYNC_RST_EN_DEFAULT           1

// Timing constants
#define HS_PREP_CONSTANT_DEFAULT   40
#define HS_PREP_UI_DEFAULT         4
#define HS_ZERO_CONSTANT_DEFAULT   145
#define HS_ZERO_UI_DEFAULT         10
#define HS_TRAIL_CONSTANT_DEFAULT  60
#define HS_TRAIL_UI_DEFAULT        4
#define HS_EXIT_CONSTANT_DEFAULT   100
#define HS_EXIT_UI_DEFAULT         0
#define CK_ZERO_CONSTANT_DEFAULT   300
#define CK_ZERO_UI_DEFAULT         0
#define CK_TRAIL_CONSTANT_DEFAULT  60
#define CK_TRAIL_UI_DEFAULT        0
#define REQ_READY_DEFAULT          0x3C
#define WAKEUP_CONSTANT_DEFAULT    1000000
#define WAKEUP_UI_DEFAULT          0
#define LPX_CONSTANT_DEFAULT       60
#define LPX_UI_DEFAULT             0
#define PLL5_VCO_DEF               1540000000
#define CLK_CALCU_DIFF             24
#define PXCLK_PLL5_DIV             3

#define ToDsiBcnt(timing, bpp)  (((timing) * (bpp)) >> 3)

STATIC CONST UINT32  SpacemitDsiLane[5] = { 0, 0x1, 0x3, 0x7, 0xf };

STATIC
UINT8
DsiGetBit (
  IN UINT32  Index,
  IN UINT8   *Pdata
  )
{
  UINT8   Ret;
  UINT32  Cindex, Bindex;

  Cindex = Index / 8;
  Bindex = Index % 8;

  if (Pdata[Cindex] & (0x1 << Bindex)) {
    Ret = 0x1;
  } else {
    Ret = 0x0;
  }

  return Ret;
}

STATIC
UINT8
CalculateEcc (
  IN UINT8  *Pdata
  )
{
  UINT8  Ret;
  UINT8  P[8];

  P[7] = 0x0;
  P[6] = 0x0;

  P[5] = (
          DsiGetBit (10, Pdata) ^
          DsiGetBit (11, Pdata) ^
          DsiGetBit (12, Pdata) ^
          DsiGetBit (13, Pdata) ^
          DsiGetBit (14, Pdata) ^
          DsiGetBit (15, Pdata) ^
          DsiGetBit (16, Pdata) ^
          DsiGetBit (17, Pdata) ^
          DsiGetBit (18, Pdata) ^
          DsiGetBit (19, Pdata) ^
          DsiGetBit (21, Pdata) ^
          DsiGetBit (22, Pdata) ^
          DsiGetBit (23, Pdata)
          );

  P[4] = (
          DsiGetBit (4, Pdata) ^
          DsiGetBit (5, Pdata) ^
          DsiGetBit (6, Pdata) ^
          DsiGetBit (7, Pdata) ^
          DsiGetBit (8, Pdata) ^
          DsiGetBit (9, Pdata) ^
          DsiGetBit (16, Pdata) ^
          DsiGetBit (17, Pdata) ^
          DsiGetBit (18, Pdata) ^
          DsiGetBit (19, Pdata) ^
          DsiGetBit (20, Pdata) ^
          DsiGetBit (22, Pdata) ^
          DsiGetBit (23, Pdata)
          );

  P[3] = (
          DsiGetBit (1, Pdata) ^
          DsiGetBit (2, Pdata) ^
          DsiGetBit (3, Pdata) ^
          DsiGetBit (7, Pdata) ^
          DsiGetBit (8, Pdata) ^
          DsiGetBit (9, Pdata) ^
          DsiGetBit (13, Pdata) ^
          DsiGetBit (14, Pdata) ^
          DsiGetBit (15, Pdata) ^
          DsiGetBit (19, Pdata) ^
          DsiGetBit (20, Pdata) ^
          DsiGetBit (21, Pdata) ^
          DsiGetBit (23, Pdata)
          );

  P[2] = (
          DsiGetBit (0, Pdata) ^
          DsiGetBit (2, Pdata) ^
          DsiGetBit (3, Pdata) ^
          DsiGetBit (5, Pdata) ^
          DsiGetBit (6, Pdata) ^
          DsiGetBit (9, Pdata) ^
          DsiGetBit (11, Pdata) ^
          DsiGetBit (12, Pdata) ^
          DsiGetBit (15, Pdata) ^
          DsiGetBit (18, Pdata) ^
          DsiGetBit (20, Pdata) ^
          DsiGetBit (21, Pdata) ^
          DsiGetBit (22, Pdata)
          );

  P[1] = (
          DsiGetBit (0, Pdata) ^
          DsiGetBit (1, Pdata) ^
          DsiGetBit (3, Pdata) ^
          DsiGetBit (4, Pdata) ^
          DsiGetBit (6, Pdata) ^
          DsiGetBit (8, Pdata) ^
          DsiGetBit (10, Pdata) ^
          DsiGetBit (12, Pdata) ^
          DsiGetBit (14, Pdata) ^
          DsiGetBit (17, Pdata) ^
          DsiGetBit (20, Pdata) ^
          DsiGetBit (21, Pdata) ^
          DsiGetBit (22, Pdata) ^
          DsiGetBit (23, Pdata)
          );

  P[0] = (
          DsiGetBit (0, Pdata) ^
          DsiGetBit (1, Pdata) ^
          DsiGetBit (2, Pdata) ^
          DsiGetBit (4, Pdata) ^
          DsiGetBit (5, Pdata) ^
          DsiGetBit (7, Pdata) ^
          DsiGetBit (10, Pdata) ^
          DsiGetBit (11, Pdata) ^
          DsiGetBit (13, Pdata) ^
          DsiGetBit (16, Pdata) ^
          DsiGetBit (20, Pdata) ^
          DsiGetBit (21, Pdata) ^
          DsiGetBit (22, Pdata) ^
          DsiGetBit (23, Pdata)
          );

  Ret = (UINT8)(
                P[0] |
                (P[1] << 0x1) |
                (P[2] << 0x2) |
                (P[3] << 0x3) |
                (P[4] << 0x4) |
                (P[5] << 0x5)
                );

  return Ret;
}

STATIC CONST UINT16  mCrc16GenerationCode = 0x8408;

/**
  Calculates CRC16 for the given data.

  @param[in]  Pdata   Pointer to the data buffer.
  @param[in]  Count   Number of bytes in the data buffer.

  @return     The calculated CRC16 value.
**/
STATIC
UINT16
CalculateCrc16 (
  IN UINT8   *Pdata,
  IN UINT16  Count
  )
{
  UINT16  ByteCounter;
  UINT8   BitCounter;
  UINT8   Data;
  UINT16  Crc16Result = 0xFFFF;

  if (Count > 0) {
    for (ByteCounter = 0; ByteCounter < Count; ByteCounter++) {
      Data = *(Pdata + ByteCounter);
      for (BitCounter = 0; BitCounter < 8; BitCounter++) {
        if (((Crc16Result & 0x0001) ^ ((0x0001 * Data) & 0x0001)) > 0) {
          Crc16Result = ((Crc16Result >> 1) & 0x7FFF) ^ mCrc16GenerationCode;
        } else {
          Crc16Result = (Crc16Result >> 1) & 0x7FFF;
        }

        Data = (Data >> 1) & 0x7F;
      }
    }
  }

  return Crc16Result;
}

STATIC VOID
DsiGetAdvancedSetting (
  IN SPACEMIT_DSI_DEVICE  *Dev,
  IN SPACEMIT_MIPI_INFO   *MipiInfo
  )
{
  SPACEMIT_DSI_ADVANCED_SETTING  *AdvSetting = &Dev->AdvSetting;

  AdvSetting->LpmFrameEnable = LPM_FRAME_EN_DEFAULT;
  AdvSetting->LastLineTurn   = LAST_LINE_TURN_DEFAULT;
  AdvSetting->HexSlotEnable  = HEX_SLOT_EN_DEFAULT;

  if (MipiInfo->BurstMode == DSI_BURST_MODE_NON_BURST_SYNC_PULSE) {
    AdvSetting->HsaPacketEnable = HSA_PKT_EN_DEFAULT_SYNC_PULSE;
  } else {
    AdvSetting->HsaPacketEnable = HSA_PKT_EN_DEFAULT_OTHER;
  }

  if (MipiInfo->BurstMode == DSI_BURST_MODE_NON_BURST_SYNC_PULSE) {
    AdvSetting->HsePacketEnable = HSE_PKT_EN_DEFAULT_SYNC_PULSE;
  } else {
    AdvSetting->HsePacketEnable = HSE_PKT_EN_DEFAULT_OTHER;
  }

  AdvSetting->HbpPacketEnable    = HBP_PKT_EN_DEFAULT;
  AdvSetting->HfpPacketEnable    = HFP_PKT_EN_DEFAULT;
  AdvSetting->HexPacketEnable    = HEX_PKT_EN_DEFAULT;
  AdvSetting->HlpPacketEnable    = HLP_PKT_EN_DEFAULT;
  AdvSetting->AutoDelayDisable   = AUTO_DLY_DIS_DEFAULT;
  AdvSetting->TimingCheckDisable = TIMING_CHECK_DIS_DEFAULT;
  AdvSetting->HactWcEnable       = HACT_WC_EN_DEFAULT;
  AdvSetting->AutoWcDisable      = AUTO_WC_DIS_DEFAULT;
  AdvSetting->VsyncResetEnable   = VSYNC_RST_EN_DEFAULT;
}

STATIC VOID
DsiGetDphySetting (
  IN SPACEMIT_DSI_DEVICE  *Dev
  )
{
  SPACEMIT_DPHY_TIMING  *DphyTiming;

  if (Dev == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Invalid parameter\n", __FUNCTION__));
    return;
  }

  DphyTiming = &Dev->DphyConfig.DphyTiming;

  DphyTiming->HsPrepConstant  = HS_PREP_CONSTANT_DEFAULT;
  DphyTiming->HsPrepUi        = HS_PREP_UI_DEFAULT;
  DphyTiming->HsZeroConstant  = HS_ZERO_CONSTANT_DEFAULT;
  DphyTiming->HsZeroUi        = HS_ZERO_UI_DEFAULT;
  DphyTiming->HsTrailConstant = HS_TRAIL_CONSTANT_DEFAULT;
  DphyTiming->HsTrailUi       = HS_TRAIL_UI_DEFAULT;
  DphyTiming->HsExitConstant  = HS_EXIT_CONSTANT_DEFAULT;
  DphyTiming->HsExitUi        = HS_EXIT_UI_DEFAULT;
  DphyTiming->CkZeroConstant  = CK_ZERO_CONSTANT_DEFAULT;
  DphyTiming->CkZeroUi        = CK_ZERO_UI_DEFAULT;
  DphyTiming->CkTrailConstant = CK_TRAIL_CONSTANT_DEFAULT;
  DphyTiming->CkTrailUi       = CK_TRAIL_UI_DEFAULT;
  DphyTiming->ReqReady        = REQ_READY_DEFAULT;
  DphyTiming->WakeupConstant  = WAKEUP_CONSTANT_DEFAULT;
  DphyTiming->WakeupUi        = WAKEUP_UI_DEFAULT;
  DphyTiming->LpxConstant     = LPX_CONSTANT_DEFAULT;
  DphyTiming->LpxUi           = LPX_UI_DEFAULT;
}

STATIC VOID
DsiReset (
  VOID
  )
{
  UINT32  reg;

  reg = CFG_SOFT_RST | CFG_SOFT_RST_REG | CFG_CLR_PHY_FIFO | CFG_RST_TXLP |
        CFG_RST_CPU | CFG_RST_CPN | CFG_RST_VPN | CFG_DSI_PHY_RST;

  DsiWrite (DSI_CTRL_0, reg);
  gBS->Stall (100);
  DsiWrite (DSI_CTRL_0, 0);
}

STATIC VOID
DsiEnableVideoMode (
  IN BOOLEAN  enable
  )
{
  if (enable) {
    DsiSetBits (DSI_CTRL_0, CFG_VPN_TX_EN | CFG_VPN_SLV | CFG_VPN_EN);
  } else {
    DsiClearBits (DSI_CTRL_0, CFG_VPN_TX_EN | CFG_VPN_EN);
  }
}

STATIC VOID
DsiEnableCmdMode (
  IN BOOLEAN  enable
  )
{
  if (enable) {
    DsiSetBits (DSI_CTRL_0, CFG_CPN_EN);
  } else {
    DsiClearBits (DSI_CTRL_0, CFG_CPN_EN);
  }
}

STATIC VOID
DsiEnableEotp (
  IN BOOLEAN  enable
  )
{
  if (enable) {
    DsiSetBits (DSI_CTRL_1, CFG_EOTP_EN);
  } else {
    DsiClearBits (DSI_CTRL_1, CFG_EOTP_EN);
  }
}

STATIC VOID
DsiEnableLptxLanes (
  IN UINT32  lane_num
  )
{
  DsiWriteBits (
                DSI_CPU_CMD_1,
                CFG_TXLP_LPDT_MASK,
                lane_num << CFG_TXLP_LPDT_SHIFT
                );
}

STATIC VOID
DsiEnableSplitMode (
  IN BOOLEAN  split_mode
  )
{
  if (split_mode) {
    DsiSetBits (DSI_LCD_BDG_CTRL0, CFG_SPLIT_EN);
  } else {
    DsiClearBits (DSI_LCD_BDG_CTRL0, CFG_SPLIT_EN);
  }
}

STATIC EFI_STATUS
DsiWriteCmd (
  IN UINT8    *parameter,
  IN UINT8    count,
  IN BOOLEAN  lp
  )
{
  UINT32   send_data = 0, reg, timeout, tmp, i;
  BOOLEAN  turnaround;
  UINT32   len;

  /* Write all packet bytes to packet data buffer */
  for (i = 0; i < count; i++) {
    send_data |= parameter[i] << ((i % 4) * 8);
    if ((i + 1) % 4 == 0) {
      DsiWrite (DSI_CPU_WDAT, send_data);
      reg = CFG_CPU_DAT_REQ | CFG_CPU_DAT_RW | ((i - 3) << CFG_CPU_DAT_ADDR_SHIFT);
      DsiWrite (DSI_CPU_CMD_3, reg);

      /* Wait write operation done */
      timeout = 1000;
      do {
        timeout--;
        tmp = DsiRead (DSI_CPU_CMD_3);
        gBS->Stall (1);
      } while ((tmp & CFG_CPU_DAT_REQ) && timeout);

      if (timeout == 0) {
        DEBUG ((DEBUG_INFO, "DSI write data to the packet data buffer not done.\n"));
      }

      send_data = 0;
    }
  }

  /* Handle last non-4Byte aligned data */
  if (i % 4 != 0) {
    DsiWrite (DSI_CPU_WDAT, send_data);
    reg = CFG_CPU_DAT_REQ | CFG_CPU_DAT_RW | ((4 * (i / 4)) << CFG_CPU_DAT_ADDR_SHIFT);
    DsiWrite (DSI_CPU_CMD_3, reg);

    /* Wait write operation done */
    timeout = 1000;
    do {
      timeout--;
      tmp = DsiRead (DSI_CPU_CMD_3);
      gBS->Stall (1);
    } while ((tmp & CFG_CPU_DAT_REQ) && timeout);

    if (timeout == 0) {
      DEBUG ((DEBUG_INFO, "DSI write data to the packet data buffer not done.\n"));
    }
  }

  turnaround = (parameter[0] == SPACEMIT_DSI_DCS_READ ||
                parameter[0] == SPACEMIT_DSI_GENERIC_READ1) ? TRUE : FALSE;

  len = count;

  reg = CFG_CPU_CMD_REQ |
        ((count == 4) ? CFG_CPU_SP : 0) |
        (turnaround ? CFG_CPU_TURN : 0) |
        (lp ? CFG_CPU_TXLP : 0) |
        (len << CFG_CPU_WC_SHIFT);

  /* Send out the packet */
  DsiWrite (DSI_CPU_CMD_0, reg);

  /* Wait packet be sent out */
  timeout = 1000;
  do {
    timeout--;
    tmp = DsiRead (DSI_CPU_CMD_0);
    gBS->Stall (20);
  } while ((tmp & CFG_CPU_CMD_REQ) && timeout);

  if (timeout == 0) {
    DEBUG ((DEBUG_ERROR, "%a: DSI send out packet maybe failed.\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC VOID
DsiConfigVideoMode (
  IN SPACEMIT_DSI_DEVICE  *DsiCtx,
  IN SPACEMIT_MIPI_INFO   *MipiInfo
  )
{
  UINT32                         HsyncB, HbpB, HactB, HexB, HfpB, HttlB;
  UINT32                         Hsync, Hbp, Hact, Httl, VTotal;
  UINT32                         HsaWc, HbpWc, HactWc, HexWc, HfpWc, HlpWc;
  UINT32                         Bpp, HssBcnt = 4, HseBct = 4, LgpOverHead = 6, Reg;
  UINT32                         SlotCnt0, SlotCnt1;
  UINT32                         DsiExPixelCnt = 0;
  UINT32                         DsiHexEn      = 0;
  UINT32                         Width, LaneNumber;
  SPACEMIT_DSI_ADVANCED_SETTING  *AdvSetting = &DsiCtx->AdvSetting;

  switch (MipiInfo->RgbMode) {
    case DSI_INPUT_DATA_RGB_MODE_565:
      Bpp = 16;
      break;
    case DSI_INPUT_DATA_RGB_MODE_666PACKET:
      Bpp = 18;
      break;
    case DSI_INPUT_DATA_RGB_MODE_666UNPACKET:
      Bpp = 18;
      break;
    case DSI_INPUT_DATA_RGB_MODE_888:
      Bpp = 24;
      break;
    default:
      Bpp = 24;
  }

  VTotal = MipiInfo->Height + MipiInfo->Vfp + MipiInfo->Vbp + MipiInfo->Vsync;

  if (MipiInfo->SplitEnable) {
    if ((0 != (MipiInfo->Width & 0x1)) || (0 != (MipiInfo->LaneNumber & 0x1))) {
      DEBUG (
             (DEBUG_INFO, "%a: warning:Invalid split config(lane = %d, width = %d)\n",
              __FUNCTION__, MipiInfo->LaneNumber, MipiInfo->Width)
             );
    }

    Width      = MipiInfo->Width >> 1;
    LaneNumber = MipiInfo->LaneNumber >> 1;
  } else {
    Width      = MipiInfo->Width;
    LaneNumber = MipiInfo->LaneNumber;
  }

  HactB    = ToDsiBcnt (Width, Bpp);
  HfpB     = ToDsiBcnt (MipiInfo->Hfp, Bpp);
  HbpB     = ToDsiBcnt (MipiInfo->Hbp, Bpp);
  HsyncB   = ToDsiBcnt (MipiInfo->Hsync, Bpp);
  HexB     = ToDsiBcnt (DsiExPixelCnt, Bpp);
  HttlB    = HactB + HsyncB + HfpB + HbpB + HexB;
  SlotCnt0 = (HttlB - HactB) / LaneNumber + 3;
  SlotCnt1 = SlotCnt0;

  Hact  = HactB / LaneNumber;
  Hbp   = HbpB / LaneNumber;
  Hsync = HsyncB / LaneNumber;
  Httl  = (HactB + HfpB + HbpB + HsyncB) / LaneNumber;

  /* word count in the unit of byte */
  HsaWc = (MipiInfo->BurstMode == DSI_BURST_MODE_NON_BURST_SYNC_PULSE) ?
          (HsyncB - HssBcnt - LgpOverHead) : 0;

  /* Hse is with backporch */
  HbpWc = (MipiInfo->BurstMode == DSI_BURST_MODE_NON_BURST_SYNC_PULSE) ?
          (HbpB - HseBct - LgpOverHead)
            : (HsyncB + HbpB - HssBcnt - LgpOverHead);

  HfpWc = ((MipiInfo->BurstMode == DSI_BURST_MODE_BURST) && (DsiHexEn == 0)) ?
          (HfpB + HexB - LgpOverHead - LgpOverHead) :
          (HfpB - LgpOverHead - LgpOverHead);

  HactWc = (Width * Bpp) >> 3;

  /* disable Hex currently */
  HexWc = 0;

  /* There is no hlp with active data segment. */
  HlpWc = (MipiInfo->BurstMode == DSI_BURST_MODE_NON_BURST_SYNC_PULSE) ?
          (HttlB - HsyncB - HseBct - LgpOverHead) :
          (HttlB - HssBcnt - LgpOverHead);

  if (MipiInfo->LaneNumber == 1) {
    DsiWrite (DSI_VPN_CTRL_0, (0x50 << 16) | 0xc12);
  } else {
    DsiWrite (DSI_VPN_CTRL_0, (0x50 << 16) | 0xc08);
  }

  /* SET UP LCD1 TIMING REGISTERS FOR DSI BUS */
  DsiWrite (DSI_VPN_TIMING_0, (Hact << 16) | Httl);
  DsiWrite (DSI_VPN_TIMING_1, (Hsync << 16) | Hbp);
  DsiWrite (DSI_VPN_TIMING_2, ((MipiInfo->Height) << 16) | (VTotal));
  DsiWrite (DSI_VPN_TIMING_3, ((MipiInfo->Vsync) << 16) | (MipiInfo->Vbp));

  /* SET UP LCD1 WORD COUNT REGISTERS FOR DSI BUS */
  DsiWrite (DSI_VPN_WC_0, (HbpWc << 16) | HsaWc);
  DsiWrite (DSI_VPN_WC_1, (HfpWc << 16) | HactWc);
  DsiWrite (DSI_VPN_WC_2, (HexWc << 16) | HlpWc);

  DsiWrite (DSI_VPN_SLOT_CNT_0, (SlotCnt0 << 16) | SlotCnt0);
  DsiWrite (DSI_VPN_SLOT_CNT_1, (SlotCnt1 << 16) | SlotCnt1);

  /* Configure LCD control register 1 FOR DSI BUS */
  Reg = AdvSetting->VsyncResetEnable << CFG_VPN_VSYNC_RST_EN_SHIFT |
        AdvSetting->AutoWcDisable << CFG_VPN_AUTO_WC_DIS_SHIFT |
        AdvSetting->HactWcEnable << CFG_VPN_HACT_WC_EN_SHIFT |
        AdvSetting->TimingCheckDisable << CFG_VPN_TIMING_CHECK_DIS_SHIFT |
        AdvSetting->AutoDelayDisable << CFG_VPN_AUTO_DLY_DIS_SHIFT |
        AdvSetting->HlpPacketEnable << CFG_VPN_HLP_PKT_EN_SHIFT |
        AdvSetting->HexPacketEnable << CFG_VPN_HEX_PKT_EN_SHIFT |
        AdvSetting->HfpPacketEnable << CFG_VPN_HFP_PKT_EN_SHIFT |
        AdvSetting->HbpPacketEnable << CFG_VPN_HBP_PKT_EN_SHIFT |
        AdvSetting->HsePacketEnable << CFG_VPN_HSE_PKT_EN_SHIFT |
        AdvSetting->HsaPacketEnable << CFG_VPN_HSA_PKT_EN_SHIFT |
        AdvSetting->HexPacketEnable << CFG_VPN_HEX_SLOT_EN_SHIFT |
        AdvSetting->LastLineTurn << CFG_VPN_LAST_LINE_TURN_SHIFT |
        AdvSetting->LpmFrameEnable << CFG_VPN_LPM_FRAME_EN_SHIFT |
        MipiInfo->BurstMode << CFG_VPN_BURST_MODE_SHIFT |
        MipiInfo->RgbMode << CFG_VPN_RGB_TYPE_SHIFT;
  DsiWrite (DSI_VPN_CTRL_1, Reg);

  DsiWriteBits (
                DSI_LCD_BDG_CTRL0,
                CFG_VPN_FIFO_AFULL_CNT_MASK,
                0 << CFG_VPN_FIFO_AFULL_CNT_SHIT
                );
  DsiSetBits (DSI_LCD_BDG_CTRL0, CFG_VPN_FIFO_AFULL_BYPASS);
  DsiSetBits (DSI_LCD_BDG_CTRL0, CFG_PIXEL_SWAP);

  DsiEnableCmdMode (FALSE);
  DsiEnableVideoMode (TRUE);
}

STATIC VOID
DsiConfigCmdMode (
  IN SPACEMIT_DSI_DEVICE  *DsiCtx,
  IN SPACEMIT_MIPI_INFO   *MipiInfo
  )
{
  UINT32  Reg;
  UINT32  RgbMode, Bpp;

  switch (MipiInfo->RgbMode) {
    case DSI_INPUT_DATA_RGB_MODE_565:
      Bpp     = 16;
      RgbMode = 2;
      break;
    case DSI_INPUT_DATA_RGB_MODE_666UNPACKET:
      Bpp     = 18;
      RgbMode = 1;
      break;
    case DSI_INPUT_DATA_RGB_MODE_888:
      Bpp     = 24;
      RgbMode = 0;
      break;
    default:
      DEBUG ((DEBUG_INFO, "%a: unsupported rgb format!\n", __FUNCTION__));
      Bpp     = 24;
      RgbMode = 0;
  }

  Reg = MipiInfo->TeEnable << CFG_CPN_TE_EN_SHIFT |
        RgbMode << CFG_CPN_RGB_TYPE_SHIFT |
        1 << CFG_CPN_BURST_MODE_SHIFT |
        0 << CFG_CPN_DMA_DIS_SHIFT |
        0 << CFG_CPN_ADDR0_EN_SHIFT;
  DsiWrite (DSI_CPN_CMD, Reg);

  Reg = MipiInfo->Width * Bpp / 8 << CFG_CPN_PKT_CNT_SHIFT |
        SPACEMIT_DSI_MAX_CMD_FIFO_BYTES << CFG_CPN_FIFO_FULL_LEVEL_SHIFT;
  DsiWrite (DSI_CPN_CTRL_1, Reg);

  DsiWriteBits (
                DSI_LCD_BDG_CTRL0,
                CFG_CPN_TE_EDGE_MASK,
                MipiInfo->TePol << CFG_CPN_TE_EDGE_SHIFT
                );
  DsiWriteBits (
                DSI_LCD_BDG_CTRL0,
                CFG_CPN_VSYNC_EDGE_MASK,
                MipiInfo->VsyncPol << CFG_CPN_VSYNC_EDGE_SHIFT
                );
  DsiWriteBits (
                DSI_LCD_BDG_CTRL0,
                CFG_CPN_TE_MODE_MASK,
                MipiInfo->TeMode << CFG_CPN_TE_MODE_SHIFT
                );

  Reg = 0x80 << CFG_CPN_TE_DLY_CNT_SHIFT |
        0 << CFG_CPN_TE_LINE_CNT_SHIFT;
  DsiWrite (DSI_LCD_BDG_CTRL1, Reg);

  DsiEnableVideoMode (FALSE);
  DsiEnableCmdMode (TRUE);
}

STATIC EFI_STATUS
DsiWriteCmdArray (
  IN SPACEMIT_DSI_DEVICE    *DsiCtx,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  )
{
  SPACEMIT_DSI_CMD_DESC  *CmdLine;
  UINT8                  Type, Parameter[SPACEMIT_DSI_MAX_TX_FIFO_BYTES];
  UINT8                  Len;
  UINT32                 Crc, Loop;
  EFI_STATUS             Status = EFI_SUCCESS;

  if (DsiCtx == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  for (Loop = 0; Loop < Count; Loop++) {
    CmdLine = &Cmds[Loop];
    Type    = CmdLine->CmdType;
    Len     = CmdLine->Length;
    SetMem (Parameter, Len + 6, 0x00);
    Parameter[0] = Type & 0xff;

    switch (Type) {
      case SPACEMIT_DSI_DCS_SWRITE:
      case SPACEMIT_DSI_DCS_SWRITE1:
      case SPACEMIT_DSI_DCS_READ:
      case SPACEMIT_DSI_GENERIC_READ1:
      case SPACEMIT_DSI_SET_MAX_PKT_SIZE:
        CopyMem (&Parameter[1], CmdLine->Data, Len);
        Len = 4;
        break;
      case SPACEMIT_DSI_GENERIC_LWRITE:
      case SPACEMIT_DSI_DCS_LWRITE:
        Parameter[1] = Len & 0xff;
        Parameter[2] = 0;
        CopyMem (&Parameter[4], CmdLine->Data, Len);
        Crc                = CalculateCrc16 (&Parameter[4], Len);
        Parameter[Len + 4] = Crc & 0xff;
        Parameter[Len + 5] = (Crc >> 8) & 0xff;
        Len               += 6;
        break;
      default:
        DEBUG ((DEBUG_INFO, "%a: data type not supported 0x%8x\n", __FUNCTION__, Type));
        break;
    }

    Parameter[3] = CalculateEcc (Parameter);

    /* send dsi commands */
    Status = DsiWriteCmd (Parameter, Len, CmdLine->Lp);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (CmdLine->Delay != 0) {
      gBS->Stall (CmdLine->Delay * 1000);
    }
  }

  return Status;
}

STATIC EFI_STATUS
DsiReadCmdArray (
  IN SPACEMIT_DSI_DEVICE    *DsiCtx,
  OUT SPACEMIT_DSI_RX_BUF   *Dbuf,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  )
{
  UINT8   Parameter[SPACEMIT_DSI_MAX_RX_FIFO_BYTES];
  UINT32  I, RxReg, Timeout, Tmp, Packet;
  UINT32  DataPointer, ByteCount;

  if (DsiCtx == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  SetMem (Dbuf, sizeof (SPACEMIT_DSI_RX_BUF), 0x0);
  DsiWriteCmdArray (DsiCtx, Cmds, Count);

  Timeout = 1000;
  do {
    Timeout--;
    Tmp = DsiRead (DSI_IRQ_ST);
  } while (((Tmp & IRQ_RX_PKT) == 0) && Timeout);

  if (Timeout == 0) {
    DEBUG ((DEBUG_INFO, "%a: dsi didn't receive packet, irq status 0x%x\n", __FUNCTION__, Tmp));
    return EFI_TIMEOUT;
  }

  if (Tmp & IRQ_RX_TRG3) {
    DEBUG ((DEBUG_INFO, "%a: not defined package is received\n", __FUNCTION__));
  }

  if (Tmp & IRQ_RX_TRG2) {
    DEBUG ((DEBUG_INFO, "%a: ACK package is received\n", __FUNCTION__));
  }

  if (Tmp & IRQ_RX_TRG1) {
    DEBUG ((DEBUG_INFO, "%a: TE trigger is received\n", __FUNCTION__));
  }

  if (Tmp & IRQ_RX_ERR) {
    Tmp = DsiRead (DSI_RX_PKT_HDR_0);
    DEBUG ((DEBUG_INFO, "%a: error: ACK with error report (0x%x)\n", __FUNCTION__, Tmp));
  }

  Packet      = DsiRead (DSI_RX_PKT_ST_0);
  DataPointer = (Packet & CFG_RX_PKT0_PTR_MASK) >> CFG_RX_PKT0_PTR_SHIFT;
  Tmp         = DsiRead (DSI_RX_PKT_CTRL_1);
  ByteCount   = Tmp & CFG_RX_PKT_BCNT_MASK;

  SetMem (Parameter, ByteCount, 0x00);
  for (I = DataPointer; I < DataPointer + ByteCount; I++) {
    RxReg  = DsiRead (DSI_RX_PKT_CTRL);
    RxReg &= ~CFG_RX_PKT_RD_PTR_MASK;
    RxReg |= CFG_RX_PKT_RD_REQ | (I << CFG_RX_PKT_RD_PTR_SHIFT);
    DsiWrite (DSI_RX_PKT_CTRL, RxReg);

    Timeout = 10000;
    do {
      Timeout--;
      RxReg = DsiRead (DSI_RX_PKT_CTRL);
    } while ((RxReg & CFG_RX_PKT_RD_REQ) && Timeout);

    if (Timeout == 0) {
      DEBUG ((DEBUG_INFO, "%a: error: read Rx packet FIFO error\n", __FUNCTION__));
    }

    Parameter[I - DataPointer] = RxReg & 0xff;
  }

  switch (Parameter[0]) {
    case SPACEMIT_DSI_ACK_ERR_RESP:
      DEBUG ((DEBUG_INFO, "%a: error: Acknowledge with error report\n", __FUNCTION__));
      break;
    case SPACEMIT_DSI_EOTP:
      DEBUG ((DEBUG_INFO, "%a: error: End of Transmission packet\n", __FUNCTION__));
      break;
    case SPACEMIT_DSI_GEN_READ1_RESP:
    case SPACEMIT_DSI_DCS_READ1_RESP:
      Dbuf->DataType = Parameter[0];
      Dbuf->Length   = 1;
      CopyMem (Dbuf->Data, &Parameter[1], Dbuf->Length);
      break;
    case SPACEMIT_DSI_GEN_READ2_RESP:
    case SPACEMIT_DSI_DCS_READ2_RESP:
      Dbuf->DataType = Parameter[0];
      Dbuf->Length   = 2;
      CopyMem (Dbuf->Data, &Parameter[1], Dbuf->Length);
      break;
    case SPACEMIT_DSI_GEN_LREAD_RESP:
    case SPACEMIT_DSI_DCS_LREAD_RESP:
      Dbuf->DataType = Parameter[0];
      Dbuf->Length   = (Parameter[2] << 8) | Parameter[1];
      CopyMem (Dbuf->Data, &Parameter[4], Dbuf->Length);
      break;
  }

  return EFI_SUCCESS;
}

STATIC VOID
DsiOpenDphy (
  IN SPACEMIT_DSI_DEVICE  *DeviceCtx,
  IN SPACEMIT_MIPI_INFO   *MipiInfo,
  IN BOOLEAN              Ready
  )
{
  SPACEMIT_DPHY_CTX  *DphyConfig = NULL;

  DsiGetDphySetting (DeviceCtx);
  DphyConfig          = &DeviceCtx->DphyConfig;
  DphyConfig->PhyFreq = DeviceCtx->BitClkRate / 1000;
  DphyConfig->EscClk  = DeviceCtx->EscClkRate / 1000;

  if (MipiInfo->SplitEnable) {
    DphyConfig->LaneNum = MipiInfo->LaneNumber >> 1;
  } else {
    DphyConfig->LaneNum = MipiInfo->LaneNumber;
  }

  DphyConfig->Status = DPHY_STATUS_UNINIT;

  if (Ready) {
    DphyConfig->Status = DPHY_STATUS_INIT;
    return;
  }

  SpacemitDphyInit (DphyConfig);
}

STATIC VOID
DsiCloseDphy (
  IN SPACEMIT_DSI_DEVICE  *DeviceCtx
  )
{
  SpacemitDphyUninit (&DeviceCtx->DphyConfig);
}

EFI_STATUS
SpacemitDsiOpen (
  IN SPACEMIT_DSI_DEVICE  *DeviceCtx,
  IN SPACEMIT_MIPI_INFO   *MipiInfo,
  IN BOOLEAN              Ready
  )
{
  UINT32  LaneNumber;

  if ((DeviceCtx == NULL) || (MipiInfo == NULL)) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DeviceCtx->BitClkRate = MipiInfo->PhyBitClock;
  DeviceCtx->EscClkRate = MipiInfo->PhyEscClock;

  if (MipiInfo->SplitEnable) {
    LaneNumber = MipiInfo->LaneNumber >> 1;
  } else {
    LaneNumber = MipiInfo->LaneNumber;
  }

  DsiGetAdvancedSetting (DeviceCtx, MipiInfo);

  if (!Ready) {
    DsiReset ();
  }

  DsiOpenDphy (DeviceCtx, MipiInfo, Ready);

  if (!Ready) {
    DsiEnableSplitMode (MipiInfo->SplitEnable);
    DsiEnableLptxLanes (SpacemitDsiLane[LaneNumber]);
    DsiEnableEotp (MipiInfo->EotpEnable);
  }

  DeviceCtx->Status = DSI_STATUS_OPENED;
  return EFI_SUCCESS;
}

EFI_STATUS
SpacemitDsiClose (
  IN SPACEMIT_DSI_DEVICE  *DeviceCtx
  )
{
  if (DeviceCtx == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DsiCloseDphy (DeviceCtx);
  DeviceCtx->Status = DSI_STATUS_UNINIT;
  return EFI_SUCCESS;
}

EFI_STATUS
SpacemitDsiReadyForDataTx (
  IN SPACEMIT_DSI_DEVICE  *DeviceCtx,
  IN SPACEMIT_MIPI_INFO   *MipiInfo
  )
{
  if ((DeviceCtx == NULL) || (MipiInfo == NULL)) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (MipiInfo->WorkMode == SPACEMIT_DSI_MODE_CMD) {
    DsiConfigCmdMode (DeviceCtx, MipiInfo);
  } else {
    DsiConfigVideoMode (DeviceCtx, MipiInfo);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
SpacemitDsiCloseDataTx (
  IN SPACEMIT_DSI_DEVICE  *DeviceCtx
  )
{
  if (DeviceCtx == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DsiEnableCmdMode (FALSE);
  DsiEnableVideoMode (FALSE);
  return EFI_SUCCESS;
}

EFI_STATUS
SpacemitDsiWriteCmds (
  IN SPACEMIT_DSI_DEVICE    *DeviceCtx,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  )
{
  if ((DeviceCtx == NULL) || (Cmds == NULL)) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  return DsiWriteCmdArray (DeviceCtx, Cmds, Count);
}

EFI_STATUS
SpacemitDsiReadCmds (
  IN SPACEMIT_DSI_DEVICE    *DeviceCtx,
  OUT SPACEMIT_DSI_RX_BUF   *Dbuf,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  )
{
  if ((DeviceCtx == NULL) || (Cmds == NULL)) {
    DEBUG ((DEBUG_INFO, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  return DsiReadCmdArray (DeviceCtx, Dbuf, Cmds, Count);
}

SPACEMIT_DSI_DRIVER_CONTEXT  DsiDriverCtx = {
  .DsiOpen           = SpacemitDsiOpen,
  .DsiClose          = SpacemitDsiClose,
  .DsiWriteCmds      = SpacemitDsiWriteCmds,
  .DsiReadCmds       = SpacemitDsiReadCmds,
  .DsiReadyForDataTx = SpacemitDsiReadyForDataTx,
  .DsiCloseDataTx    = SpacemitDsiCloseDataTx,
};

SPACEMIT_DSI_DEVICE  SpacemitDsiDev = { 0 };

EFI_STATUS
SpacemitDsiProbe (
  VOID
  )
{
  EFI_STATUS  Status;

  SpacemitDsiDev.DriverCtx = &DsiDriverCtx;
  SpacemitDsiDev.Status    = DSI_STATUS_UNINIT;
  SpacemitDsiDev.Id        = 0;

  SpacemitDsiDev.EscClkRate = SPACEMIT_ESC_CLK_DEFAULT;
  SpacemitDsiDev.BitClkRate = SPACEMIT_BIT_CLK_DEFAULT;

  Status = SpacemitDsiRegisterDevice (&SpacemitDsiDev);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: register dsi (%d) device fail!\n", __FUNCTION__, SpacemitDsiDev.Id));
    return Status;
  }

  SpacemitDsiDev.Status = DSI_STATUS_INIT;
  SpacemitDsiDev.Version = DSI_VERSION_2;
  return EFI_SUCCESS;
}
