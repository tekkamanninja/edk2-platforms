/**
  Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

//
//// The package level header files this module uses
////
#include <PiPei.h>

#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>

/**
  Cpu Peim initialization.
**/
EFI_STATUS
EFIAPI
CpuPeimInitializationCommon (
  IN  VOID  *DeviceTreeAddress
  )
{
  //
  // for MMU type >= sv39
  //
  BuildCpuHob (FixedPcdGet8 (PcdMemoryAddressWidthMax), FixedPcdGet8 (PcdIoAddressWidthMax));

  return EFI_SUCCESS;
}
