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
#include <Include/Library/SpacemitVideoTx.h>
#include "SpacemitDsiCommon.h"

#define PANEL_NUM_MAX  5

STATIC INTN             mTxDeviceNum   = 0;
STATIC VIDEO_TX_DEVICE  *mTxDevices[2] = { NULL };

VIDEO_TX_DEVICE *
EFIAPI
FindVideoTx (
  VOID
  )
{
  VIDEO_TX_DEVICE  *TxDevice;
  EFI_STATUS       IsPanel;
  INTN             i;

  for (i = 0; i < mTxDeviceNum; i++) {
    TxDevice = mTxDevices[i];
    if (TxDevice == NULL) {
      continue;
    }

    if (TxDevice->Driver->Identify == NULL) {
      continue;
    }

    IsPanel = TxDevice->Driver->Identify (TxDevice);
    if (!EFI_ERROR (IsPanel)) {
      DEBUG ((DEBUG_INFO, "LCD (port %d) is opened by kernel!\n", TxDevice->PanelType));
      return TxDevice;
    } else {
      DEBUG ((DEBUG_INFO, "LCD port (%d) is not the correct video transmitter!\n", TxDevice->PanelType));
    }
  }

  DEBUG ((DEBUG_INFO, "Cannot find the correct panel!\n"));
  return NULL;
}

INTN
EFIAPI
VideoTxGetModes (
  IN  VIDEO_TX_DEVICE     *VideoTx,
  OUT SPACEMIT_MODE_INFO  *Modelist
  )
{
  if (VideoTx->Driver->GetModes == NULL) {
    return -1;
  }

  return VideoTx->Driver->GetModes (VideoTx, Modelist);
}

INTN
EFIAPI
VideoTxDpms (
  IN  VIDEO_TX_DEVICE  *VideoTx,
  IN  INTN             Mode
  )
{
  INTN  Ret;

  if (VideoTx->Driver->Dpms == NULL) {
    return -1;
  }

  Ret = VideoTx->Driver->Dpms (VideoTx, Mode);
  return Ret;
}

VOID
EFIAPI
VideoTxEsdCheck (
  IN  VIDEO_TX_DEVICE  *VideoTx
  )
{
  BOOLEAN  EsdStatus;
  INTN     Ret;

  if ((VideoTx->Driver->EsdCheck == NULL) || (VideoTx->Driver->PanelReset == NULL)) {
    DEBUG ((DEBUG_INFO, "ESD check not implemented\n"));
    return;
  }

  EsdStatus = VideoTx->Driver->EsdCheck (VideoTx);
  if (!EsdStatus) {
    Ret = VideoTx->Driver->PanelReset (VideoTx);
    if (Ret != 0) {
      DEBUG ((DEBUG_INFO, "Panel reset failed!\n"));
    }
  }
}

VOID
EFIAPI
VideoTxReset (
  IN  VIDEO_TX_DEVICE  *VideoTx
  )
{
  INTN  Ret;

  if (VideoTx->Driver->PanelReset == NULL) {
    DEBUG ((DEBUG_INFO, "Reset not implemented\n"));
    return;
  }

  Ret = VideoTx->Driver->PanelReset (VideoTx);
  if (Ret != 0) {
    DEBUG ((DEBUG_INFO, "Panel reset failed!\n"));
  }
}

INTN
EFIAPI
VideoTxRegisterDevice (
  IN  VIDEO_TX_DEVICE  *TxDevice
  )
{
  if (mTxDeviceNum >= ARRAY_SIZE (mTxDevices)) {
    DEBUG ((DEBUG_INFO, "%a: Video transmitter device list is full!\n", __FUNCTION__));
    return -1;
  }

  mTxDevices[mTxDeviceNum] = TxDevice;
  mTxDeviceNum++;

  DEBUG ((DEBUG_INFO, "FB: Video transmitter (panel type %d) registered\n", TxDevice->PanelType));
  return 0;
}

VOID *
EFIAPI
VideoTxGetDrvData (
  IN  VIDEO_TX_DEVICE  *TxDevice
  )
{
  return TxDevice->Private;
}
