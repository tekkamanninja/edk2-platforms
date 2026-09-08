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
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/EmbeddedGpio.h>
#include <Include/Library/SpacemitVideoTx.h>
#include <Include/Library/SpacemitDisplayLib.h>
#include <Library/LcdPcdConfig.h>
#include "SpacemitDsiCommon.h"

#define PANEL_NUM_MAX  5
UINT32               PanelNum               = 0;
LCD_MIPI_PANEL_INFO  *Panels[PANEL_NUM_MAX] = { 0 };
UINT32               LcdId                  = 0;
UINT32               LcdWidth               = 0;
UINT32               LcdHeight              = 0;
CHAR8                *LcdName               = NULL;

STATIC EMBEDDED_GPIO  *mGpio           = NULL;
STATIC BOOLEAN        mGpioInitialized = FALSE;

STATIC LCD_GPIO_CONFIG  *DcpGpio   = NULL;
STATIC LCD_GPIO_CONFIG  *DcnGpio   = NULL;
STATIC LCD_GPIO_CONFIG  *ResetGpio = NULL;
STATIC LCD_GPIO_CONFIG  *BlGpio    = NULL;

STATIC
EFI_STATUS
InitializeGpioProtocol (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *HandleBuffer;
  UINTN       HandleCount;
  UINTN       Index;

  if (mGpioInitialized) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEmbeddedGpioProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LED: Failed to locate GPIO protocol: %r\n", Status));
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEmbeddedGpioProtocolGuid,
                                  (VOID **)&mGpio
                                  );
    if (!EFI_ERROR (Status)) {
      break;
    }
  }

  gBS->FreePool (HandleBuffer);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LED: Failed to get GPIO protocol: %r\n", Status));
    return Status;
  }

  mGpioInitialized = TRUE;
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
EFIAPI
LcdMipiReadId (
  IN LCD_MIPI_TX_DATA  *VideoTxClient
  )
{
  SPACEMIT_DSI_RX_BUF  DsiRxBuffer;
  UINT32               ReadId[3] = { 0 };
  INTN                 Ret;
  UINTN                i;

  if ((VideoTxClient == NULL) || (VideoTxClient->PanelInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter!\n", __FUNCTION__));
    return FALSE;
  }

  for (i = 0; i < 1; i++) {
    SpacemitMipiWriteCmds (
                           0,
                           VideoTxClient->PanelInfo->SetIdCmds,
                           VideoTxClient->PanelInfo->SetIdCmdsNum
                           );

    Ret = SpacemitMipiReadCmds (
                                0,
                                &DsiRxBuffer,
                                VideoTxClient->PanelInfo->ReadIdCmds,
                                VideoTxClient->PanelInfo->ReadIdCmdsNum
                                );

    if (Ret != 0) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to read panel ID!\n", __FUNCTION__));
      return FALSE;
    }

    ReadId[0] = DsiRxBuffer.Data[0];
    ReadId[1] = DsiRxBuffer.Data[1];
    ReadId[2] = DsiRxBuffer.Data[2];

    if ((ReadId[0] != VideoTxClient->PanelInfo->PanelId0) ||
        (ReadId[1] != VideoTxClient->PanelInfo->PanelId1) ||
        (ReadId[2] != VideoTxClient->PanelInfo->PanelId2))
    {
      DEBUG (
             (DEBUG_INFO,
              "Panel ID mismatch: Read 0x%x, 0x%x, 0x%x (Expected 0x%x, 0x%x, 0x%x)\n",
              ReadId[0], ReadId[1], ReadId[2],
              VideoTxClient->PanelInfo->PanelId0,
              VideoTxClient->PanelInfo->PanelId1,
              VideoTxClient->PanelInfo->PanelId2)
             );
    } else {
      DEBUG (
             (DEBUG_INFO,
              "Panel ID verified: 0x%x, 0x%x, 0x%x\n",
              ReadId[0], ReadId[1], ReadId[2])
             );
      return TRUE;
    }
  }

  return FALSE;
}

STATIC INTN
LcdMipiReset (
  SPACEMIT_PANEL_PRIV  *priv
  )
{
  EFI_STATUS  Status;

  UINT32  ResetGpioPin;

  ResetGpioPin = ResetGpio->GpioPin;

  Status = mGpio->Set (mGpio, ResetGpioPin, GPIO_MODE_OUTPUT_1);
  gBS->Stall (10000);
  Status = mGpio->Set (mGpio, ResetGpioPin, GPIO_MODE_OUTPUT_0);
  gBS->Stall (10000);
  Status = mGpio->Set (mGpio, ResetGpioPin, GPIO_MODE_OUTPUT_1);
  DEBUG ((DEBUG_INFO, "LcdMipiReset pin: %d on\n", ResetGpio->GpioPin));
  return 0;
}

STATIC EFI_STATUS
EFIAPI
LcdMipiDcEnable (
  IN BOOLEAN              PowerOn,
  IN SPACEMIT_PANEL_PRIV  *Priv
  )
{
  UINT32      DcpGpioPin;
  UINT32      DcnGpioPin;
  EFI_STATUS  Status;

  if (Priv == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid panel private data\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DcpGpioPin = DcpGpio->GpioPin;
  DcnGpioPin = DcnGpio->GpioPin;

  if (PowerOn) {
    DEBUG ((DEBUG_INFO, "LcdMipiDcEnable pin: %d on\n", DcnGpio->GpioPin));

    Status = mGpio->Set (mGpio, DcpGpioPin, GPIO_MODE_OUTPUT_1);
    Status = mGpio->Set (mGpio, DcnGpioPin, GPIO_MODE_OUTPUT_1);
  } else {
    Status = mGpio->Set (mGpio, DcpGpioPin, GPIO_MODE_OUTPUT_0);
    Status = mGpio->Set (mGpio, DcnGpioPin, GPIO_MODE_OUTPUT_0);
  }

  DEBUG (
         (DEBUG_VERBOSE, "%a: DC power %a completed\n",
          __FUNCTION__, PowerOn ? "on" : "off")
         );
  return EFI_SUCCESS;
}

STATIC
UINT32
EFIAPI
LcdMipiReadPower (
  IN LCD_MIPI_TX_DATA  *VideoTxClient
  )
{
  SPACEMIT_DSI_RX_BUF  DsiRxBuffer;
  UINT32               PowerValue = 0;

  if ((VideoTxClient == NULL) || (VideoTxClient->PanelInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return 0;
  }

  SpacemitMipiWriteCmds (
                         0,
                         VideoTxClient->PanelInfo->SetPowerCmds,
                         VideoTxClient->PanelInfo->SetPowerCmdsNum
                         );

  SpacemitMipiReadCmds (
                        0,
                        &DsiRxBuffer,
                        VideoTxClient->PanelInfo->ReadPowerCmds,
                        VideoTxClient->PanelInfo->ReadPowerCmdsNum
                        );

  PowerValue = DsiRxBuffer.Data[0];
  DEBUG ((DEBUG_VERBOSE, "%a: Read power value 0x%x\n", __FUNCTION__, PowerValue));

  return PowerValue;
}

STATIC
BOOLEAN
EFIAPI
LcdMipiEsdCheck (
  IN VIDEO_TX_DEVICE  *Dev
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;
  INTN              PowerValue;
  UINTN             i;

  if (Dev == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return FALSE;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);
  if ((VideoTxClient == NULL) || (VideoTxClient->PanelInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get client data\n", __FUNCTION__));
    return FALSE;
  }

  if (VideoTxClient->PanelInfo->SetPowerCmdsNum == 0) {
    return TRUE;
  }

  for (i = 0; i < 3; i++) {
    PowerValue = LcdMipiReadPower (VideoTxClient);

    if (PowerValue == VideoTxClient->PanelInfo->PowerValue) {
      DEBUG ((DEBUG_INFO, "%a: ESD check passed (0x%x)\n", __FUNCTION__, PowerValue));
      return TRUE;
    }

    DEBUG (
           (DEBUG_WARN, "%a: ESD check failed (0x%x), attempt %d\n",
            __FUNCTION__, PowerValue, i + 1)
           );
    gBS->Stall (50000);
  }

  return FALSE;
}

STATIC
EFI_STATUS
EFIAPI
LcdMipiPanelReset (
  IN VIDEO_TX_DEVICE  *Dev
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;
  EFI_STATUS        Status;

  if (Dev == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);
  if ((VideoTxClient == NULL) || (VideoTxClient->PanelInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get client data\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  SpacemitMipiCloseDataTx (0);

  Status = LcdMipiReset (VideoTxClient->Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GPIO reset failed - %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (VideoTxClient->PanelInfo->PanelType == LCD_MIPI) {
    Status = SpacemitMipiWriteCmds (
                                    0,
                                    VideoTxClient->PanelInfo->InitCmds,
                                    VideoTxClient->PanelInfo->InitCmdsNum
                                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Init commands failed - %r\n", __FUNCTION__, Status));
    }

    Status = SpacemitMipiWriteCmds (
                                    0,
                                    VideoTxClient->PanelInfo->SleepOutCmds,
                                    VideoTxClient->PanelInfo->SleepOutCmdsNum
                                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Sleep out commands failed - %r\n", __FUNCTION__, Status));
    }
  }

  Status = SpacemitMipiReadyForDataTx (0, VideoTxClient->PanelInfo->MipiInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Data TX preparation failed - %r\n", __FUNCTION__, Status));
    SpacemitMipiClose (0);
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
LcdMipiIdentify (
  IN VIDEO_TX_DEVICE  *Dev
  )
{
  LCD_MIPI_TX_DATA     *VideoTxClient;
  LCD_MIPI_PANEL_INFO  *PanelInfo = NULL;
  BOOLEAN              IsPanel    = FALSE;
  EFI_STATUS           Status     = EFI_SUCCESS;
  UINTN                i;

  VideoTxClient = VideoTxGetDrvData (Dev);

  Status = LcdMipiDcEnable (TRUE, VideoTxClient->Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "LCD MIPI DC enable failed! %r\n", Status));
  }

  for (i = 0; i < PanelNum; i++) {
    PanelInfo = Panels[i];
    if (PanelInfo == NULL) {
      continue;
    }

    DEBUG ((DEBUG_VERBOSE, "Identify LCD (%a)\n", PanelInfo->LcdName));

    VideoTxClient->PanelInfo = PanelInfo;

    if ((PanelInfo->PanelType == LCD_EDP) || (PanelInfo->PanelType == LCD_DPI)) {
      IsPanel = TRUE;
    } else {
      Status = LcdMipiReset (VideoTxClient->Priv);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "LCD MIPI reset failed! %r\n", Status));
        continue;
      }

      Status = SpacemitMipiOpen (0, VideoTxClient->PanelInfo->MipiInfo, FALSE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "LCD MIPI open failed! %r\n", Status));
        continue;
      }

      IsPanel = LcdMipiReadId (VideoTxClient);
      SpacemitMipiClose (0);
    }

    if (!IsPanel) {
      VideoTxClient->PanelInfo = NULL;
      continue;
    } else {
      Status = LcdMipiDcEnable (FALSE, VideoTxClient->Priv);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "LCD MIPI DC disable failed! %r\n", Status));
      }

      DEBUG ((DEBUG_INFO, "Identified Panel: %a\n", VideoTxClient->PanelInfo->LcdName));
      LcdId     = VideoTxClient->PanelInfo->LcdId;
      LcdName   = VideoTxClient->PanelInfo->LcdName;
      LcdWidth  = VideoTxClient->PanelInfo->SpacemitModeInfo->XRes;
      LcdHeight = VideoTxClient->PanelInfo->SpacemitModeInfo->YRes;
      return EFI_SUCCESS;
    }
  }

  Status = LcdMipiDcEnable (FALSE, VideoTxClient->Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "LCD MIPI DC disable failed! %r\n", Status));
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
EFIAPI
LcdMipiInit (
  IN VIDEO_TX_DEVICE  *Dev
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;
  EFI_STATUS        Status;

  if (Dev == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);
  if ((VideoTxClient == NULL) || (VideoTxClient->PanelInfo == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get client data\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Status = SpacemitMipiOpen (0, VideoTxClient->PanelInfo->MipiInfo, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MIPI open failed - %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (VideoTxClient->PanelInfo->PanelType == LCD_MIPI) {
    Status = SpacemitMipiWriteCmds (
                                    0,
                                    VideoTxClient->PanelInfo->InitCmds,
                                    VideoTxClient->PanelInfo->InitCmdsNum
                                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Init commands failed - %r\n", __FUNCTION__, Status));
    }
  }

  return Status;
}

STATIC EFI_STATUS
EFIAPI
LcdMipiSleepOut (
  IN VIDEO_TX_DEVICE  *Dev
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;
  EFI_STATUS        Status;

  if (Dev == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid device parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);
  if ((VideoTxClient == NULL) || (VideoTxClient->Priv == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get client data\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Status = LcdMipiDcEnable (TRUE, VideoTxClient->Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to enable DC power - %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = LcdMipiInit (Dev);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LCD initialization failed - %r\n", __FUNCTION__, Status));
    return Status;
  }

  Status = SpacemitMipiReadyForDataTx (0, VideoTxClient->PanelInfo->MipiInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: MIPI data TX preparation failed - %r\n", __FUNCTION__, Status));
    SpacemitMipiClose (0);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: Completed successfully\n", __FUNCTION__));
  return EFI_SUCCESS;
}

STATIC INTN
EFIAPI
LcdMipiGetModes (
  IN  VIDEO_TX_DEVICE     *Dev,
  OUT SPACEMIT_MODE_INFO  *ModeInfo
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;

  if (ModeInfo == NULL) {
    return 0;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);

  CopyMem (
           ModeInfo,
           VideoTxClient->PanelInfo->SpacemitModeInfo,
           sizeof (SPACEMIT_MODE_INFO)
           );

  return 1;
}

STATIC EFI_STATUS
EFIAPI
LcdMipiDpms (
  IN VIDEO_TX_DEVICE  *Dev,
  IN INTN             Status
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;
  CHAR8             *DpmsStatusStr;

  if (Dev == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);
  if (VideoTxClient == NULL) {
    return EFI_DEVICE_ERROR;
  }

  if (Status == VideoTxClient->DpmsStatus) {
    DEBUG ((DEBUG_INFO, "LCD already in DPMS state (%d)\n", Status));
    return EFI_SUCCESS;
  }

  switch (Status) {
    case DPMS_ON:
      DpmsStatusStr = "DPMS_ON";
      LcdMipiSleepOut (Dev);
      break;

    case DPMS_OFF:
      DpmsStatusStr = "DPMS_OFF";
      break;

    default:
      DEBUG ((DEBUG_ERROR, "Invalid DPMS status requested: %d\n", Status));
      return EFI_UNSUPPORTED;
  }

  VideoTxClient->DpmsStatus = Status;
  DEBUG ((DEBUG_INFO, "Setting DPMS mode: %a\n", DpmsStatusStr));

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
EFIAPI
LcdBlEnable (
  IN VIDEO_TX_DEVICE  *Dev,
  IN BOOLEAN          Enable
  )
{
  LCD_MIPI_TX_DATA  *VideoTxClient;
  UINT32            BlGpioPin;
  EFI_STATUS        Status;

  if (Dev == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VideoTxClient = (LCD_MIPI_TX_DATA *)VideoTxGetDrvData (Dev);
  if ((VideoTxClient == NULL) || (VideoTxClient->Priv == NULL)) {
    return EFI_DEVICE_ERROR;
  }

  BlGpioPin = BlGpio->GpioPin;

  if (Enable) {
    DEBUG ((DEBUG_INFO, "BlGpio pin: %d\n", BlGpio->GpioPin));
    Status = mGpio->Set (mGpio, BlGpioPin, GPIO_MODE_OUTPUT_1);
  } else {
    DEBUG ((DEBUG_INFO, "BlGpio pin: %d\n", BlGpio->GpioPin));
    Status = mGpio->Set (mGpio, BlGpioPin, GPIO_MODE_OUTPUT_0);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LcdMipiRegisterPanel (
  IN LCD_MIPI_PANEL_INFO  *PanelInfo
  )
{
  if (PanelNum >= PANEL_NUM_MAX) {
    DEBUG (
           (DEBUG_ERROR, "%a: Maximum panel limit (%d) reached!\n",
            __FUNCTION__, PANEL_NUM_MAX)
           );
    return EFI_OUT_OF_RESOURCES;
  }

  Panels[PanelNum] = PanelInfo;
  PanelNum++;

  DEBUG (
         (DEBUG_INFO, "Registered panel: %a (Total: %d)\n",
          PanelInfo->LcdName, PanelNum)
         );

  return EFI_SUCCESS;
}

STATIC VIDEO_TX_DRIVER  mLcdMipiDriverTx = {
  .GetModes   = LcdMipiGetModes,
  .Dpms       = LcdMipiDpms,
  .Identify   = LcdMipiIdentify,
  .EsdCheck   = LcdMipiEsdCheck,
  .PanelReset = LcdMipiPanelReset,
  .BlEnable   = LcdBlEnable
};

STATIC LCD_MIPI_TX_DATA  mTxDeviceClient = { 0 };
STATIC VIDEO_TX_DEVICE   mTxDevice       = { 0 };

STATIC
EFI_STATUS
EFIAPI
LcdMipiClientInit (
  IN SPACEMIT_PANEL_PRIV  *Priv
  )
{
  if (Priv == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  mTxDeviceClient.PanelType  = LCD_MIPI;
  mTxDeviceClient.PanelInfo  = NULL;
  mTxDeviceClient.DpmsStatus = DPMS_OFF;
  mTxDeviceClient.Priv       = Priv;

  mTxDevice.Driver    = &mLcdMipiDriverTx;
  mTxDevice.PanelType = mTxDeviceClient.PanelType;
  mTxDevice.Private   = &mTxDeviceClient;

  VideoTxRegisterDevice (&mTxDevice);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LcdMipiProbe (
  LCD_CONFIG_ARRAY  *LcdConfigs,
  DISPLAY_PANEL_NAME  *Panel
  )
{
  EFI_STATUS           Status;
  SPACEMIT_PANEL_PRIV  *Priv = NULL;

  DEBUG ((DEBUG_INFO, "%a: LCD MIPI Driver Initialization\n", __FUNCTION__));

  DcpGpio   = &LcdConfigs->DcpGpio;
  DcnGpio   = &LcdConfigs->DcnGpio;
  ResetGpio = &LcdConfigs->ResetGpio;
  BlGpio    = &LcdConfigs->BlGpio;

  DEBUG ((DEBUG_INFO, "DcpGpio pin: %d", DcpGpio->GpioPin));
  DEBUG ((DEBUG_INFO, "DcnGpio pin: %d", DcnGpio->GpioPin));
  DEBUG ((DEBUG_INFO, "ResetGpio pin: %d", ResetGpio->GpioPin));
  DEBUG ((DEBUG_INFO, "BlGpio pin: %d", BlGpio->GpioPin));

  Status = InitializeGpioProtocol ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Priv = AllocateZeroPool (sizeof (SPACEMIT_PANEL_PRIV));
  if (Priv == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate panel private data\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  AsciiStrCpyS (Priv->PanelName, sizeof (Priv->PanelName), Panel->PanelName);
  Status = LcdMipiClientInit (Priv);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Client initialization failed - %r\n", __FUNCTION__, Status));
    FreePool (Priv);
    return Status;
  }

  Status = SpacemitDsiProbe ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: DSI initialization failed - %r\n", __FUNCTION__, Status));
    FreePool (Priv);
    return Status;
  }

  if (AsciiStrCmp ("lt8911ext_edp_1080p", Priv->PanelName) == 0) {
    mTxDeviceClient.PanelType = LCD_EDP;
    mTxDevice.PanelType       = mTxDeviceClient.PanelType;
    DEBUG ((DEBUG_INFO, "%a: lt8911ext_edp_1080p \n", __FUNCTION__));
  } else if (AsciiStrCmp ("jd9365dah3", Priv->PanelName) == 0) {
    mTxDeviceClient.PanelType = LCD_MIPI;
    mTxDevice.PanelType       = mTxDeviceClient.PanelType;
    DEBUG ((DEBUG_INFO, "%a: jd9365dah3 \n", __FUNCTION__));
  } else {
    mTxDeviceClient.PanelType = LCD_MIPI;
    mTxDevice.PanelType       = mTxDeviceClient.PanelType;
    Status                    = LcdGx09inx101Init ();
    DEBUG ((DEBUG_INFO, "%a: LcdGx09inx101Init \n", __FUNCTION__));
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Panel initialization failed - %r\n", __FUNCTION__, Status));
    FreePool (Priv);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "%a: LCD MIPI Driver Initialized Successfully\n", __FUNCTION__));
  return EFI_SUCCESS;
}
