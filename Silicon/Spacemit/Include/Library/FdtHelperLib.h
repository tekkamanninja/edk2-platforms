/** @file
  Provides FDT helper interfaces based on FdtLib.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FDT_HELPER_LIB_H__
#define __FDT_HELPER_LIB_H__

#include <Base.h>

//
// Common definitions of FDT interrupt level/sense encodings.
//
#define FDT_IRQ_TYPE_NONE             0
#define FDT_IRQ_TYPE_EDGE_RISING      1
#define FDT_IRQ_TYPE_EDGE_FALLING     2
#define FDT_IRQ_TYPE_EDGE_BOTH        (FDT_IRQ_TYPE_EDGE_FALLING | FDT_IRQ_TYPE_EDGE_RISING)
#define FDT_IRQ_TYPE_LEVEL_HIGH       4
#define FDT_IRQ_TYPE_LEVEL_LOW        8

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
  );

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
  );

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
  );

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
  );

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
  );

//
// Hierarchy type in "cpu-map"
//
typedef enum {
  FdtCpuMapHierarchyTypeSocket    = 0,
  FdtCpuMapHierarchyTypeCluster,
  FdtCpuMapHierarchyTypeCore,
  FdtCpuMapHierarchyTypeThread,
  FdtCpuMapHierarchyTypeMax,
} FDT_CPU_MAP_HIERARCHY_TYPE;

//
// A structure to store information in "cpu-map".
//
typedef struct {
  LIST_ENTRY                    Link;

  UINTN                         Id;
  FDT_CPU_MAP_HIERARCHY_TYPE    HierarchyType;
  UINTN                         HierarchyLevel;   // Starting from 1, top-down incremental

  //
  // For a leaf node, IsEnabled == TRUE means its status is "okay",
  // otherwise means "disabled".
  //
  // For a non-leaf node, IsEnabled == TRUE means it has at least one enabled
  // child node, otherwise means all of its child nodes are disabled.
  //
  BOOLEAN                       IsEnabled;

  LIST_ENTRY                    ChildList;  // A list of its child FDT_CPU_MAP_INFO(s).
} FDT_CPU_MAP_INFO;

//
// Helper macro to get FDT_CPU_MAP_INFO from its LIST_ENTRY member.
//
#define LIST_ENTRY_TO_FDT_CPU_MAP_INFO(Entry)   \
  BASE_CR (Entry, FDT_CPU_MAP_INFO, Link)

/**
  Get information from the FDT cpu-map node.

  @param  Fdt               The poiner to FDT blob.
  @param  CpuMapNodeOffset  The offset to the cpu-map node.
  @param  CpuMapInfoList    Pointer to a list storing the cpu-map information.
                            It should be released via FdtReleaseCpuMapInfoList()
                            when it is no longer needed.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtGetCpuMapInfo (
  IN CONST  VOID          *Fdt,
  IN        INTN          CpuMapNodeOffset,
  OUT       LIST_ENTRY    **CpuMapInfoList
  );

/**
  Release the information list got via FdtGetCpuMapInfo().

  @param  CpuMapInfoList  Pointer to the information list got via FdtGetCpuMapInfo().

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
FdtReleaseCpuMapInfo (
  IN OUT    LIST_ENTRY    *CpuMapInfoList
  );

//
// Flags for PCI address.
//
// The meaning of each bit is the same as that in the phys.hi cell of PCI
// address described in the PCI Bus Supplement to IEEE Std 1275-1994 (Open
// Firmware Core Specification). For more information, please refer to:
// https://www.devicetree.org/open-firmware/home.html#OFDbusPCI
//
#define FDT_PCI_ADDR_FLAGS_RELOCATABLE_SHIFT          31
#define FDT_PCI_ADDR_FLAGS_RELOCATABLE_MASK           0x1
#define   FDT_PCI_ADDR_FLAGS_RELOCATABLE              0x0
#define FDT_PCI_ADDR_FLAGS_PREFETCHABLE_SHIFT         30
#define FDT_PCI_ADDR_FLAGS_PREFETCHABLE_MASK          0x1
#define   FDT_PCI_ADDR_FLAGS_PREFETCHABLE             0x1
#define FDT_PCI_ADDR_FLAGS_ALIASED_SHIFT              29
#define FDT_PCI_ADDR_FLAGS_ALIASED_MASK               0x1
#define   FDT_PCI_ADDR_FLAGS_ALIASED                  0x1
#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_SHIFT           24
#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_MASK            0x3
#define   FDT_PCI_ADDR_FLAGS_SPACE_TYPE_CONFIG        0x0
#define   FDT_PCI_ADDR_FLAGS_SPACE_TYPE_IO            0x1
#define   FDT_PCI_ADDR_FLAGS_SPACE_TYPE_MEM32         0x2
#define   FDT_PCI_ADDR_FLAGS_SPACE_TYPE_MEM64         0x3
#define FDT_PCI_ADDR_FLAGS_BUS_NUM_SHIFT              16
#define FDT_PCI_ADDR_FLAGS_BUS_NUM_MASK               0xff
#define FDT_PCI_ADDR_FLAGS_DEV_NUM_SHIFT              11
#define FDT_PCI_ADDR_FLAGS_DEV_NUM_MASK               0x1f
#define FDT_PCI_ADDR_FLAGS_FUNC_NUM_SHIFT             8
#define FDT_PCI_ADDR_FLAGS_FUNC_NUM_MASK              0x7
#define FDT_PCI_ADDR_FLAGS_REG_NUM_SHIFT              0
#define FDT_PCI_ADDR_FLAGS_REG_NUM_MASK               0xff

#define FDT_PCI_ADDR_FLAGS_BDF_SHIFT                  8
#define FDT_PCI_ADDR_FLAGS_BDF_MASK                   0xffff

#define FDT_PCI_ADDR_FLAGS_GET(Name, Flags)                     \
  (((Flags) >> (FDT_PCI_ADDR_FLAGS_ ## Name ## _SHIFT)) & (FDT_PCI_ADDR_FLAGS_ ## Name ## _MASK))

#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_GET(Flags)                \
  FDT_PCI_ADDR_FLAGS_GET (SPACE_TYPE, Flags)
#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_IS_CONFIG(Flags)          \
  (FDT_PCI_ADDR_FLAGS_SPACE_TYPE_GET (Flags) == FDT_PCI_ADDR_FLAGS_SPACE_TYPE_CONFIG)
#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_IS_IO(Flags)              \
  (FDT_PCI_ADDR_FLAGS_SPACE_TYPE_GET (Flags) == FDT_PCI_ADDR_FLAGS_SPACE_TYPE_IO)
#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_IS_MEM32(Flags)           \
  (FDT_PCI_ADDR_FLAGS_SPACE_TYPE_GET (Flags) == FDT_PCI_ADDR_FLAGS_SPACE_TYPE_MEM32)
#define FDT_PCI_ADDR_FLAGS_SPACE_TYPE_IS_MEM64(Flags)           \
  (FDT_PCI_ADDR_FLAGS_SPACE_TYPE_GET (Flags) == FDT_PCI_ADDR_FLAGS_SPACE_TYPE_MEM64)

#define FDT_PCI_ADDR_FLAGS_IS_PREFETCHABLE(Flags)               \
  (FDT_PCI_ADDR_FLAGS_GET (PREFETCHABLE, Flags) == FDT_PCI_ADDR_FLAGS_PREFETCHABLE)

//
// A structure to store one of the parent-child address mapping information in
// "ranges" or "dma-ranges" property in a PCI bus bridge FDT node.
//
typedef struct {
  UINT32    Flags;
  UINT64    ChildAddr;
  UINT64    ParentAddr;
  UINT64    Size;
} FDT_PCI_BUS_BRIDGE_RANGES_INFO;

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
  );

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
  );

//
// Mask information in "interrupt-map-mask" property of PCI bus FDT node.
//
typedef struct {
  UINT32    DevMask;
  UINT32    IrqMask;
} FDT_PCI_INTERRUPT_MAP_MASK;

//
// The interrupt controller specified by interrupt-parent component in
// "interrupt-map" property of PCI bus FDT node.
//
typedef struct {
  LIST_ENTRY      Link;

  INTN            NodeOffset;
  UINTN           AddrCells;
  UINTN           IrqCells;
} FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER;

//
// Helper macro to get FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER from its LIST_ENTRY member.
//
#define LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER(Entry)  \
  BASE_CR (Entry, FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER, Link)

//
// Interrupt pin of PCI legacy INTA ~ INTD.
//
#define FDT_PCI_IRQ_PIN_INTA  1
#define FDT_PCI_IRQ_PIN_INTB  2
#define FDT_PCI_IRQ_PIN_INTC  3
#define FDT_PCI_IRQ_PIN_INTD  4

//
// Information of one entry of "interrupt-map" property in a PCI bus FDT node.
//
typedef struct {
  LIST_ENTRY                                Link;

  UINT32                                    DevNum;
  UINT32                                    PciIrqPin;

  FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER      *IrqController;
  UINT32                                    IrqNum;
  UINT32                                    IrqType;  // FDT_IRQ_TYPE_xxx
} FDT_PCI_INTERRUPT_MAP_ENTRY;

//
// Helper macro to get FDT_PCI_INTERRUPT_MAP_ENTRY from its LIST_ENTRY member.
//
#define LIST_ENTRY_TO_FDT_PCI_INTERRUPT_MAP_ENTRY(Entry)  \
  BASE_CR (Entry, FDT_PCI_INTERRUPT_MAP_ENTRY, Link)

//
// Information of "interrupt-map" property in a PCI bus FDT node.
//
typedef struct {
  FDT_PCI_INTERRUPT_MAP_MASK  Mask;

  // A list of FDT_PCI_INTERRUPT_MAP_IRQ_CONTROLLER
  LIST_ENTRY                  IrqControllerList;
  UINTN                       NumIrqControllers;

  // A list of FDT_PCI_INTERRUPT_MAP_ENTRY
  LIST_ENTRY                  MapEntryList;
  UINTN                       NumMapEntries;
} FDT_PCI_INTERRUPT_MAP_INFO;

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
  );

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
  );

#endif /* ifndef __FDT_HELPER_LIB_H__ */
