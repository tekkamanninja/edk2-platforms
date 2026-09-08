/** @file
  A hook-in library for modules depending on Flash Firmware Volume Block Service Ready Protocol.

  Plugging this library instance into one module makes it depend on the
  gSpacemitFlashFvbServiceReadyProtocolGuid, to ensure the module is loaded after
  another module (usually a Flash DXE driver) produces this protocol.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

EFI_STATUS
EFIAPI
PlatformFlashFvbReadyProtocolLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  //
  // Do nothing, just imbue another module with a protocol dependency on
  // gSpacemitFlashFvbServiceReadyProtocolGuid.
  //
  return EFI_SUCCESS;
}
