/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/ClockCtrl.h>
#include <Library/MemoryManagementLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Include/Library/SpacemitDpu.h>
#include <Include/Library/SpacemitVideoTx.h>
#include <Include/Library/SpacemitDisplayLib.h>
#include <Include/Protocol/SpacemitConnectorProtocol.h>
#include <Library/LcdMipiDsiCommonLib/SpacemitDsiCommon.h>
#define K3_LCD_POWER_CTRL  0x380

#define APMU_POWER_STATUS_REG   0xf0
#define APMU_Mipi_CLK_RES_CTRL  0x1B8
#define APMU_LCD_CLK_RES_CTRL1  0x44
#define APMU_LCD_CLK_RES_CTRL2  0x4c

#define PLL_CTRL_REG0    0xC0
#define PLL_CTRL_REG1    0xC4
#define PLL_CTRL_REG2    0xC8
#define PLL_CTRL_REG3    0xCC
#define PLL_CTRL_STATUS  0x230
#define APMU_BASE       (FixedPcdGet64(PcdSpacemitAPMURegBase))
#define APB_SPARE_BASE  (FixedPcdGet64(PcdSpacemitAPBSpareRegBase))

#define PLL_LK      BIT0
#define PLL_UP      BIT31
#define PLL_DIV_EN  (0xF << 4)

STATIC FB_INFO  mFbInfo = { 0 };

STATIC
VOID
DpuMipiMmioRemap (
  VOID
  )
{
  MapRegToGcdMmioSpace (MIPI_REG_BASE, SIZE_4KB);
  MapRegToGcdMmioSpace (MIPI_DPU_REG_BASE, SIZE_4KB);
  MapRegToGcdMmioSpace (MIPI_DPU_REG_BASE + 0x4000, SIZE_4KB);
  MapRegToGcdMmioSpace (MIPI_DPU_REG_BASE + 0x18000, SIZE_4KB);
  MapRegToGcdMmioSpace (APB_SPARE_BASE, SIZE_4KB);
  MapRegToGcdMmioSpace (APMU_BASE, SIZE_4KB);
}

EFI_STATUS
EnableDsipll (
  VOID
  )
{
  UINT32  Value;
  UINT32  Timeout = 100;

  UINT32  Reg0 = 0x8010C563;
  UINT32  Reg1 = 0x0BCEC4EC;
  UINT32  Reg2 = 0x10;

  MmioWrite32 (MIPI_REG_BASE + PLL_CTRL_REG0, Reg0);

  Value = Reg2;
  MmioWrite32 (MIPI_REG_BASE + PLL_CTRL_REG2, Value);

  Value = Reg1;
  MmioWrite32 (MIPI_REG_BASE + PLL_CTRL_REG1, Value);

  Value |= PLL_UP;
  MmioWrite32 (MIPI_REG_BASE + PLL_CTRL_REG1, Value);

  while (TRUE) {
    Value = MmioRead32 (MIPI_REG_BASE + PLL_CTRL_REG3);

    if ((Value & PLL_LK) == 1) {
      DEBUG ((DEBUG_INFO, "BitClk PLL locked successfully.\n"));
      break;
    }

    if (Timeout == 0) {
      DEBUG ((DEBUG_ERROR, "BitClk PLL lock timeout, last reg=0x%x\n", Value));
      return EFI_TIMEOUT;
    }

    Timeout--;
    gBS->Stall (10);
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
MipiClockInit (
  CONNECTOR_STATE  *ConnectorState
  )
{
  EFI_STATUS                  Status;
  IN UINT32                   Freq;
  SILICON_CLOCKCTRL_PROTOCOL  *ClockCtrlProtocol;
  LCD_CONFIG_ARRAY            *LcdCfg = &ConnectorState->LcdConfigs;

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID **)&ClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
  }

  /* lcd reset*/
  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    "LCDCLK",
                                    ENABLE_CLOCK
                                    );
  /* aclk reset*/
  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    "LCDACLK",
                                    ENABLE_CLOCK
                                    );
  /* mclk reset */
  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    "LCDMCLK",
                                    ENABLE_CLOCK
                                    );
  /* esc reset*/
  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    "LCDDSIESC",
                                    ENABLE_CLOCK
                                    );
  /* dsc reset*/
  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    "LCDDSC",
                                    ENABLE_CLOCK
                                    );

  /* enable hclk */
  ClockCtrlProtocol->SetClockState (
                                    ClockCtrlProtocol,
                                    "LCDHCLK",
                                    ENABLE_CLOCK
                                    );
  /* enable pxclk */
  Freq = LcdCfg->PixClk;
  if (Freq == 0) {
    Freq = 98000000;
  }

  ClockCtrlProtocol->SetClockRate (
                                   ClockCtrlProtocol,
                                   "LCDPIX",
                                   (UINT64) Freq
                                   );
  /* enable mclk */
  Freq = LcdCfg->MClk;
  if (Freq == 0) {
    Freq = 307200000;
  }

  ClockCtrlProtocol->SetClockRate (
                                   ClockCtrlProtocol,
                                   "LCDMCLK",
                                   (UINT64) Freq
                                   );
  /* enable escclk */
  Freq = LcdCfg->EscClk;
  if (Freq == 0) {
    Freq = 52000000;
  }

  ClockCtrlProtocol->SetClockRate (
                                   ClockCtrlProtocol,
                                   "ESCCLK",
                                   (UINT64) Freq
                                   );
  /* enable aclk */
  Freq = LcdCfg->AClk;
  if (Freq == 0) {
    Freq = 409000000;
  }

  ClockCtrlProtocol->SetClockRate (
                                   ClockCtrlProtocol,
                                   "ACLK",
                                   (UINT64) Freq
                                   );

  // dpu_enable_dsipll

  Status = EnableDsipll ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "EnableDsipll returned %r\n", Status));
  }

  /* enable dscclk */
  Freq = LcdCfg->DscClk;
  if (Freq == 0) {
    Freq = 307200000;
  }

  ClockCtrlProtocol->SetClockRate (
                                   ClockCtrlProtocol,
                                   "DSCCLK",
                                   (UINT64) Freq
                                   );

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
MipiPowerOn (
  VOID
  )
{
  INT32   loop;
  UINT32  Val;

  /* DSI */
  // enable dldo1

  /* LCD POWER_CTRL */
  Val  = MmioRead32 (APMU_BASE + K3_LCD_POWER_CTRL);
  Val |= (1 << 0);
  Val |= (1 << 4);
  MmioWrite32 (APMU_BASE + K3_LCD_POWER_CTRL, Val);

  gBS->Stall (310);

  for (loop = 10000; loop >= 0; --loop) {
    Val = MmioRead32 (APMU_BASE + APMU_POWER_STATUS_REG);
    if (Val & (1 << 4)) {
      break;
    }

    gBS->Stall (6);
  }

  if (loop < 0) {
    DEBUG ((DEBUG_ERROR, "power-on Mipi domain error\n"));
    return EFI_NO_RESPONSE;
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
SpacemitPanelInit (
  CONNECTOR_STATE  *ConnectorState
  )
{
  VIDEO_TX_DEVICE  *Tx      = NULL;
  UINT32           ModesNum = 0;

  Tx = FindVideoTx ();
  if (Tx == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to find video tx\n"));
    return EFI_NOT_FOUND;
  }

  mFbInfo.Tx                       = Tx;
  ModesNum                         = VideoTxGetModes (mFbInfo.Tx, &mFbInfo.Mode);
  ConnectorState->SpacemitModeInfo = &mFbInfo.Mode;

  if (ModesNum == 0) {
    DEBUG ((DEBUG_INFO, "Can't get videomode num\n"));
    return EFI_NOT_FOUND;
  }

  VideoTxDpms (mFbInfo.Tx, DPMS_ON);

  return EFI_SUCCESS;
}

EFI_STATUS
MipiDsiConnectorPreInit (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  EFI_STATUS  Ret;

  CONNECTOR_STATE  *ConnectorState = &DisplayState->ConnectorState;
  CRTC_STATE       *CrtcState      = &DisplayState->CrtcState;

  ConnectorState->OutputInterface = DpuModeMipi;
  CrtcState->DpuMode              = DpuModeMipi;

  LCD_CONFIG_ARRAY  *LcdCfg = (LCD_CONFIG_ARRAY *)PcdGetPtr (PcdLcdConfigs);
  CopyMem (&ConnectorState->LcdConfigs, LcdCfg, sizeof (LCD_CONFIG_ARRAY));

  Ret = LcdMipiProbe (&ConnectorState->LcdConfigs, PcdGetPtr (PcdDisplayPanelsPriority));

  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "%a: DSI probe failed - %r\n", __FUNCTION__, Ret));
    return Ret;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MipiDsiConnectorInit (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  EFI_STATUS       Ret;
  CONNECTOR_STATE  *ConnectorState = &DisplayState->ConnectorState;

  DpuMipiMmioRemap ();

  Ret = MipiPowerOn ();
  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "Mipi power on failed: %d", Ret));
    return EFI_DEVICE_ERROR;
  }

  Ret = MipiClockInit (ConnectorState);
  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "Mipi clock enable failed: %d", Ret));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MipiDsiConnectorPrepare (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  EFI_STATUS       Status;
  CONNECTOR_STATE  *ConnectorState = &DisplayState->ConnectorState;

  Status = SpacemitPanelInit (ConnectorState);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to init panel\n"));
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MipiDsiConnectorEnable (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  VIDEO_TX_DEVICE  *VideoTx;

  VideoTx = mFbInfo.Tx;

  if (VideoTx->PanelType == LCD_MIPI) {
    VideoTxEsdCheck (VideoTx);
    VideoTx->Driver->BlEnable (VideoTx, TRUE);
  } else if (VideoTx->PanelType == LCD_EDP) {
    VideoTxReset (VideoTx);

    VideoTx->Driver->BlEnable (VideoTx, TRUE);
  } else {
    DEBUG ((DEBUG_INFO, "Failed to find panel\n"));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

SPACEMIT_CONNECTOR_PROTOCOL  mMipiConnectorOps = {
  0,
  NULL,
  MipiDsiConnectorPreInit,
  MipiDsiConnectorInit,
  NULL,
  NULL,
  NULL,
  NULL,
  MipiDsiConnectorPrepare,
  MipiDsiConnectorEnable,
  NULL,
  NULL
};

EFI_STATUS
LcdInitMipi (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;

  Status = gBS->InstallMultipleProtocolInterfaces (
                                                   &Handle,
                                                   &gSpacemitLcdConnectorProtocolGuid,
                                                   &mMipiConnectorOps,
                                                   NULL
                                                   );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
