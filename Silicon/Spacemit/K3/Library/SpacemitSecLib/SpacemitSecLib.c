/**
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/SpacemitSecHelperLib.h>

#define K3_APMU_BASE  (FixedPcdGet64 (PcdSpacemitAPMURegBase))

EFI_STATUS
EFIAPI
MemoryPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  )
{
  // OpenSBI protects the APMU REGISTER_PRESERVATION page via PMP (M-only).
  // Register it as MMIO so Sv39 page tables include it; S-mode accesses will
  // then raise cause=5 (Load Access Fault) instead of cause=13 (page not
  // mapped), allowing OpenSBI's spacemit_k3_emulate_load to handle them.
  BuildResourceDescriptorHob (
    EFI_RESOURCE_MEMORY_MAPPED_IO,
    EFI_RESOURCE_ATTRIBUTE_PRESENT,
    K3_APMU_BASE & (~0xFFFULL),
    SIZE_4KB
    );

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
