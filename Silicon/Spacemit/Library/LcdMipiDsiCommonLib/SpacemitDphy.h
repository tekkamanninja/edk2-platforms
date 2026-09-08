/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_DPHY_H_
#define _SPACEMIT_DPHY_H_

#include <Uefi.h>

typedef enum {
  DPHY_LANE_MAP_0123 = 0,
  DPHY_LANE_MAP_0312 = 1,
  DPHY_LANE_MAP_0231 = 2,
  DPHY_LANE_MAP_MAX
} SPACEMIT_DPHY_LANE_MAP;

typedef enum {
  DPHY_STATUS_UNINIT = 0,
  DPHY_STATUS_INIT   = 1,
  DPHY_STATUS_MAX
} SPACEMIT_DPHY_STATUS;

typedef enum {
  DPHY_BIT_CLK_SRC_PLL5 = 1,
  DPHY_BIT_CLK_SRC_MUX  = 2,
  DPHY_BIT_CLK_SRC_MAX
} SPACEMIT_DPHY_BIT_CLK_SRC;

typedef struct {
  UINT32    HsPrepConstant;   /* Unit: ns */
  UINT32    HsPrepUi;
  UINT32    HsZeroConstant;
  UINT32    HsZeroUi;
  UINT32    HsTrailConstant;
  UINT32    HsTrailUi;
  UINT32    HsExitConstant;
  UINT32    HsExitUi;
  UINT32    CkZeroConstant;
  UINT32    CkZeroUi;
  UINT32    CkTrailConstant;
  UINT32    CkTrailUi;
  UINT32    ReqReady;
  UINT32    WakeupConstant;
  UINT32    WakeupUi;
  UINT32    LpxConstant;
  UINT32    LpxUi;
} SPACEMIT_DPHY_TIMING;

typedef struct {
  UINT32                       PhyFreq; /* kHz */
  UINT32                       LaneNum;
  UINT32                       EscClk; /* kHz */
  UINT32                       HalfPll5;
  SPACEMIT_DPHY_BIT_CLK_SRC    ClkSrc;
  SPACEMIT_DPHY_TIMING         DphyTiming;
  UINT32                       DphyStatus0; /* status0 reg */
  UINT32                       DphyStatus1; /* status1 reg */
  UINT32                       DphyStatus2; /* status2 reg */
  SPACEMIT_DPHY_STATUS         Status;
} SPACEMIT_DPHY_CTX;

VOID
SpacemitDphyInit (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  );

VOID
SpacemitDphyGetStatus (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  );

VOID
SpacemitDphyReset (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  );

VOID
SpacemitDphyUninit (
  IN SPACEMIT_DPHY_CTX  *DphyCtx
  );

#endif /* _SPACEMIT_DPHY_H_ */
