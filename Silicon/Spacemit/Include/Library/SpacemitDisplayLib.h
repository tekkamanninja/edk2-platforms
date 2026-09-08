/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __SPACEMIT_DISPLAY_LIB_H__
#define __SPACEMIT_DISPLAY_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Include/Library/SpacemitVideoTx.h>
#include <Library/LcdPcdConfig.h>

#define MIPI_REG_BASE      (FixedPcdGet64(PcdSpacemitMipiDsiRegBase))
#define MIPI_DPU_REG_BASE  (FixedPcdGet64(PcdSpacemitMipiDpuRegBase))

enum BIT_DEPTH {
  EIGHT_BPP  = 0,
  TEN_BPP    = 1,
  TWELVE_BPP = 2,
};

#define VIDEO_BPP32_BITS_PER_PIXEL   (32)
#define VIDEO_BPP32_BYTES_PER_PIXEL  (VIDEO_BPP32_BITS_PER_PIXEL / 8)

typedef struct {
  VOID                  *Connector;
  VOID                  *Private;
  SPACEMIT_MODE_INFO    *SpacemitModeInfo;
  INT32                 OutputInterface;
  LCD_CONFIG_ARRAY      LcdConfigs;
} CONNECTOR_STATE;

typedef struct {
  VOID                    *Crtc;
  VOID                    *Private;
  UINTN                   FbSize;
  EFI_PHYSICAL_ADDRESS    *FbAddress;
  INT32                   DpuMode;
  INT32                   DpuId;
  INT32                   PinGroup;
} CRTC_STATE;

typedef struct {
  CRTC_STATE         CrtcState;
  CONNECTOR_STATE    ConnectorState;
  BOOLEAN            IsEnable;
} DISPLAY_STATE;

#endif
