/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_DSI_COMMON_H_
#define _SPACEMIT_DSI_COMMON_H_

#include <Uefi.h>
#include <Library/SpacemitDisplayLib.h>
#include <Library/LcdPcdConfig.h>

#define MAX_TX_CMD_COUNT   100
#define MAX_RX_DATA_COUNT  64

typedef enum {
  DSI_BURST_MODE_NON_BURST_SYNC_PULSE = 0,
  DSI_BURST_MODE_NON_BURST_SYNC_EVENT = 1,
  DSI_BURST_MODE_BURST                = 2,
  DSI_BURST_MODE_MAX
} SPACEMIT_MIPI_BURST_MODE;

typedef enum {
  DSI_INPUT_DATA_RGB_MODE_565         = 0,
  DSI_INPUT_DATA_RGB_MODE_666PACKET   = 1,
  DSI_INPUT_DATA_RGB_MODE_666UNPACKET = 2,
  DSI_INPUT_DATA_RGB_MODE_888         = 3,
  DSI_INPUT_DATA_RGB_MODE_MAX
} SPACEMIT_MIPI_INPUT_DATA_MODE;

typedef enum {
  SPACEMIT_DSI_MODE_VIDEO,
  SPACEMIT_DSI_MODE_CMD,
  SPACEMIT_DSI_MODE_MAX
} SPACEMIT_DSI_WORK_MODE;

typedef enum {
  SPACEMIT_DSI_DCS_SWRITE       = 0x5,
  SPACEMIT_DSI_DCS_SWRITE1      = 0x15,
  SPACEMIT_DSI_DCS_LWRITE       = 0x39,
  SPACEMIT_DSI_DCS_READ         = 0x6,
  SPACEMIT_DSI_GENERIC_LWRITE   = 0x29,
  SPACEMIT_DSI_GENERIC_READ1    = 0x14,
  SPACEMIT_DSI_SET_MAX_PKT_SIZE = 0x37,
} SPACEMIT_DSI_CMD_TYPE;

typedef enum {
  SPACEMIT_DSI_HS_MODE = 0,
  SPACEMIT_DSI_LP_MODE = 1,
} SPACEMIT_DSI_TX_MODE;

typedef enum {
  SPACEMIT_DSI_ACK_ERR_RESP   = 0x2,
  SPACEMIT_DSI_EOTP           = 0x8,
  SPACEMIT_DSI_GEN_READ1_RESP = 0x11,
  SPACEMIT_DSI_GEN_READ2_RESP = 0x12,
  SPACEMIT_DSI_GEN_LREAD_RESP = 0x1A,
  SPACEMIT_DSI_DCS_READ1_RESP = 0x21,
  SPACEMIT_DSI_DCS_READ2_RESP = 0x22,
  SPACEMIT_DSI_DCS_LREAD_RESP = 0x1C,
} SPACEMIT_DSI_RX_DATA_TYPE;

typedef enum {
  SPACEMIT_DSI_POLARITY_POS = 0,
  SPACEMIT_DSI_POLARITY_NEG,
  SPACEMIT_DSI_POLARITY_MAX
} SPACEMIT_DSI_POLARITY;

typedef enum {
  SPACEMIT_DSI_TE_MODE_NO = 0,
  SPACEMIT_DSI_TE_MODE_A,
  SPACEMIT_DSI_TE_MODE_B,
  SPACEMIT_DSI_TE_MODE_C,
  SPACEMIT_DSI_TE_MODE_MAX,
} SPACEMIT_DSI_TE_MODE;

typedef enum {
  DSI_VERSION_1 = 0,
  DSI_VERSION_2,
  DSI_VERSION_MAX
} SPACEMIT_DSI_VERSION;

typedef struct {
  UINT32    Height;
  UINT32    Width;
  UINT32    Hfp;
  UINT32    Hbp;
  UINT32    Hsync;
  UINT32    Vfp;
  UINT32    Vbp;
  UINT32    Vsync;
  UINT32    Fps;
  UINT32    WorkMode;
  UINT32    RgbMode;
  UINT32    LaneNumber;
  UINT32    PhyBitClock;
  UINT32    PhyEscClock;
  UINT32    SplitEnable;
  UINT32    EotpEnable;
  UINT32    BurstMode;
  UINT32    TeEnable;
  UINT32    VsyncPol;
  UINT32    TePol;
  UINT32    TeMode;
  UINT32    RealFps;
} SPACEMIT_MIPI_INFO;

typedef struct {
  SPACEMIT_DSI_CMD_TYPE    CmdType;
  UINT8                    Lp;
  UINT32                   Delay;
  UINT32                   Length;
  UINT8                    Data[MAX_TX_CMD_COUNT];
} SPACEMIT_DSI_CMD_DESC;

typedef struct {
  SPACEMIT_DSI_RX_DATA_TYPE    DataType;
  UINT32                       Length;
  UINT8                        Data[MAX_RX_DATA_COUNT];
} SPACEMIT_DSI_RX_BUF;
#pragma pack()

EFI_STATUS
EFIAPI
SpacemitMipiOpen (
  IN UINT32              Id,
  IN SPACEMIT_MIPI_INFO  *MipiInfo,
  IN BOOLEAN             Ready
  );

EFI_STATUS
EFIAPI
SpacemitMipiClose (
  IN UINT32  Id
  );

EFI_STATUS
EFIAPI
SpacemitMipiWriteCmds (
  IN UINT32                 Id,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  );

EFI_STATUS
EFIAPI
SpacemitMipiReadCmds (
  IN UINT32                 Id,
  OUT SPACEMIT_DSI_RX_BUF   *Dbuf,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  );

EFI_STATUS
EFIAPI
SpacemitMipiReadyForDataTx (
  IN UINT32              Id,
  IN SPACEMIT_MIPI_INFO  *MipiInfo
  );

EFI_STATUS
EFIAPI
SpacemitMipiCloseDataTx (
  IN UINT32  Id
  );

EFI_STATUS
EFIAPI
SpacemitDsiRegisterDevice (
  IN VOID  *Device
  );

EFI_STATUS
SpacemitDsiProbe (
  VOID
  );

EFI_STATUS
LcdMipiProbe (
  LCD_CONFIG_ARRAY    *LcdConfigs,
  DISPLAY_PANEL_NAME  *Panel
  );

/* LCD initialization functions */
EFI_STATUS
LcdIcnl9911cInit (
  VOID
  );

EFI_STATUS
LcdIcnl9951rInit (
  VOID
  );

EFI_STATUS
LcdGx09inx101Init (
  VOID
  );

EFI_STATUS
LcdJd9365dah3Init (
  VOID
  );

EFI_STATUS
LcdFt8201sinx101Init (
  VOID
  );

EFI_STATUS
LcdLt8911extEdp1080pInit (
  VOID
  );

EFI_STATUS
LcdHxdm101Init (
  VOID
  );

#endif /* _SPACEMIT_DSI_COMMON_H_ */
