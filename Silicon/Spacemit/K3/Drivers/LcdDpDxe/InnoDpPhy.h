/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _INNO_DP_PHY_H_
#define _INNO_DP_PHY_H_

#include <Uefi.h>

#define EINVAL  -22

typedef enum {
  SOC_DP_LINK_RATE_1_62 = 1620000,
  SOC_DP_LINK_RATE_2_70 = 2700000,
  SOC_DP_LINK_RATE_5_40 = 5400000,
  SOC_DP_LINK_RATE_8_10 = 8100000,
} SOC_DP_LINK_RATE;

typedef enum {
  SOC_DP_LANE_1 = 1,
  SOC_DP_LANE_2 = 2,
  SOC_DP_LANE_4 = 4,
} SOC_DP_LANE_COUNT;

typedef struct {
  UINT32    LinkRate;
  UINT8     Lanes;

  UINT8     Voltage[4];
  UINT8     Pre[4];

  UINT8     SetRate;
  UINT8     SetLanes;
  UINT8     SetVoltages;
} SOC_DP_PHY_CONFIGURE_OPTS;

typedef struct {
  VOID      *Dev;
  UINTN     Regs;
  UINT32    RefClkKhz;

  UINT32    LinkRateKhz;
  INTN      LaneCount;

  INTN      PowerCount;
} SOC_DP_PHY;

INTN
SocDpPhyInit (
  IN SOC_DP_PHY  *Phy,
  IN UINTN       BaseAddr,
  IN UINT32      RefClkKhz
  );

INTN
SocDpPhyExit (
  IN SOC_DP_PHY  *Phy
  );

INTN
SocDpPhyPowerOn (
  IN SOC_DP_PHY  *Phy
  );

INTN
SocDpPhyPowerOff (
  IN SOC_DP_PHY  *Phy
  );

INTN
SocDpPhyConfigure (
  IN SOC_DP_PHY                 *Phy,
  IN SOC_DP_PHY_CONFIGURE_OPTS  *Opts
  );

INTN
SocDpPhySetPixelClk (
  IN SOC_DP_PHY  *Phy,
  IN UINT32      PixelClkKhz
  );

#endif
