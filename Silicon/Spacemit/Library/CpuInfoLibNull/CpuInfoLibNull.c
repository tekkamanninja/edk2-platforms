/** @file
  Null implementation of CpuInfoLib.

  Returns EFI_NOT_FOUND for all queries.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/CpuInfoLib.h>

EFI_STATUS
EFIAPI
CpuInfoGetSocketData (
  IN  UINT32               SocketIndex,
  OUT CPU_INFO_SOCKET_DATA *SocketData
  )
{
  if (SocketData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_NOT_FOUND;
}
