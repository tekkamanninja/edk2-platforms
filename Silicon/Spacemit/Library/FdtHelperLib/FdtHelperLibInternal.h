/** @file
  Provides FDT helper interfaces based on FdtLib.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FDT_HELPER_LIB_INTERNAL_H__
#define __FDT_HELPER_LIB_INTERNAL_H__

STATIC
inline
UINT64
MultiFdt32ToCpu64 (
  IN CONST  UINT32    *Prop,
  IN        INTN      Cells
  )
{
  UINT64  Temp;
  UINTN   I;

  Temp = 0;
  for (I = 0; I < Cells; I++) {
    Temp = (Temp << 32) | Fdt32ToCpu (*Prop);
    Prop++;
  }

  return Temp;
}

#endif /* ifndef __FDT_HELPER_LIB_INTERNAL_H__ */
