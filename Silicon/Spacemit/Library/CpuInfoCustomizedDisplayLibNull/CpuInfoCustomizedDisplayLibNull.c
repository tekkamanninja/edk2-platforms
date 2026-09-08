/** @file
  Null implementation of CpuInfoCustomizedDisplayLib.

  Returns EFI_UNSUPPORTED so the caller can fall back to the
  SMBIOS-based default CPU information path.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/CpuInfoCustomizedDisplayLib.h>

EFI_STATUS
EFIAPI
CpuInfoCustomizedDisplay (
  IN EFI_HII_HANDLE  HiiHandle,
  IN VOID            *StartOpCodeHandle
  )
{
  return EFI_UNSUPPORTED;
}
