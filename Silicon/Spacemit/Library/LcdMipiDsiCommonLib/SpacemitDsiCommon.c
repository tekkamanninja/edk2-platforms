/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include <Uefi.h>  
#include <Library/DebugLib.h>
#include "SpacemitDsiDrv.h"
#include "SpacemitDsiCommon.h"

SPACEMIT_DSI_DEVICE  *gSpacemitDsiList[MAX_DSI_NUM];

/* API for panel */
EFI_STATUS
EFIAPI
SpacemitMipiOpen (
  IN UINT32              Id,
  IN SPACEMIT_MIPI_INFO  *MipiInfo,
  IN BOOLEAN             Ready
  )
{
  if (Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param (%d)\n", __FUNCTION__, Id));
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == gSpacemitDsiList[Id]) {
    DEBUG ((DEBUG_ERROR, "%a: DSI (%d) has not been registered\n", __FUNCTION__, Id));
    return EFI_NOT_READY;
  }

  return gSpacemitDsiList[Id]->DriverCtx->DsiOpen (gSpacemitDsiList[Id], MipiInfo, Ready);
}

EFI_STATUS
EFIAPI
SpacemitMipiClose (
  IN UINT32  Id
  )
{
  if (Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param (%d)\n", __FUNCTION__, Id));
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == gSpacemitDsiList[Id]) {
    DEBUG ((DEBUG_ERROR, "%a: DSI (%d) has not been registered\n", __FUNCTION__, Id));
    return EFI_NOT_READY;
  }

  return gSpacemitDsiList[Id]->DriverCtx->DsiClose (gSpacemitDsiList[Id]);
}

EFI_STATUS
EFIAPI
SpacemitMipiWriteCmds (
  IN UINT32                 Id,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  )
{
  if (Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param (%d)\n", __FUNCTION__, Id));
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == gSpacemitDsiList[Id]) {
    DEBUG ((DEBUG_ERROR, "%a: DSI (%d) has not been registered\n", __FUNCTION__, Id));
    return EFI_NOT_READY;
  }

  return gSpacemitDsiList[Id]->DriverCtx->DsiWriteCmds (gSpacemitDsiList[Id], Cmds, Count);
}

EFI_STATUS
EFIAPI
SpacemitMipiReadCmds (
  IN UINT32                 Id,
  OUT SPACEMIT_DSI_RX_BUF   *Dbuf,
  IN SPACEMIT_DSI_CMD_DESC  *Cmds,
  IN UINT32                 Count
  )
{
  if (Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param (%d)\n", __FUNCTION__, Id));
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == gSpacemitDsiList[Id]) {
    DEBUG ((DEBUG_ERROR, "%a: DSI (%d) has not been registered\n", __FUNCTION__, Id));
    return EFI_NOT_READY;
  }

  return gSpacemitDsiList[Id]->DriverCtx->DsiReadCmds (gSpacemitDsiList[Id], Dbuf, Cmds, Count);
}

EFI_STATUS
EFIAPI
SpacemitMipiReadyForDataTx (
  IN UINT32              Id,
  IN SPACEMIT_MIPI_INFO  *MipiInfo
  )
{
  if (Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param (%d)\n", __FUNCTION__, Id));
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == gSpacemitDsiList[Id]) {
    DEBUG ((DEBUG_ERROR, "%a: DSI (%d) has not been registered\n", __FUNCTION__, Id));
    return EFI_NOT_READY;
  }

  return gSpacemitDsiList[Id]->DriverCtx->DsiReadyForDataTx (gSpacemitDsiList[Id], MipiInfo);
}

EFI_STATUS
EFIAPI
SpacemitMipiCloseDataTx (
  IN UINT32  Id
  )
{
  if (Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param (%d)\n", __FUNCTION__, Id));
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == gSpacemitDsiList[Id]) {
    DEBUG ((DEBUG_ERROR, "%a: DSI (%d) has not been registered\n", __FUNCTION__, Id));
    return EFI_NOT_READY;
  }

  return gSpacemitDsiList[Id]->DriverCtx->DsiCloseDataTx (gSpacemitDsiList[Id]);
}

/* API for DSI driver */
EFI_STATUS
EFIAPI
SpacemitDsiRegisterDevice (
  IN VOID  *Device
  )
{
  SPACEMIT_DSI_DEVICE  *DsiDevice = (SPACEMIT_DSI_DEVICE *)Device;

  if (NULL == DsiDevice) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid param\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (DsiDevice->Id >= MAX_DSI_NUM) {
    DEBUG ((DEBUG_ERROR, "%a: Error Id(%d)!\n", __FUNCTION__, DsiDevice->Id));
    return EFI_INVALID_PARAMETER;
  }

  if (DsiDevice->DriverCtx == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Error DriverCtx!\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (gSpacemitDsiList[DsiDevice->Id] != NULL) {
    DEBUG ((DEBUG_ERROR, "%a: %d Id has been registered!\n", __FUNCTION__, DsiDevice->Id));
    return EFI_ALREADY_STARTED;
  }

  gSpacemitDsiList[DsiDevice->Id] = DsiDevice;

  return EFI_SUCCESS;
}
