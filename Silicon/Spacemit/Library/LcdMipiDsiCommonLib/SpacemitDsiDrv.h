/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SPACEMIT_DSI_DRV_H__
#define __SPACEMIT_DSI_DRV_H__

#include <Uefi.h>
#include "SpacemitDsiCommon.h"
#include "SpacemitDphy.h"

typedef enum {
  SpacemitDsiEventError,
  SpacemitDsiEventMax
} SPACEMIT_DSI_EVENT_ID;

typedef enum {
  DSI_STATUS_UNINIT = 0,
  DSI_STATUS_OPENED = 1,
  DSI_STATUS_INIT   = 2,
  DSI_STATUS_MAX
} SPACEMIT_DSI_STATUS;

typedef struct _SPACEMIT_DSI_DEVICE SPACEMIT_DSI_DEVICE;

typedef struct {
  EFI_STATUS (EFIAPI *DsiOpen)(
                               IN SPACEMIT_DSI_DEVICE        *Device,
                               IN SPACEMIT_MIPI_INFO         *MipiInfo,
                               IN BOOLEAN                    Ready
                               );

  EFI_STATUS (EFIAPI *DsiClose)(
                                IN SPACEMIT_DSI_DEVICE        *Device
                                );

  EFI_STATUS (EFIAPI *DsiWriteCmds)(
                                    IN SPACEMIT_DSI_DEVICE        *Device,
                                    IN SPACEMIT_DSI_CMD_DESC      *Commands,
                                    IN UINT32                      Count
                                    );

  EFI_STATUS (EFIAPI *DsiReadCmds)(
                                   IN SPACEMIT_DSI_DEVICE        *Device,
                                   IN SPACEMIT_DSI_RX_BUF        *RxBuffer,
                                   IN SPACEMIT_DSI_CMD_DESC      *Commands,
                                   IN UINT32                      Count
                                   );

  EFI_STATUS (EFIAPI *DsiReadyForDataTx)(
                                         IN SPACEMIT_DSI_DEVICE        *Device,
                                         IN SPACEMIT_MIPI_INFO         *MipiInfo
                                         );

  EFI_STATUS (EFIAPI *DsiCloseDataTx)(
                                      IN SPACEMIT_DSI_DEVICE        *Device
                                      );
} SPACEMIT_DSI_DRIVER_CONTEXT;

typedef struct {
  UINT32    LpmFrameEnable;
  UINT32    LastLineTurn;
  UINT32    HexSlotEnable;
  UINT32    HsaPacketEnable;
  UINT32    HsePacketEnable;
  UINT32    HbpPacketEnable;
  UINT32    HfpPacketEnable;
  UINT32    HexPacketEnable;
  UINT32    HlpPacketEnable;
  UINT32    AutoDelayDisable;
  UINT32    TimingCheckDisable;
  UINT32    HactWcEnable;
  UINT32    AutoWcDisable;
  UINT32    VsyncResetEnable;
} SPACEMIT_DSI_ADVANCED_SETTING;

struct _SPACEMIT_DSI_DEVICE {
  UINT32                           Id;
  UINT32                           Version;
  UINT64                           EscClkRate;
  UINT64                           BitClkRate;
  SPACEMIT_DSI_DRIVER_CONTEXT      *DriverCtx;
  SPACEMIT_DPHY_CTX                DphyConfig;
  SPACEMIT_DSI_ADVANCED_SETTING    AdvSetting;
  SPACEMIT_DSI_STATUS              Status;
};

#define MAX_DSI_NUM  1
extern SPACEMIT_DSI_DEVICE  *gSpacemitDsiList[MAX_DSI_NUM];

#endif /* __SPACEMIT_DSI_DRV_H__ */
