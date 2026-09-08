/**
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/SpacemitSecHelperLib.h>

EFI_STATUS
EFIAPI
MemoryPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  )
{
  return MemoryPeimInitializationCommon (DeviceTreeAddress);
}

EFI_STATUS
EFIAPI
CpuPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  )
{
  return CpuPeimInitializationCommon (DeviceTreeAddress);
}

EFI_STATUS
EFIAPI
PlatformPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  )
{
  EFI_STATUS Status;

  Status = PlatformPeimInitializationCommon (DeviceTreeAddress);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  return EFI_SUCCESS;
}
