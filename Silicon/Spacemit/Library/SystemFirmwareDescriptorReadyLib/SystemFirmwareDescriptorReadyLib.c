/** @file
  A hook-in library for modules depending on SystemFirmwareDescriptor Service Ready Protocol.

  Plugging this library instance into one module makes it depend on the
  gSpacemitSystemFirmwareDescriptorReadyProtocolGuid, to ensure the module is loaded after
  another module (usually a system firmware descriptor builder) produces this protocol.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

EFI_STATUS
EFIAPI
SystemFirmwareDescriptorReadyLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  //
  // Do nothing, just imbue another module with a protocol dependency on
  // gSpacemitSystemFirmwareDescriptorReadyProtocolGuid.
  //
  return EFI_SUCCESS;
}
