/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_VIDEO_TX_H_
#define _SPACEMIT_VIDEO_TX_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

#define INVALID_GPIO  0x0FFFFFFF
#define LCD_DUMMY     0xFFFF
#define DEFAULT_ID    0x1901

typedef enum {
  DPMS_OFF = 0,
  DPMS_ON  = 1
} DPMS_STATE;

typedef enum {
  LCD_MIPI = 0,
  LCD_HDMI = 1,
  LCD_EDP  = 2,
  LCD_DP   = 3,
  LCD_DPI  = 4,
  LCD_NULL
} PANEL_TYPE;

typedef enum {
  PIXFMT_RGB565 = 0,
  PIXFMT_RGB1555,
  PIXFMT_RGB888PACK,
  PIXFMT_RGB888UNPACK,
  PIXFMT_RGBA888,
  PIXFMT_YUV422_PACK,
  PIXFMT_YUV422P,
  PIXFMT_YUV420P,
  PIXFMT_RGB888A     = 0xB,
  PIXFMT_YUV420SP    = 0xC,
  PIXFMT_PSEUDOCOLOR = 0x200
} PIXEL_FORMAT;

typedef enum {
  DMA_FMT_RGB      = 0x0,
  DMA_FMT_YUV422P  = 0x4,
  DMA_FMT_YUV420P  = 0x6,   /* 3 planes */
  DMA_FMT_YUV420SP = 0x7    /* 2 planes */
} VDMA_FORMAT;

typedef struct {
  CHAR8     *Name;
  UINT32    Refresh;
  UINT32    XRes;
  UINT32    YRes;
  UINT32    RealXRes;
  UINT32    RealYRes;
  UINT32    LeftMargin;
  UINT32    RightMargin;
  UINT32    UpperMargin;
  UINT32    LowerMargin;
  UINT32    HsyncLen;
  UINT32    VsyncLen;
  UINT32    HsyncInvert;
  UINT32    VsyncInvert;
  UINT32    InvertPixclock;
  UINT32    PixclockFreq;
  INT32     PixFmtOut;
  UINT32    Height; /* screen height in mm */
  UINT32    Width;  /* screen width in mm */
  UINT32    Flags;
} SPACEMIT_MODE_INFO;

typedef struct {
  CHAR8                                   *LcdName;
  UINT32                                  LcdId;
  UINT32                                  PanelId0;
  UINT32                                  PanelId1;
  UINT32                                  PanelId2;
  UINT32                                  PowerValue;
  PANEL_TYPE                              PanelType;
  UINT32                                  WidthMm;
  UINT32                                  HeightMm;
  UINT32                                  DftPwmBl;
  UINT32                                  SetPowerCmdsNum;
  UINT32                                  ReadPowerCmdsNum;
  UINT32                                  SetIdCmdsNum;
  UINT32                                  SetBacklightValueCmdsNum;
  UINT32                                  SetBacklightPwmFreqCmdsNum;
  UINT32                                  ReadIdCmdsNum;
  UINT32                                  InitCmdsNum;
  UINT32                                  SleepOutCmdsNum;
  UINT32                                  SleepInCmdsNum;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *DrmModeInfo;
  SPACEMIT_MODE_INFO                      *SpacemitModeInfo;
  VOID                                    *MipiInfo;
  VOID                                    *SetPowerCmds;
  VOID                                    *SetBacklightValueCmds;
  VOID                                    *SetBacklightPwmFreqCmds;
  VOID                                    *ReadPowerCmds;
  VOID                                    *SetIdCmds;
  VOID                                    *ReadIdCmds;
  VOID                                    *InitCmds;
  VOID                                    *SleepOutCmds;
  VOID                                    *SleepInCmds;
  VOID (*SetBacklightValue)(
    INTN,
    INTN
    );
  UINT32                                  BitclkSel;
  UINT32                                  BitclkDiv;
  UINT32                                  PxclkSel;
  UINT32                                  PxclkDiv;
} LCD_MIPI_PANEL_INFO;

typedef struct _VIDEO_TX_DRIVER VIDEO_TX_DRIVER;

typedef struct {
  const VIDEO_TX_DRIVER    *Driver;
  PANEL_TYPE               PanelType;
  VOID                     *Private;
} VIDEO_TX_DEVICE;

struct _VIDEO_TX_DRIVER {
  INTN          (*GetModes)(
    VIDEO_TX_DEVICE *,
    SPACEMIT_MODE_INFO *
    );
  EFI_STATUS    (*Dpms)(
    VIDEO_TX_DEVICE *,
    INTN
    );
  EFI_STATUS    (*Identify)(
    VIDEO_TX_DEVICE *
    );
  BOOLEAN       (*EsdCheck)(
    VIDEO_TX_DEVICE *
    );
  EFI_STATUS    (*PanelReset)(
    VIDEO_TX_DEVICE *
    );
  EFI_STATUS    (*BlEnable)(
    VIDEO_TX_DEVICE *,
    BOOLEAN
    );
};

typedef struct {
  INTN                   DpmsStatus;
  PANEL_TYPE             PanelType;
  LCD_MIPI_PANEL_INFO    *PanelInfo;
  VOID                   *Priv;
} LCD_MIPI_TX_DATA;

typedef struct {
  CHAR8      PanelName[128];
  UINTN      Ldo1v2Gpio;
  UINT32     Ldo1v8;
  UINT32     Ldo2v8;
  UINT32     Ldo1v2;
  UINT32     BlPwm;
  UINTN      DcpGpio;
  UINTN      DcnGpio;
  UINTN      AveeGpio;
  UINTN      AvddGpio;
  UINTN      BlGpio;
  UINTN      EnableGpio;
  UINTN      ResetGpio;
  BOOLEAN    DcpValid;
  BOOLEAN    DcnValid;
  BOOLEAN    AveeValid;
  BOOLEAN    AvddValid;
  BOOLEAN    BlValid;
  BOOLEAN    EnableValid;
  BOOLEAN    ResetValid;
} SPACEMIT_PANEL_PRIV;

extern UINT32  LcdId;
extern UINT32  LcdWidth;
extern UINT32  LcdHeight;
extern CHAR8   *LcdName;

/* Host functions */
VIDEO_TX_DEVICE *
FindVideoTx (
  VOID
  );

INTN
VideoTxGetModes (
  VIDEO_TX_DEVICE     *VideoTx,
  SPACEMIT_MODE_INFO  *Modelist
  );

INTN
VideoTxDpms (
  VIDEO_TX_DEVICE  *VideoTx,
  INTN             Mode
  );

VOID
VideoTxEsdCheck (
  VIDEO_TX_DEVICE  *VideoTx
  );

VOID
VideoTxReset (
  VIDEO_TX_DEVICE  *VideoTx
  );

/* Client functions */
INTN
VideoTxRegisterDevice (
  VIDEO_TX_DEVICE  *TxDevice
  );

VOID *
VideoTxGetDrvData (
  VIDEO_TX_DEVICE  *TxDevice
  );

EFI_STATUS
LcdMipiRegisterPanel (
  LCD_MIPI_PANEL_INFO  *PanelInfo
  );

typedef struct {
  SPACEMIT_MODE_INFO    Mode;
  VIDEO_TX_DEVICE       *Tx;
} FB_INFO;

#endif /* _SPACEMIT_VIDEO_TX_H_ */
