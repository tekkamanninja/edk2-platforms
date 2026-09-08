/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TimerLib.h>
#include "SpacemitDsiHw.h"
#include "SpacemitDphy.h"

STATIC UINT32  mSpacemitDphyLane[5] = { 0, 0x1, 0x3, 0x7, 0xf };

STATIC
VOID
DphyAnaReset (
  VOID
  )
{
  DsiClearBits (DSI_PHY_ANA_PWR_CTRL, CFG_DPHY_ANA_RESET);
  gBS->Stall (5);
  DsiSetBits (DSI_PHY_ANA_PWR_CTRL, CFG_DPHY_ANA_RESET);
}

STATIC
VOID
DphySetPower (
  IN BOOLEAN  PowerOn
  )
{
  if (PowerOn) {
    DsiSetBits (DSI_PHY_ANA_PWR_CTRL, CFG_DPHY_ANA_RESET);
    DsiSetBits (DSI_PHY_ANA_PWR_CTRL, CFG_DPHY_ANA_PU);
  } else {
    DsiClearBits (DSI_PHY_ANA_PWR_CTRL, CFG_DPHY_ANA_PU);
    DsiClearBits (DSI_PHY_ANA_PWR_CTRL, CFG_DPHY_ANA_RESET);
  }
}

STATIC
VOID
DphySetContClk (
  IN BOOLEAN  ContClk
  )
{
  if (ContClk) {
    DsiSetBits (DSI_PHY_CTRL_1, CFG_DPHY_CONT_CLK);
  } else {
    DsiClearBits (DSI_PHY_CTRL_1, CFG_DPHY_CONT_CLK);
  }

  DsiSetBits (DSI_PHY_CTRL_1, CFG_DPHY_ADD_VALID);
  DsiSetBits (DSI_PHY_CTRL_1, CFG_DPHY_VDD_VALID);
}

STATIC
VOID
DphySetLaneNum (
  IN UINT32  LaneNum
  )
{
  DsiWriteBits (
                DSI_PHY_CTRL_2,
                CFG_DPHY_LANE_EN_MASK,
                mSpacemitDphyLane[LaneNum] << CFG_DPHY_LANE_EN_SHIFT
                );
}

STATIC
VOID
DphySetBitClkSrc (
  IN UINT32  BitClkSrc,
  IN UINT32  HalfPll5
  )
{
  if (BitClkSrc >= DPHY_BIT_CLK_SRC_MAX) {
    DEBUG ((DEBUG_INFO, "DphySetBitClkSrc: Invalid bit clk src (%d)\n", BitClkSrc));
    return;
  }
}

STATIC
VOID
DphySetTiming (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  )
{
  UINT32                BitClk, LpxClk, LpxTime, TaGet, TaGo;
  INT32                 Ui, Wakeup, Reg;
  INT32                 HsPrep, HsZero, HsTrail, HsExit, CkZero, CkTrail, CkExit;
  INT32                 EscClk, EscClkT;
  SPACEMIT_DPHY_TIMING  *PhyTiming;

  if (DphyCtx == NULL) {
    DEBUG ((DEBUG_INFO, "DphySetTiming: Invalid param!\n"));
    return;
  }

  PhyTiming = &(DphyCtx->DphyTiming);

  EscClk  = DphyCtx->EscClk / 1000;
  EscClkT = 1000 / EscClk;

  BitClk = DphyCtx->PhyFreq / 1000;
  Ui     = 1000 / BitClk + 1;

  DEBUG ((DEBUG_INFO, "DphySetTiming: esc_clk %d bit_clk %d\n", EscClk, BitClk));

  LpxClk  = (PhyTiming->LpxConstant + PhyTiming->LpxUi * Ui) / EscClkT + 1;
  LpxTime = LpxClk * EscClkT;

  /* Below is for NT35451 */
  TaGet = LpxTime * 5 / EscClkT - 1;
  TaGo  = LpxTime * 4 / EscClkT - 1;

  Wakeup = PhyTiming->WakeupConstant;
  Wakeup = Wakeup / EscClkT + 1;

  HsPrep = PhyTiming->HsPrepConstant + PhyTiming->HsPrepUi * Ui;
  HsPrep = HsPrep / EscClkT + 1;

  HsZero = PhyTiming->HsZeroConstant + PhyTiming->HsZeroUi * Ui -
           (HsPrep + 1) * EscClkT;
  HsZero = (HsZero - (3 * Ui << 3)) / EscClkT + 4;
  if (HsZero < 0) {
    HsZero = 0;
  }

  HsTrail = PhyTiming->HsTrailConstant + PhyTiming->HsTrailUi * Ui;
  HsTrail = ((8 * Ui) >= HsTrail) ? (8 * Ui) : HsTrail;
  HsTrail = HsTrail / EscClkT + 1;
  if (HsTrail > 3) {
    HsTrail -= 3;
  } else {
    HsTrail = 0;
  }

  HsExit = PhyTiming->HsExitConstant + PhyTiming->HsExitUi * Ui;
  HsExit = HsExit / EscClkT + 1;

  CkZero = PhyTiming->CkZeroConstant + PhyTiming->CkZeroUi * Ui -
           (HsPrep + 1) * EscClkT;
  CkZero = CkZero / EscClkT + 1;

  CkTrail = PhyTiming->CkTrailConstant + PhyTiming->CkTrailUi * Ui;
  CkTrail = CkTrail / EscClkT + 1;

  CkExit = HsExit;

  Reg = (HsExit << CFG_DPHY_TIME_HS_EXIT_SHIFT) |
        (HsTrail << CFG_DPHY_TIME_HS_TRAIL_SHIFT) |
        (HsZero << CFG_DPHY_TIME_HS_ZERO_SHIFT) |
        (HsPrep << CFG_DPHY_TIME_HS_PREP_SHIFT);

  DEBUG (
         (DEBUG_INFO, "DphySetTiming dphy time0 hs_exit %d hs_trail %d hs_zero %d hs_prep %d reg 0x%x\n",
          HsExit, HsTrail, HsZero, HsPrep, Reg)
         );
  DsiWrite (DSI_PHY_TIME_0, Reg);

  Reg = (TaGet << CFG_DPHY_TIME_TA_GET_SHIFT) |
        (TaGo << CFG_DPHY_TIME_TA_GO_SHIFT) |
        (Wakeup << CFG_DPHY_TIME_WAKEUP_SHIFT);

  DEBUG (
         (DEBUG_INFO, "DphySetTiming dphy time1 ta_get %d ta_go %d wakeup %d reg 0x%x\n",
          TaGet, TaGo, Wakeup, Reg)
         );
  DsiWrite (DSI_PHY_TIME_1, Reg);

  Reg = (CkExit << CFG_DPHY_TIME_CLK_EXIT_SHIFT) |
        (CkTrail << CFG_DPHY_TIME_CLK_TRAIL_SHIFT) |
        (CkZero << CFG_DPHY_TIME_CLK_ZERO_SHIFT) |
        (LpxClk << CFG_DPHY_TIME_CLK_LPX_SHIFT);

  DEBUG (
         (DEBUG_INFO, "DphySetTiming dphy time2 ck_exit %d ck_trail %d ck_zero %d lpx_clk %d reg 0x%x\n",
          CkExit, CkTrail, CkZero, LpxClk, Reg)
         );
  DsiWrite (DSI_PHY_TIME_2, Reg);

  Reg = (LpxClk << CFG_DPHY_TIME_LPX_SHIFT) |
        (PhyTiming->ReqReady << CFG_DPHY_TIME_REQRDY_SHIFT);

  DEBUG (
         (DEBUG_INFO, "DphySetTiming dphy time3 lpx_clk %d req_ready %d reg 0x%x\n",
          LpxClk, PhyTiming->ReqReady, Reg)
         );
  DsiWrite (DSI_PHY_TIME_3, Reg);
}

VOID
SpacemitDphyGetStatus (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  )
{
  if (DphyCtx == NULL) {
    DEBUG ((DEBUG_INFO, "SpacemitDphyGetStatus: Invalid param\n"));
    return;
  }

  DphyCtx->DphyStatus0 = DsiRead (DSI_PHY_STATUS_0);
  DphyCtx->DphyStatus1 = DsiRead (DSI_PHY_STATUS_1);
  DphyCtx->DphyStatus2 = DsiRead (DSI_PHY_STATUS_2);
}

VOID
SpacemitDphyReset (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  )
{
  if (DphyCtx == NULL) {
    DEBUG ((DEBUG_INFO, "SpacemitDphyReset: Invalid param\n"));
    return;
  }

  DphyAnaReset ();
}

VOID
SpacemitDphyInit (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  )
{
  if (DphyCtx == NULL) {
    DEBUG ((DEBUG_INFO, "SpacemitDphyInit: Invalid param\n"));
    return;
  }

  if (DPHY_STATUS_UNINIT != DphyCtx->Status) {
    DEBUG (
           (DEBUG_INFO, "SpacemitDphyInit: dphy_ctx has been initialized (%d)\n",
            DphyCtx->Status)
           );
    return;
  }

  /* Use DPHY_BIT_CLK_SRC_MUX as default clk src */
  DphySetBitClkSrc (DphyCtx->ClkSrc, DphyCtx->HalfPll5);

  /* Digital and analog power on */
  DphySetPower (TRUE);

  /* Turn on DSI continuous clock for HS */
  DphySetContClk (TRUE);

  /* Set dphy timing */
  DphySetTiming (DphyCtx);

  /* Enable data lanes */
  DphySetLaneNum (DphyCtx->LaneNum);

  DphyCtx->Status = DPHY_STATUS_INIT;

  /* Add delay for dsi phy stable */
  gBS->Stall (1000);  // 1ms delay
}

VOID
SpacemitDphyUninit (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  )
{
  if (DphyCtx == NULL) {
    DEBUG ((DEBUG_INFO, "SpacemitDphyUninit: Invalid param\n"));
    return;
  }

  if (DPHY_STATUS_INIT != DphyCtx->Status) {
    DEBUG (
           (DEBUG_INFO, "SpacemitDphyUninit: dphy_ctx has not been initialized (%d)\n",
            DphyCtx->Status)
           );
    return;
  }

  DphySetContClk (FALSE);
  DphyAnaReset ();
  DphySetPower (FALSE);

  DphyCtx->Status = DPHY_STATUS_UNINIT;
}
