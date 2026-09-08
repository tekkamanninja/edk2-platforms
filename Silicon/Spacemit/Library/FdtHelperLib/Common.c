/** @file
  Provides FDT helper interfaces based on FdtLib.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/FdtLib.h>

#include <Library/FdtHelperLib.h>
#include "FdtHelperLibInternal.h"

/**
  Check whether a FDT node is enabled.

  @param  Fdt             The poiner to FDT blob.
  @param  NodeOffset      The offset to the given node.

  @retval TRUE if the FDT node is enabled, otherwise FALSE.

**/
BOOLEAN
EFIAPI
FdtNodeIsEnabled (
  IN CONST  VOID    *Fdt,
  IN        INTN    NodeOffset
  )
{
  CONST CHAR8     *Val;
  INT32           Len;

  Val = (CONST CHAR8 *) FdtGetProp (Fdt, NodeOffset, "status", &Len);

  //
  // A missing "status" property implies "okay".
  //
  if (Val == NULL) {
    return TRUE;
  }

  //
  // If the "status" property exists, check whether it is set to "okay" or "ok",
  // which means "enabled", otherwise anything else is treated as "disabled".
  //
  if ((Len >= 5) && (0 == AsciiStrCmp (Val, "okay"))) {
    return TRUE;
  }
  if ((Len >= 3) && (0 == AsciiStrCmp (Val, "ok"))) {
    return TRUE;
  }

  return FALSE;
}

STATIC
EFI_STATUS
FdtTranslateAddress (
  IN CONST  VOID    *Fdt,
  IN        UINT64  Reg,
  IN        INTN    ParentOffset,
  OUT       UINT64  *Addr
  )
{
  INTN              AddrCells;
  INTN              SizeCells;
  INTN              I;
  CONST UINT32      *Ranges;
  INT32             Len;
  UINT64            ChildAddr;
  UINT64            ParentAddr;
  UINT64            Size;

  AddrCells = FdtAddressCells (Fdt, ParentOffset);
  if (AddrCells < 1) {
    return EFI_NOT_FOUND;
  }
  SizeCells = FdtSizeCells (Fdt, ParentOffset);
  if (SizeCells < 0) {
    return EFI_NOT_FOUND;
  }

  ChildAddr = 0;
  ParentAddr = 0;
  Size = 0;
  Ranges = FdtGetProp (Fdt, ParentOffset, "ranges", &Len);
  if ((Ranges != NULL) && (Len > 0)) {
    for (I = 0; I < AddrCells; I++) {
      ChildAddr = (ChildAddr << 32) | Fdt32ToCpu (*Ranges);
      Ranges++;
    }
    for (I = 0; I < AddrCells; I++) {
      ParentAddr = (ParentAddr << 32) | Fdt32ToCpu (*Ranges);
      Ranges++;
    }
    for (I = 0; I < SizeCells; I++) {
      Size = (Size << 32) | Fdt32ToCpu (*Ranges);
      Ranges++;
    }
    if (Reg < ChildAddr || ChildAddr >= (Reg + Size)) {
      return EFI_UNSUPPORTED;
    }
    *Addr = ParentAddr + (Reg - ChildAddr);
  } else {
    //
    // No translation required
    //
    *Addr = Reg;
  }

  return EFI_SUCCESS;
}

/**
  Get the address and size described by "reg" property of a FDT node.

  @param  Fdt             The poiner to FDT blob.
  @param  NodeOffset      The offset to the given node.
  @param  Index           Index in the "reg" property array.
  @param  Addr            The address got from "reg" property.
                          It can be NULL if we don't care about it.
  @param  Size            The size got from "reg" property.
                          It can be NULL if we don't care about it.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGetNodeAddrSize (
  IN CONST  VOID    *Fdt,
  IN        INTN    NodeOffset,
  IN        INTN    Index,
  OUT       UINT64  *Addr,        OPTIONAL
  OUT       UINT64  *Size         OPTIONAL
  )
{
  INTN              ParentOffset;
  INTN              AddrCells;
  INTN              SizeCells;
  CONST UINT32      *AddrProp;
  CONST UINT32      *SizeProp;
  INT32             Len;
  UINT64            Temp;
  EFI_STATUS        Status;

  if (Fdt == NULL || NodeOffset < 0 || Index < 0) {
    return EFI_INVALID_PARAMETER;
  }

  ParentOffset = FdtParentOffset (Fdt, NodeOffset);
  if (ParentOffset < 0) {
    return EFI_UNSUPPORTED;
  }

  AddrCells = FdtAddressCells (Fdt, ParentOffset);
  if (AddrCells < 1) {
    return EFI_NOT_FOUND;
  }
  SizeCells = FdtSizeCells (Fdt, ParentOffset);
  if (SizeCells < 0) {
    return EFI_NOT_FOUND;
  }

  AddrProp = FdtGetProp (Fdt, NodeOffset, "reg", &Len);
  if (AddrProp == NULL) {
    return EFI_NOT_FOUND;
  }

  if ((Len / sizeof (UINT32)) <= (Index * (AddrCells + SizeCells))) {
    return EFI_INVALID_PARAMETER;
  }

  AddrProp = AddrProp + (Index * (AddrCells + SizeCells));
  SizeProp = AddrProp + AddrCells;

  if (Addr != NULL) {
    Temp = MultiFdt32ToCpu64 (AddrProp, AddrCells);

    do {
      if (ParentOffset < 0) {
        break;
      }

      Status = FdtTranslateAddress (Fdt, Temp, ParentOffset, Addr);
      if (Status != EFI_SUCCESS) {
        break;
      }

      ParentOffset = FdtParentOffset (Fdt, ParentOffset);
      Temp = *Addr;
    } while (1);
  }

  if (Size != NULL) {
    *Size = MultiFdt32ToCpu64 (SizeProp, SizeCells);
  }

  return EFI_SUCCESS;
}

/**
  Get the address and size described by "reg" property of a FDT node, via a name
  in "reg-names".

  @param  Fdt             The poiner to FDT blob.
  @param  NodeOffset      The offset to the given node.
  @param  Name            A name in "reg-names" to specify the index of "reg".
  @param  Addr            The address got from "reg" property.
                          It can be NULL if we don't care about it.
  @param  Size            The size got from "reg" property.
                          It can be NULL if we don't care about it.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGetNodeAddrSizeByName (
  IN CONST  VOID    *Fdt,
  IN        INTN    NodeOffset,
  IN CONST  CHAR8   *Name,
  OUT       UINT64  *Addr,        OPTIONAL
  OUT       UINT64  *Size         OPTIONAL
  )
{
  CONST CHAR8     *Val;
  CONST CHAR8     *RegName;
  INT32           Len;
  INTN            I;
  INTN            J;

  if (Fdt == NULL || NodeOffset < 0 || Name == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Val = FdtGetProp (Fdt, NodeOffset, "reg-names", &Len);
  if (Val == NULL) {
    return EFI_NOT_FOUND;
  }

  for (I = 0, J = 0; I < Len; I++, J++) {
    RegName = Val + I;

    if (0 == AsciiStrCmp (Name, RegName)) {
      return FdtGetNodeAddrSize (Fdt, NodeOffset, J, Addr, Size);
    }

    I += AsciiStrLen (RegName);
  }

  return EFI_NOT_FOUND;
}

/**
  Get "#interrupt-cells" property of a FDT node.

  @param  Fdt         The poiner to FDT blob.
  @param  NodeOffset  The offset to the given node.

  @retval Number of cells on success, or negative number on failure.

**/
INTN
EFIAPI
FdtInterruptCells (
  IN CONST  VOID    *Fdt,
  IN        INTN    NodeOffset
  )
{
  CONST UINT32    *Val;
  INT32           Len;

  Val = (CONST UINT32 *) FdtGetProp (Fdt, NodeOffset, "#interrupt-cells", &Len);
  if (Val == NULL || Len <= 0) {
    return -FDT_ERR_NOTFOUND;
  }
  return Fdt32ToCpu (*Val);
}

/**
  Determine whether a device in FDT is is capable of coherent DMA operations.

  @param  Fdt         The poiner to FDT blob.
  @param  NodeOffset  The offset to the given node.

  @retval TRUE if DMA is coherent, otherwise FALSE.

**/
BOOLEAN
EFIAPI
FdtDmaIsCoherent (
  IN CONST  VOID    *Fdt,
  IN        INTN    NodeOffset
  )
{
  INTN    Offset;

  Offset = NodeOffset;
  while (Offset >= 0) {
    if (NULL != FdtGetProp (Fdt, Offset, "dma-coherent", NULL)) {
      return TRUE;
    }
    if (NULL != FdtGetProp (Fdt, Offset, "dma-noncoherent", NULL)) {
      return FALSE;
    }

    Offset = FdtParentOffset (Fdt, Offset);
  }

  //
  // If neither "dma-coherent" nor "dma-noncoherent" exists in the whole
  // hierarchy, we assume that DMA is non-coherent.
  //
  return FALSE;
}
