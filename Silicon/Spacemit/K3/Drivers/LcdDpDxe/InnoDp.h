/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _INNO_DP_H_
#define _INNO_DP_H_

#include <Uefi.h>
#include "InnoDpPhy.h"

#define EDID_EXT_LENGTH       256
#define EDID_LENGTH           128
#define EDID_EXTENSION_FLAG   0x7e
#define DP_RECEIVER_CAP_SIZE  0xF

#define SOC_DP_MODE_FLAG_PHSYNC  BIT0
#define SOC_DP_MODE_FLAG_PVSYNC  BIT3

typedef enum {
  SOC_VIDEO_RGB_6BIT     = 0,
  SOC_VIDEO_RGB_8BIT     = 1,
  SOC_VIDEO_RGB_10BIT    = 2,
  SOC_VIDEO_RGB_12BIT    = 3,
  SOC_VIDEO_RGB_16BIT    = 4,
  SOC_VIDEO_YUV444_8BIT  = 5,
  SOC_VIDEO_YUV444_10BIT = 6,
  SOC_VIDEO_YUV444_12BIT = 7,
  SOC_VIDEO_YUV444_16BIT = 8,
  SOC_VIDEO_YUV422_8BIT  = 9,
  SOC_VIDEO_YUV422_10BIT = 10,
  SOC_VIDEO_YUV422_12BIT = 11,
  SOC_VIDEO_YUV422_16BIT = 12,
} SOC_VIDEO_FORMAT;

typedef enum {
  SOC_DP_REF_CLK_24M = 24000,
  SOC_DP_REF_CLK_50M = 50000,
} SOC_DP_REF_CLK;

typedef enum {
  ConnectorStatusDisconnected = 0,
  ConnectorStatusConnected    = 1,
  ConnectorStatusUnknown      = 2,
} SOC_DP_CONNECTOR_STATUS;

typedef struct {
  UINT32    Clock;
  UINT16    Hdisplay;
  UINT16    Htotal;
  UINT16    HsyncStart;
  UINT16    HsyncEnd;
  UINT16    Vdisplay;
  UINT16    Vtotal;
  UINT16    VsyncStart;
  UINT16    VsyncEnd;

  UINT32    Flags;
} SOC_DP_VIDEO_MODE;

typedef struct {
  SOC_DP_LINK_RATE     Rate;
  SOC_DP_LANE_COUNT    Lanes;
} SOC_DP_LINK_CONFIG;

typedef struct {
  VOID                 *Dev;
  UINTN                Regs;

  SOC_DP_PHY           Phy;

  UINT8                Dpcd[DP_RECEIVER_CAP_SIZE];

  UINT8                EdidData[EDID_EXT_LENGTH];
  SOC_DP_VIDEO_MODE    VideoMode;
  BOOLEAN              EdpMode;

  struct {
    UINT8     Revision;
    UINT8     EnhancedFraming;
    UINT32    MaxRate;
    UINT32    MaxNumLanes;
  } Link;

  UINT32                     ColorFormat;
  SOC_DP_CONNECTOR_STATUS    ConnectorStatus;
} SOC_DP_DEV;

INTN
SocDpInit (
  IN SOC_DP_DEV        *Dp,
  IN UINTN             BaseAddr,
  IN SOC_DP_REF_CLK    RefClkKhz,
  IN SOC_VIDEO_FORMAT  ColorFormat
  );

SOC_DP_CONNECTOR_STATUS
SocDpHwDetectHpd (
  IN SOC_DP_DEV  *Dp
  );

VOID
SocDpHwCleanHpd (
  IN SOC_DP_DEV  *Dp
  );

INTN
SocDpHwReadSinkCaps (
  IN SOC_DP_DEV  *Dp
  );

INTN
SocDpConnGetEdidBlock (
  IN SOC_DP_DEV  *Dp,
  OUT UINT8      *Buf,
  IN UINT32      Block,
  IN UINTN       Len
  );

INTN
SocDpModeSet (
  IN SOC_DP_DEV               *Dp,
  IN CONST SOC_DP_VIDEO_MODE  *Mode
  );

VOID
SocDpHwEnable (
  IN SOC_DP_DEV  *Dp
  );

VOID
SocDpHwDisable (
  IN SOC_DP_DEV  *Dp
  );

#endif
