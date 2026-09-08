/** @file
  Null board pinctrl map library.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BoardPinctrlMapLib.h>

EFI_STATUS
EFIAPI
BoardPinctrlGetMap (
  OUT CONST PINCTRL_BOARD_MAP  **BoardMap
  )
{
  if (BoardMap == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *BoardMap = NULL;
  return EFI_NOT_FOUND;
}
