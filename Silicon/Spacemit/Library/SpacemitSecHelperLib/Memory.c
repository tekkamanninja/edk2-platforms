/**
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>
  Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

//
// The package level header files this module uses
//
#include <PiPei.h>

//
// The Library classes this module consumes
//
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrePiLib.h>
#include <Library/ResourcePublicationLib.h>
#include <Register/RiscV64/RiscVEncoding.h>
#include <Guid/FdtHob.h>

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

VOID
BuildMemoryTypeInformationHob (
  VOID
  );

/**
  Create memory range resource HOB using the memory base
  address and size.

  @param  MemoryBase     Memory range base address.
  @param  MemorySize     Memory range size.

**/
STATIC
VOID
AddMemoryBaseSizeHob (
  IN EFI_PHYSICAL_ADDRESS  MemoryBase,
  IN UINT64                MemorySize
  )
{
  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_TESTED,
    MemoryBase,
    MemorySize
    );
}

/**
  Create memory range resource HOB using memory base
  address and top address of the memory range.

  @param  MemoryBase     Memory range base address.
  @param  MemoryLimit    Memory range size.

**/
STATIC
VOID
AddMemoryRangeHob (
  IN EFI_PHYSICAL_ADDRESS  MemoryBase,
  IN EFI_PHYSICAL_ADDRESS  MemoryLimit
  )
{
  AddMemoryBaseSizeHob (MemoryBase, (UINT64)(MemoryLimit - MemoryBase));
}

/**
  Publish system RAM and reserve memory regions.

**/
STATIC
VOID
InitializeRamRegions (
  IN EFI_PHYSICAL_ADDRESS  SystemMemoryBase,
  IN UINT64                SystemMemorySize
  )
{
  AddMemoryRangeHob (
    SystemMemoryBase,
    SystemMemoryBase + SystemMemorySize
    );
}

/** Get the number of cells for a given property

  @param[in]  Fdt   Pointer to Device Tree (DTB)
  @param[in]  Node  Node
  @param[in]  Name  Name of the property

  @return           Number of cells.
**/
STATIC
INT32
GetNumCells (
  IN VOID         *Fdt,
  IN INT32        Node,
  IN CONST CHAR8  *Name
  )
{
  CONST INT32  *Prop;
  INT32        Len;
  UINT32       Val;

  Prop = FdtGetProp (Fdt, Node, Name, &Len);
  if (Prop == NULL) {
    return Len;
  }

  if (Len != sizeof (*Prop)) {
    return -FDT_ERR_BADNCELLS;
  }

  Val = Fdt32ToCpu (*Prop);
  if (Val > FDT_MAX_NCELLS) {
    return -FDT_ERR_BADNCELLS;
  }

  return (INT32)Val;
}

/** Mark reserved memory ranges in the EFI memory map

 * As per DT spec v0.4 Section 3.5.4,
 * "Reserved regions with the no-map property must be listed in the
 * memory map with type EfiReservedMemoryType. All other reserved
 * regions must be listed with type EfiBootServicesData."

  @param FdtPointer Pointer to FDT

**/
STATIC
VOID
AddReservedMemoryMap (
  IN VOID  *FdtPointer
  )
{
  CONST INT32           *RegProp;
  INT32                 Node;
  INT32                 SubNode;
  INT32                 Len;
  EFI_PHYSICAL_ADDRESS  Addr;
  UINT64                Size;
  INTN                  NumRsv, i;
  INT32                 NumAddrCells, NumSizeCells;

  NumRsv = FdtGetNumberOfReserveMapEntries (FdtPointer);

  /* Look for an existing entry and add it to the efi mem map. */
  for (i = 0; i < NumRsv; i++) {
    if (FdtGetReserveMapEntry (FdtPointer, i, &Addr, &Size) != 0) {
      continue;
    }

    BuildMemoryAllocationHob (
      Addr,
      Size,
      EfiReservedMemoryType
      );
  }

  /* process reserved-memory */
  Node = FdtSubnodeOffset (FdtPointer, 0, "reserved-memory");
  if (Node >= 0) {
    NumAddrCells = GetNumCells (FdtPointer, Node, "#address-cells");
    if (NumAddrCells <= 0) {
      return;
    }

    NumSizeCells = GetNumCells (FdtPointer, Node, "#size-cells");
    if (NumSizeCells <= 0) {
      return;
    }

    FdtForEachSubnode (SubNode, FdtPointer, Node) {
      RegProp = FdtGetProp (FdtPointer, SubNode, "reg", &Len);

      if ((RegProp != 0) && (Len == ((NumAddrCells + NumSizeCells) * sizeof (INT32)))) {
        Addr = Fdt32ToCpu (RegProp[0]);

        if (NumAddrCells > 1) {
          Addr = (Addr << 32) | Fdt32ToCpu (RegProp[1]);
        }

        RegProp += NumAddrCells;
        Size     = Fdt32ToCpu (RegProp[0]);

        if (NumSizeCells > 1) {
          Size = (Size << 32) | Fdt32ToCpu (RegProp[1]);
        }

        DEBUG ((
          DEBUG_INFO,
          "%a: Adding Reserved Memory Addr = 0x%llx, Size = 0x%llx\n",
          __func__,
          Addr,
          Size
          ));

        if (FdtGetProp (FdtPointer, SubNode, "no-map", &Len)) {
          BuildMemoryAllocationHob (
            Addr,
            Size,
            EfiReservedMemoryType
            );
        } else {
          BuildMemoryAllocationHob (
            Addr,
            Size,
            EfiBootServicesData
            );
        }
      }
    }
  }
}

/**
  Initialize memory hob based on the DTB information.

  @return EFI_SUCCESS     The memory hob added successfully.

**/
EFI_STATUS
EFIAPI
MemoryPeimInitializationCommon (
  IN  VOID  *DeviceTreeAddress
  )
{
  CONST UINT32                *RegProp;
  CONST CHAR8                 *Type;
  UINT64                      CurBase, CurSize;
  INT32                       Node, Prev;
  INT32                       Len;
  INT32                       Parent;
  INT32                       AddrCells, SizeCells;
  INT32                       NumRegions;
  INT32                       Index;

  // Look for memory nodes
  for (Prev = 0; ; Prev = Node) {
    Node = FdtNextNode (DeviceTreeAddress, Prev, NULL);
    if (Node < 0) {
      break;
    }

    // Check for memory node
    Type = FdtGetProp (DeviceTreeAddress, Node, "device_type", &Len);
    if ((Type != NULL) && (0 == AsciiStrnCmp (Type, "memory", Len))) {
      Parent = FdtParentOffset (DeviceTreeAddress, Node);
      if (Parent < 0) {
        continue;
      }
      AddrCells = FdtAddressCells (DeviceTreeAddress, Parent);
      if (AddrCells < 1) {
        continue;
      }
      SizeCells = FdtSizeCells (DeviceTreeAddress, Parent);
      if (SizeCells < 1) {
        continue;
      }

      RegProp = FdtGetProp (DeviceTreeAddress, Node, "reg", &Len);
      if ((RegProp == NULL) || (Len < sizeof (UINT32) * (AddrCells + SizeCells))) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to parse FDT memory node\n",
          __func__
          ));
        continue;
      }

      NumRegions = Len / (sizeof (UINT32) * (AddrCells + SizeCells));
      for (Index = 0; Index < NumRegions; Index++) {
        CurBase = MultiFdt32ToCpu64 (RegProp, AddrCells);
        RegProp += AddrCells;
        CurSize = MultiFdt32ToCpu64 (RegProp, SizeCells);
        RegProp += SizeCells;
        DEBUG ((
          DEBUG_INFO,
          "%a: System RAM @ 0x%lx - 0x%lx\n",
          __func__,
          CurBase,
          CurBase + CurSize - 1
          ));
        InitializeRamRegions (CurBase, CurSize);
      }
    }
  }

  AddReservedMemoryMap (DeviceTreeAddress);

  /* Make sure SEC is booting with bare mode */
  ASSERT ((RiscVGetSupervisorAddressTranslationRegister () & SATP64_MODE) == (SATP_MODE_OFF << SATP64_MODE_SHIFT));

  BuildMemoryTypeInformationHob ();

  return EFI_SUCCESS;
}
