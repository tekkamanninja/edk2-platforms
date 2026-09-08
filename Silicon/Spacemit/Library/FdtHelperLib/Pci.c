/** @file
  Provides FDT helper interfaces based on FdtLib.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Library/FdtHelperLib.h>
#include "FdtHelperLibInternal.h"

/**
  Get information from "ranges" or "dma-ranges" property in a PCI bus bridge FDT node.

  @param  Fdt             The poiner to FDT blob.
  @param  NodeOffset      The offset to the given node.
  @param  PropName        The property name, can be "ranges" or "dma-ranges".
  @param  Infos           An array storing the information got. It should be
                          released via FdtReleasePciBusBridgeRangesInfo() when
                          it is no longer needed.
  @param  NumInfos        The number of elements in Infos array.

  @retval EFI_SUCCESS     Succeed.
  @retval EFI_NOT_FOUND   "ranges" or "dma-ranges" not found in this FDT node.
  @retval Other           Other error status.

**/
EFI_STATUS
EFIAPI
FdtGetPciBusBridgeRangesInfo (
  IN CONST  VOID                                *Fdt,
  IN        INTN                                NodeOffset,
  IN CONST  CHAR8                               *PropName,
  OUT       FDT_PCI_BUS_BRIDGE_RANGES_INFO      **Infos,
  OUT       UINTN                               *NumInfos
  )
{
  EFI_STATUS                        Status;
  INTN                              ChildAddrCells;
  INTN                              ChildSizeCells;
  INTN                              ParentOffset;
  INTN                              ParentAddrCells;
  CONST UINT32                      *RangesProp;
  INT32                             Len;
  UINTN                             NumRanges;
  UINTN                             Index;
  FDT_PCI_BUS_BRIDGE_RANGES_INFO    *RangesInfos;

  if (Fdt == NULL || Infos == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ErrOut;
  }
  if ((0 != AsciiStrCmp (PropName, "ranges")) &&
      (0 != AsciiStrCmp (PropName, "dma-ranges"))) {
    Status = EFI_INVALID_PARAMETER;
    goto ErrOut;
  }

  ChildAddrCells = FdtAddressCells (Fdt, NodeOffset);
  if (ChildAddrCells != 3) {
    Status = EFI_UNSUPPORTED;
    goto ErrOut;
  }
  ChildSizeCells = FdtSizeCells (Fdt, NodeOffset);
  if (ChildSizeCells != 2) {
    Status = EFI_UNSUPPORTED;
    goto ErrOut;
  }

  ParentOffset = FdtParentOffset (Fdt, NodeOffset);
  if (ParentOffset < 0) {
    Status = EFI_UNSUPPORTED;
    goto ErrOut;
  }

  ParentAddrCells = FdtAddressCells (Fdt, ParentOffset);
  if (ParentAddrCells < 1) {
    Status = EFI_UNSUPPORTED;
    goto ErrOut;
  }

  RangesProp = FdtGetProp (Fdt, NodeOffset, PropName, &Len);
  if (RangesProp == NULL || Len <= 0) {
    Status = EFI_NOT_FOUND;
    goto ErrOut;
  }

  NumRanges = Len / (sizeof (UINT32) * (ChildAddrCells + ParentAddrCells + ChildSizeCells));
  if (NumRanges <= 0) {
    Status = EFI_UNSUPPORTED;
    goto ErrOut;
  }

  RangesInfos = AllocateZeroPool (sizeof (FDT_PCI_BUS_BRIDGE_RANGES_INFO) * NumRanges);
  if (RangesInfos == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }

  for (Index = 0; Index < NumRanges; Index++) {
    RangesInfos[Index].Flags = Fdt32ToCpu (*RangesProp);
    RangesProp++;

    RangesInfos[Index].ChildAddr = MultiFdt32ToCpu64 (RangesProp, ChildAddrCells - 1);
    RangesProp += ChildAddrCells - 1;

    RangesInfos[Index].ParentAddr = MultiFdt32ToCpu64 (RangesProp, ParentAddrCells);
    RangesProp += ParentAddrCells;

    RangesInfos[Index].Size = MultiFdt32ToCpu64 (RangesProp, ChildSizeCells);
    RangesProp += ChildSizeCells;
  }

  *Infos = RangesInfos;
  *NumInfos = NumRanges;

  return EFI_SUCCESS;

ErrOut:
  return Status;
}

/**
  Release the information array got via FdtGetPciBusBridgeRangesInfo().

  @param  Infos           The information array got via FdtGetPciBusBridgeRangesInfo().

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtReleasePciBusBridgeRangesInfo (
  IN  FDT_PCI_BUS_BRIDGE_RANGES_INFO    *Infos
  )
{
  if (Infos == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FreePool (Infos);

  return EFI_SUCCESS;
}

STATIC
FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER *
FindPciInterruptMapIrqControllerFromList (
  IN        INTN          IrqControllerNodeOffset,
  IN CONST  LIST_ENTRY    *IrqControllerList
  )
{
  LIST_ENTRY                              *Entry;
  FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER    *IrqController;

  BASE_LIST_FOR_EACH (Entry, IrqControllerList) {
    IrqController = LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER (Entry);

    if (IrqController->NodeOffset == IrqControllerNodeOffset) {
      return IrqController;
    }
  }
  return NULL;
}

STATIC
INTN
FdtIrqControllerAddressCells (
  IN CONST  VOID    *Fdt,
  IN        INTN    NodeOffset
  )
{
  CONST UINT32    *Val;
  INT32           Len;

  Val = (CONST UINT32 *) FdtGetProp (Fdt, NodeOffset, "#address-cells", &Len);
  if (Val == NULL || Len <= 0) {
    //
    // In many interrupt controllers, #address-cells are missing, and it is
    // treated as a value of 0 rather than the default value of 2 specified in
    // FDT spec. To be compatible, here we treat a missing #address-cells as a
    // value of 0 too.
    //
    return 0;
  }
  return Fdt32ToCpu (*Val);
}

/**
  Get information from "interrupt-map" property in a PCI bus FDT node.

  @param  Fdt             The poiner to FDT blob.
  @param  NodeOffset      The offset to the given node.
  @param  Info            The pointer to the information got.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGetPciInterruptMapInfo (
  IN CONST  VOID                          *Fdt,
  IN        INTN                          NodeOffset,
  OUT       FDT_PCI_INTERRUPT_MAP_INFO    **Info
  )
{
  EFI_STATUS                              Status;
  FDT_PCI_INTERRUPT_MAP_INFO              *MapInfo;
  CONST UINT32                            *Prop;
  INT32                                   Len;
  CONST UINT32                            *MapProp;
  INT32                                   MapLen;
  INTN                                    ChildAddrCells;
  INTN                                    ChildIrqCells;
  INTN                                    ParentAddrCells;
  INTN                                    ParentIrqCells;
  FDT_PCI_INTERRUPT_MAP_ENTRY             *MapEntry;
  INTN                                    IrqControllerNodeOffset;
  FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER    *IrqController;
  LIST_ENTRY                              *Entry;
  LIST_ENTRY                              *NextEntry;

  if (Fdt == NULL || Info == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ErrOut;
  }

  MapInfo = AllocateZeroPool (sizeof (FDT_PCI_INTERRUPT_MAP_INFO));
  if (MapInfo == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }
  InitializeListHead (&MapInfo->IrqControllerList);
  InitializeListHead (&MapInfo->MapEntryList);

  ChildAddrCells = FdtAddressCells (Fdt, NodeOffset);
  if (ChildAddrCells != 3) {
    Status = EFI_UNSUPPORTED;
    goto FreeMapInfo;
  }

  ChildIrqCells = FdtInterruptCells (Fdt, NodeOffset);
  if (ChildIrqCells != 1) {
    Status = EFI_UNSUPPORTED;
    goto FreeMapInfo;
  }

  Prop = (CONST UINT32 *) FdtGetProp (Fdt, NodeOffset, "interrupt-map-mask", &Len);
  if (Prop != NULL && Len == (sizeof (UINT32) * (ChildAddrCells + ChildIrqCells))) {
    MapInfo->Mask.DevMask = FDT_PCI_ADDR_FLAGS_GET (DEV_NUM, Fdt32ToCpu (*Prop));
    MapInfo->Mask.IrqMask = Fdt32ToCpu (*(Prop + ChildAddrCells));
  } else {
    MapInfo->Mask.DevMask = FDT_PCI_ADDR_FLAGS_DEV_NUM_MASK;
    MapInfo->Mask.IrqMask = 0xffffffff;
  }

  MapProp = (CONST UINT32 *) FdtGetProp (Fdt, NodeOffset, "interrupt-map", &MapLen);
  if (MapProp == NULL || MapLen <= 0) {
    Status = EFI_UNSUPPORTED;
    goto FreeMapInfo;
  }

  Prop = MapProp;
  while (Prop < MapProp + MapLen / sizeof (UINT32)) {
    MapEntry = AllocateZeroPool (sizeof (FDT_PCI_INTERRUPT_MAP_ENTRY));
    if (MapEntry == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto FreeMapEntries;
    }
    InsertTailList (&MapInfo->MapEntryList, &MapEntry->Link);
    MapInfo->NumMapEntries++;

    MapEntry->DevNum = FDT_PCI_ADDR_FLAGS_GET (DEV_NUM, Fdt32ToCpu (*Prop));
    Prop += ChildAddrCells;

    MapEntry->PciIrqPin = Fdt32ToCpu (*Prop);
    Prop += ChildIrqCells;

    IrqControllerNodeOffset = FdtNodeOffsetByPhandle (Fdt, Fdt32ToCpu (*Prop));
    if (IrqControllerNodeOffset < 0) {
      Status = EFI_NOT_FOUND;
      goto FreeMapEntries;
    }
    IrqController = FindPciInterruptMapIrqControllerFromList (IrqControllerNodeOffset,
                                                              &MapInfo->IrqControllerList);
    if (IrqController == NULL) {
      IrqController = AllocateZeroPool (sizeof (FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER));
      if (IrqController == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto FreeMapEntries;
      }

      //
      // "#address-cells = <0>" is a legal usage and may occur in many interrupt
      // controllers, but FdtAddressCells() in FdtLib treats it as an error. So
      // here use a customize interface FdtIrqControllerAddressCells() to get
      // the #address-cells of the interrupt controller.
      //
      ParentAddrCells = FdtIrqControllerAddressCells (Fdt, IrqControllerNodeOffset);
      if (ParentAddrCells < 0) {
        Status = EFI_UNSUPPORTED;
        goto FreeMapEntries;
      }

      ParentIrqCells = FdtInterruptCells (Fdt, IrqControllerNodeOffset);
      if (ParentIrqCells != 2) {
        //
        // For now we only support parsing interrupt controller with #interrupt-cells == 2,
        // one cell for interrupt number and the other one cell for level/sense information.
        //
        Status = EFI_UNSUPPORTED;
        goto FreeMapEntries;
      }

      IrqController->NodeOffset = IrqControllerNodeOffset;
      IrqController->AddrCells = ParentAddrCells;
      IrqController->IrqCells = ParentIrqCells;
      InsertTailList (&MapInfo->IrqControllerList, &IrqController->Link);
      MapInfo->NumIrqControllers++;
    }
    MapEntry->IrqController = IrqController;
    Prop += 1;

    Prop += IrqController->AddrCells;

    MapEntry->IrqNum = Fdt32ToCpu (*Prop);
    Prop += 1;

    MapEntry->IrqType = Fdt32ToCpu (*Prop);
    Prop += 1;
  }

  *Info = MapInfo;

  return EFI_SUCCESS;

FreeMapEntries:
  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, &MapInfo->MapEntryList) {
    MapEntry = LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_ENTRY (Entry);
    RemoveEntryList (Entry);
    FreePool (MapEntry);
  }
  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, &MapInfo->IrqControllerList) {
    IrqController = LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER (Entry);
    RemoveEntryList (Entry);
    FreePool (IrqController);
  }
FreeMapInfo:
  FreePool (MapInfo);
ErrOut:
  return Status;
}

/**
  Release the information got via FdtGetPciInterruptMapInfo().

  @param  Info          The information got via FdtGetPciInterruptMapInfo().

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
FdtReleasePciInterruptMapInfo (
  IN  FDT_PCI_INTERRUPT_MAP_INFO    *Info
  )
{
  LIST_ENTRY                              *Entry;
  LIST_ENTRY                              *NextEntry;
  FDT_PCI_INTERRUPT_MAP_ENTRY             *MapEntry;
  FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER    *IrqController;

  if (Info == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, &Info->MapEntryList) {
    MapEntry = LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_ENTRY (Entry);
    RemoveEntryList (Entry);
    FreePool (MapEntry);
  }
  Info->NumMapEntries = 0;

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, &Info->IrqControllerList) {
    IrqController = LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER (Entry);
    RemoveEntryList (Entry);
    FreePool (IrqController);
  }
  Info->NumIrqControllers = 0;

  FreePool (Info);

  return EFI_SUCCESS;
}
