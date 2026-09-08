/** @file
  Synopsys DesignWare PCIe controller interfaces

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <BitOps.h>
#include <WordPart.h>
#include <PciRegs.h>
#include <IndustryStandard/Pci.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/PciRootBrdigeResourceConfig.h>
#include <Library/DmaIoMmuConfig.h>

#include <Library/DesignWarePcieControllerLib.h>

//
// The default address offset between DbiBase and AtuBase. If AtuBase not set,
// the driver core automatically derives AtuBase from DbiBase using this offset.
//
#define DEFAULT_DBI_ATU_OFFSET  (0x3 << 20)

// PCI IDs
#define PCI_CLASS_BRIDGE_PCI    0x0604

// Helper macro to traverse each entry in a list>
#define LIST_FOR_EACH_ENTRY(Entry, Head)        \
  for (Entry = GetFirstNode (Head);             \
       !IsNull (Head, Entry);                   \
       Entry = GetNextNode (Head, Entry))

// Helper macro to get the DW_PCIE_RESOURCE instance from its LIST_ENTRY member.
#define LIST_ENTRY_TO_DW_PCIE_RESOURCE(Entry)   \
  BASE_CR (Entry, DW_PCIE_RESOURCE, Link)

//
// An array to store all the pointers to DW_PCIE_ROOT_PORTs.
// It use segment number as the array index.
//
CONST DW_PCIE_ROOT_PORT **gDwPcieRootPorts = NULL;

STATIC
inline
DW_PCIE_RESOURCE *
DwPcieAllocateResource (
  VOID
  )
{
  return (DW_PCIE_RESOURCE *) AllocateZeroPool (sizeof (DW_PCIE_RESOURCE));
}

STATIC
inline
VOID
DwPcieFreeResource (
  IN OUT  DW_PCIE_RESOURCE     *DwPcieResource
  )
{
  FreePool (DwPcieResource);
}

/**
  Free all resources in a DW_PCIE instance.

  @param  DwPcie         Pointer to DW_PCIE instance.

**/
STATIC
VOID
DwPcieFreeAllResources (
  IN OUT  DW_PCIE     *DwPcie
  )
{
  LIST_ENTRY        *Entry;
  LIST_ENTRY        *NextEntry;
  DW_PCIE_RESOURCE  *Resource;

  Entry = GetFirstNode (&DwPcie->Rp.Resources);
  while (!IsNull (&DwPcie->Rp.Resources, Entry)) {
    Resource = LIST_ENTRY_TO_DW_PCIE_RESOURCE (Entry);
    NextEntry = GetNextNode (&DwPcie->Rp.Resources, Entry);

    RemoveEntryList (Entry);
    DwPcieFreeResource (Resource);

    Entry = NextEntry;
  }

  Entry = GetFirstNode (&DwPcie->Rp.IbResources);
  while (!IsNull (&DwPcie->Rp.IbResources, Entry)) {
    Resource = LIST_ENTRY_TO_DW_PCIE_RESOURCE (Entry);
    NextEntry = GetNextNode (&DwPcie->Rp.IbResources, Entry);

    RemoveEntryList (Entry);
    DwPcieFreeResource (Resource);

    Entry = NextEntry;
  }
}

/**
  Allocate a DW_PCIE instance.

  @param  Id          The ID of the DW_PCIE instance.

  @retval   A pointer to the allocated DW_PCIE instances, or NULL on failure.

**/
DW_PCIE *
EFIAPI
DwPcieAllocateInstance (
  IN  UINT32   Id
  )
{
  DW_PCIE *DwPcie = NULL;

  DwPcie = (DW_PCIE *) AllocateZeroPool (sizeof (DW_PCIE));
  if (DwPcie == NULL) {
    return NULL;
  }

  DwPcie->Id = Id;

  InitializeListHead (&DwPcie->Rp.Resources);
  InitializeListHead (&DwPcie->Rp.IbResources);

  return DwPcie;
}

/**
  Free a DW_PCIE instance.

  @param  DwPcie         Pointer to DW_PCIE instance.

**/
VOID
EFIAPI
DwPcieFreeInstance (
  IN OUT  DW_PCIE     *DwPcie
  )
{
  DwPcieFreeAllResources (DwPcie);
  FreePool (DwPcie);
}

STATIC
EFI_STATUS
DwPcieRead (
  IN  UINT64      Addr,
  IN  UINT32      Size,
  OUT UINT32      *Value
  )
{
  DEBUG ((DEBUG_VERBOSE, "%a: Addr: 0x%lx, Size: %u\n", __func__, Addr, Size));

  switch (Size) {
    case 1:
      *Value = MmioRead8 (Addr);
      break;
    case 2:
      *Value = MmioRead16 (Addr);
      break;
    case 4:
      *Value = MmioRead32 (Addr);
      break;
    default:
      return EFI_UNSUPPORTED;
  }
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DwPcieWrite (
  IN  UINT64      Addr,
  IN  UINT32      Size,
  IN  UINT32      Value
  )
{
  DEBUG ((DEBUG_VERBOSE, "%a: Addr: 0x%lx, Size: %u, Value: 0x%x\n",
          __func__, Addr, Size, Value));

  switch (Size) {
    case 1:
      MmioWrite8 (Addr, Value);
      break;
    case 2:
      MmioWrite16 (Addr, Value);
      break;
    case 4:
      MmioWrite32 (Addr, Value);
      break;
    default:
      return EFI_UNSUPPORTED;
  }
  return EFI_SUCCESS;
}

UINT32
EFIAPI
DwPcieReadDbi (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT32    Size
  )
{
  EFI_STATUS  Status;
  UINT32      Value;

  Status = DwPcieRead (DwPcie->Reg.DbiBase + Reg, Size, &Value);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Read DBI address 0x%lx failed\n",
            __func__, DwPcie->Id, DwPcie->Reg.DbiBase + Reg));
  }

  return Value;
}

VOID
EFIAPI
DwPcieWriteDbi (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Reg,
  IN        UINT32    Size,
  IN        UINT32    Value
  )
{
  EFI_STATUS  Status;

  Status = DwPcieWrite (DwPcie->Reg.DbiBase + Reg, Size, Value);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Write value 0x%x to DBI address 0x%lx failed\n",
            __func__, DwPcie->Id, Value, DwPcie->Reg.DbiBase + Reg));
  }
}

STATIC
UINT64
DwPcieSelectAtu (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Dir,
  IN        UINT32    Index
  )
{
  if (DW_PCIE_CAP_IS (DwPcie, DW_PCIE_CAP_IATU_UNROLL)) {
    return DwPcie->Reg.AtuBase + DW_PCIE_ATU_UNROLL_BASE (Dir, Index);
  }

  DwPcieWriteDbi32 (DwPcie, DW_PCIE_ATU_VIEWPORT, Dir | Index);
  return DwPcie->Reg.AtuBase;
}

STATIC
UINT32
DwPcieReadAtu32 (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    Dir,
  IN        UINT32    Index,
  IN        UINT32    Reg
  )
{
  UINT64          Base;
  EFI_STATUS      Status;
  UINT32          Value;

  Base = DwPcieSelectAtu (DwPcie, Dir, Index);

  Status = DwPcieRead (Base + Reg, 4, &Value);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Read ATU address 0x%lx failed\n",
            __func__, DwPcie->Id, Base + Reg));
  }

  return Value;
}

STATIC
VOID
DwPcieWriteAtu32 (
  IN CONST  DW_PCIE     *DwPcie,
  IN        UINT32      Dir,
  IN        UINT32      Index,
  IN        UINT32      Reg,
  IN        UINT32      Value
  )
{
  UINT64        Base;
  EFI_STATUS    Status;

  Base = DwPcieSelectAtu (DwPcie, Dir, Index);

  Status = DwPcieWrite (Base + Reg, 4, Value);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Write value 0x%x to ATU address 0x%lx failed\n",
            __func__, DwPcie->Id, Value, Base + Reg));
  }
}

STATIC
inline
UINT32
DwPcieReadAtuOb32 (
  IN CONST  DW_PCIE     *DwPcie,
  IN        UINT32      Index,
  IN        UINT32      Reg
  )
{
  return DwPcieReadAtu32 (DwPcie, DW_PCIE_ATU_REGION_DIR_OB, Index, Reg);
}

STATIC
inline
VOID
DwPcieWriteAtuOb32 (
  IN CONST  DW_PCIE     *DwPcie,
  IN        UINT32      Index,
  IN        UINT32      Reg,
  IN        UINT32      Value
  )
{
  DwPcieWriteAtu32 (DwPcie, DW_PCIE_ATU_REGION_DIR_OB, Index, Reg, Value);
}

STATIC
inline
UINT32
DwPcieReadAtuIb32 (
  IN CONST  DW_PCIE     *DwPcie,
  IN        UINT32      Index,
  IN        UINT32      Reg
  )
{
  return DwPcieReadAtu32 (DwPcie, DW_PCIE_ATU_REGION_DIR_IB, Index, Reg);
}

STATIC
inline
VOID
DwPcieWriteAtuIb32 (
  IN CONST  DW_PCIE     *DwPcie,
  IN        UINT32      Index,
  IN        UINT32      Reg,
  IN        UINT32      Value
  )
{
  DwPcieWriteAtu32 (DwPcie, DW_PCIE_ATU_REGION_DIR_IB, Index, Reg, Value);
}

/**
  Get DesignWare PCIe controller config for DW_PCIE instance.

  @param  DwPcie                    Pointer to DW_PCIE instance.
  @param  DwPcieControllerConfig    Pointer to DW PCIe controller config.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
STATIC
EFI_STATUS
DwPcieGetDwPcieControllerConfig (
  IN OUT    DW_PCIE                         *DwPcie,
  IN CONST  DW_PCIE_CONTROLLER_CONFIG_DATA  *DwPcieControllerConfig
  )
{
  //
  // TODO: Is the hardware DBI space size always 4KB?
  //
  // Here we assume that the DBI space size is 4KB, not matter what the DbiSize
  // is set. The DbiSize in DwPcieControllerConfig is just a flag to determine
  // whether the DbiBase is set.
  //
  if (DwPcieControllerConfig->Reg.DbiSize == 0) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: DBI space not specified\n", __func__, DwPcie->Id));
    return EFI_UNSUPPORTED;
  }
  DwPcie->Reg.DbiBase = DwPcieControllerConfig->Reg.DbiBase;
  DwPcie->Reg.DbiSize = SIZE_4KB;
  MapRegToGcdMmioSpace (DwPcie->Reg.DbiBase, DwPcie->Reg.DbiSize);

  //
  // Similar to DbiSize, we assume DBI2 space size is 4KB and Dbi2Size in
  // DwPcieControllerConfig is just a flag to determine whether the Dbi2Base is
  // set.
  //
  if (DwPcieControllerConfig->Reg.Dbi2Size != 0) {
    DwPcie->Reg.Dbi2Base = DwPcieControllerConfig->Reg.Dbi2Base;
    DwPcie->Reg.Dbi2Size = SIZE_4KB;
  } else {
    DwPcie->Reg.Dbi2Base = DwPcie->Reg.DbiBase + SIZE_4KB;
    DwPcie->Reg.Dbi2Size = SIZE_4KB;
  }
  MapRegToGcdMmioSpace (DwPcie->Reg.Dbi2Base, DwPcie->Reg.Dbi2Size);

  if (DwPcieControllerConfig->Reg.AtuSize != 0) {
    DwPcie->Reg.AtuBase = DwPcieControllerConfig->Reg.AtuBase;
    DwPcie->Reg.AtuSize = DwPcieControllerConfig->Reg.AtuSize;
  } else {
    DwPcie->Reg.AtuBase = DwPcie->Reg.DbiBase + DEFAULT_DBI_ATU_OFFSET;
    // Set a default value suitable for at most 8 in and 8 out windows.
    DwPcie->Reg.AtuSize = SIZE_4KB;
  }
  MapRegToGcdMmioSpace (DwPcie->Reg.AtuBase, DwPcie->Reg.AtuSize);

  if (DwPcieControllerConfig->NumLanes != 0) {
    DwPcie->NumLanes = DwPcieControllerConfig->NumLanes;
  }

  if (DwPcieControllerConfig->MaxLinkSpeed != 0) {
    DwPcie->MaxLinkSpeed = DwPcieControllerConfig->MaxLinkSpeed;
  }

  DwPcie->CfgShiftModeEnabled = DwPcieControllerConfig->CfgShiftModeEnabled;

  DwPcie->EcamEnabled = DwPcieControllerConfig->EcamEnabled;

  return EFI_SUCCESS;
}

STATIC
VOID
DwPciePrintInformations (
  IN CONST  DW_PCIE     *DwPcie
  )
{
  DEBUG_CODE_BEGIN ();
    LIST_ENTRY          *Entry;
    DW_PCIE_RESOURCE    *Resource;
    INTN                Index;

    if (DwPcie == NULL) {
      return;
    }

    DEBUG ((DEBUG_INFO, "DwPcie [%u] Info:\n", DwPcie->Id));
    DEBUG ((DEBUG_INFO, "  Reg:\n"));
    DEBUG ((DEBUG_INFO, "    DbiBase: 0x%lx\n", DwPcie->Reg.DbiBase));
    DEBUG ((DEBUG_INFO, "    DbiSize: 0x%lx\n", DwPcie->Reg.DbiSize));
    DEBUG ((DEBUG_INFO, "    Dbi2Base: 0x%lx\n", DwPcie->Reg.Dbi2Base));
    DEBUG ((DEBUG_INFO, "    Dbi2Size: 0x%lx\n", DwPcie->Reg.Dbi2Size));
    DEBUG ((DEBUG_INFO, "    AtuBase: 0x%lx\n", DwPcie->Reg.AtuBase));
    DEBUG ((DEBUG_INFO, "    AtuSize: 0x%lx\n", DwPcie->Reg.AtuSize));
    DEBUG ((DEBUG_INFO, "  Version: 0x%08x\n", DwPcie->Version));
    DEBUG ((DEBUG_INFO, "  Type: 0x%08x\n", DwPcie->Type));
    DEBUG ((DEBUG_INFO, "  Caps: 0x%08x\n", DwPcie->Type));
    DEBUG ((DEBUG_INFO, "  NumObWindows: %u\n", DwPcie->NumObWindows));
    DEBUG ((DEBUG_INFO, "  NumIbWindows: %u\n", DwPcie->NumIbWindows));
    DEBUG ((DEBUG_INFO, "  RegionAlign: 0x%x (%u KB)\n",
            DwPcie->RegionAlign, DwPcie->RegionAlign / SIZE_1KB));
    DEBUG ((DEBUG_INFO, "  RegionLimit: 0x%lx (%lu GB)\n",
            DwPcie->RegionLimit, (DwPcie->RegionLimit + 1) / SIZE_1GB));
    DEBUG ((DEBUG_INFO, "  NumLanes: %u\n", DwPcie->NumLanes));
    DEBUG ((DEBUG_INFO, "  MaxLinkSpeed: %u\n", DwPcie->MaxLinkSpeed));
    DEBUG ((DEBUG_INFO, "  N_FTS: [0] 0x%x [1] 0x%x\n", DwPcie->NFts[0], DwPcie->NFts[1]));
    DEBUG ((DEBUG_INFO, "  CfgShiftModeEnabled: %u\n", DwPcie->CfgShiftModeEnabled));
    DEBUG ((DEBUG_INFO, "  EcamEnabled: %u\n", DwPcie->EcamEnabled));
    DEBUG ((DEBUG_INFO, "  Root Port:\n"));
    DEBUG ((DEBUG_INFO, "    ConfigBase: 0x%lx\n", DwPcie->Rp.ConfigBase));
    DEBUG ((DEBUG_INFO, "    ConfigSize: 0x%lx\n", DwPcie->Rp.ConfigSize));
    DEBUG ((DEBUG_INFO, "    BusMin: 0x%lx\n", DwPcie->Rp.BusMin));
    DEBUG ((DEBUG_INFO, "    BusMax: 0x%lx\n", DwPcie->Rp.BusMax));
    DEBUG ((DEBUG_INFO, "    Segment: 0x%x\n", DwPcie->Rp.Segment));

    Index = 0;
    LIST_FOR_EACH_ENTRY (Entry, &DwPcie->Rp.Resources) {
      Resource = LIST_ENTRY_TO_DW_PCIE_RESOURCE (Entry);
      DEBUG ((DEBUG_INFO,
              "    Resource %d: PciBase: 0x%lx, CpuBase: 0x%lx, Size: 0x%lx, Flags: 0x%lx\n",
              Index, Resource->PciBase, Resource->CpuBase, Resource->Size, Resource->Flags));
      Index++;
    }

    Index = 0;
    LIST_FOR_EACH_ENTRY (Entry, &DwPcie->Rp.IbResources) {
      Resource = LIST_ENTRY_TO_DW_PCIE_RESOURCE (Entry);
      DEBUG ((DEBUG_INFO,
              "    IbResource %d: PciBase: 0x%lx, CpuBase: 0x%lx, Size: 0x%lx, Flags: 0x%lx\n",
              Index, Resource->PciBase, Resource->CpuBase, Resource->Size, Resource->Flags));
      Index++;
    }
  DEBUG_CODE_END ();
}

/**
  Get Root Bridge resource config for DW_PCIE instance.

  @param  DwPcie                    Pointer to DW_PCIE instance.
  @param  RootBridgeResourceConfig  Pointer to Root Bridge resource config.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
STATIC
EFI_STATUS
DwPcieGetRootBridgeResourceConfig (
  IN OUT    DW_PCIE                               *DwPcie,
  IN CONST  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_DATA  *RootBridgeResourceConfig
  )
{
  DW_PCIE_RESOURCE  *Resource;

  //
  // Configuration space
  //
  if (RootBridgeResourceConfig->ConfigSize == 0) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Root Bridge configuration space not specified\n",
            __func__, DwPcie->Id));
    return EFI_UNSUPPORTED;
  }
  DwPcie->Rp.ConfigBase = RootBridgeResourceConfig->ConfigBase;
  DwPcie->Rp.ConfigSize = RootBridgeResourceConfig->ConfigSize;

  //
  // Bus range
  //
  DwPcie->Rp.BusMin = RootBridgeResourceConfig->BusBase;
  DwPcie->Rp.BusMax = RootBridgeResourceConfig->BusLimit;
  if (DwPcie->Rp.BusMin > DwPcie->Rp.BusMax) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: invalid bus range [%u, %u]\n",
            __func__, DwPcie->Id, DwPcie->Rp.BusMin, DwPcie->Rp.BusMax));
    return EFI_UNSUPPORTED;
  }

  //
  // Segment number
  //
  DwPcie->Rp.Segment = RootBridgeResourceConfig->Segment;

  //
  // IO space
  //
  if (RootBridgeResourceConfig->Io.PciSize) {
    Resource = DwPcieAllocateResource ();
    ASSERT (Resource != NULL);

    Resource->PciBase = RootBridgeResourceConfig->Io.PciBase;
    Resource->CpuBase = RootBridgeResourceConfig->Io.CpuBase;
    Resource->Size = RootBridgeResourceConfig->Io.PciSize;
    Resource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (IO);

    InsertTailList (&DwPcie->Rp.Resources, &Resource->Link);
  }

  //
  // 32-bit memory space
  //
  if (RootBridgeResourceConfig->Mem.PciSize) {
    Resource = DwPcieAllocateResource ();
    ASSERT (Resource != NULL);

    Resource->PciBase = RootBridgeResourceConfig->Mem.PciBase;
    Resource->CpuBase = RootBridgeResourceConfig->Mem.CpuBase;
    Resource->Size = RootBridgeResourceConfig->Mem.PciSize;
    Resource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (MEM32);

    InsertTailList (&DwPcie->Rp.Resources, &Resource->Link);
  }

  //
  // 64-bit memory space
  //
  if (RootBridgeResourceConfig->Mem64.PciSize) {
    Resource = DwPcieAllocateResource ();
    ASSERT (Resource != NULL);

    Resource->PciBase = RootBridgeResourceConfig->Mem64.PciBase;
    Resource->CpuBase = RootBridgeResourceConfig->Mem64.CpuBase;
    Resource->Size = RootBridgeResourceConfig->Mem64.PciSize;
    Resource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (MEM64);

    InsertTailList (&DwPcie->Rp.Resources, &Resource->Link);
  }

  //
  // 32-bit prefetchable memory space
  //
  if (RootBridgeResourceConfig->PMem.PciSize) {
    Resource = DwPcieAllocateResource ();
    ASSERT (Resource != NULL);

    Resource->PciBase = RootBridgeResourceConfig->PMem.PciBase;
    Resource->CpuBase = RootBridgeResourceConfig->PMem.CpuBase;
    Resource->Size = RootBridgeResourceConfig->PMem.PciSize;
    Resource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (MEM32);
    Resource->Flags |= DW_PCIE_RESOURCE_PREFETCHABLE_SET ();

    InsertTailList (&DwPcie->Rp.Resources, &Resource->Link);
  }

  //
  // 64-bit prefetchable memory space
  //
  if (RootBridgeResourceConfig->PMem64.PciSize) {
    Resource = DwPcieAllocateResource ();
    ASSERT (Resource != NULL);

    Resource->PciBase = RootBridgeResourceConfig->PMem64.PciBase;
    Resource->CpuBase = RootBridgeResourceConfig->PMem64.CpuBase;
    Resource->Size = RootBridgeResourceConfig->PMem64.PciSize;
    Resource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (MEM64);
    Resource->Flags |= DW_PCIE_RESOURCE_PREFETCHABLE_SET ();

    InsertTailList (&DwPcie->Rp.Resources, &Resource->Link);
  }

  return EFI_SUCCESS;
}

/**
  Get DMA mappings config for DW_PCIE instance.

  @param  DwPcie            Pointer to DW_PCIE instance.
  @param  DmaMappings       Pointer to DMA mappings config.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
STATIC
EFI_STATUS
DwPcieGetDmaMappingsConfig (
  IN OUT    DW_PCIE                 *DwPcie,
  IN CONST  DMA_IOMMU_MAPPINGS      *DmaMappings
  )
{
  INTN                        Index;
  DW_PCIE_RESOURCE            *IbResource;
  CONST DMA_IOMMU_MAPPING     *Mapping;

  for (Index = 0; Index < DmaMappings->Num; Index++) {
    Mapping = &DmaMappings->Data[Index];

    // Skip empty mapping.
    if (Mapping->Size == 0) {
      continue;
    }

    IbResource = DwPcieAllocateResource ();
    if (IbResource == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    IbResource->PciBase = Mapping->DmaAddr;
    IbResource->CpuBase = Mapping->CpuAddr;
    IbResource->Size = Mapping->Size;
    if (((IbResource->PciBase + IbResource->Size - 1) & ((UINT64) GENMASK_ULL (63, 32))) != 0) {
      IbResource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (MEM64);
    } else {
      IbResource->Flags |= DW_PCIE_RESOURCE_TYPE_SET (MEM32);
    }

    InsertTailList (&DwPcie->Rp.IbResources, &IbResource->Link);
  }

  return EFI_SUCCESS;
}

/**
  Enable the write permission to DBI read-only registers.

  @param  DwPcie      Pointer to DW_PCIE instance.

**/
STATIC
inline
VOID
DwPcieDbiRoWrEnable (
  IN CONST  DW_PCIE   *DwPcie
  )
{
  UINT32 Reg;
  UINT32 Val;

  Reg = DW_PCIE_MISC_CONTROL_1_OFF;
  Val = DwPcieReadDbi32 (DwPcie, Reg);
  Val |= DW_PCIE_DBI_RO_WR_EN;
  DwPcieWriteDbi32 (DwPcie, Reg, Val);
}

/**
  Disable the write permission to DBI read-only registers.

  @param  DwPcie      Pointer to DW_PCIE instance.

**/
STATIC
inline
VOID
DwPcieDbiRoWrDisable (
  IN CONST  DW_PCIE   *DwPcie
  )
{
  UINT32 Reg;
  UINT32 Val;

  Reg = DW_PCIE_MISC_CONTROL_1_OFF;
  Val = DwPcieReadDbi32 (DwPcie, Reg);
  Val &= ~DW_PCIE_DBI_RO_WR_EN;
  DwPcieWriteDbi32 (DwPcie, Reg, Val);
}

STATIC
VOID
DwPcieVersionDetect (
  IN OUT  DW_PCIE   *DwPcie
  )
{
  UINT32 Version;

  // The content of the CSR is zero on DWC PCIe older than v4.70a
  Version = DwPcieReadDbi32 (DwPcie, DW_PCIE_VERSION_NUMBER);
  if (Version == 0) {
    return;
  }

  if (DwPcie->Version && DwPcie->Version != Version) {
    DEBUG ((DEBUG_WARN, "(%a) DwPcie %u: Version don't match (0x%08x != 0x%08x)\n",
            __func__, DwPcie->Id, DwPcie->Version, Version));
  } else {
    DwPcie->Version = Version;
  }

  Version = DwPcieReadDbi32 (DwPcie, DW_PCIE_VERSION_TYPE);
  if (DwPcie->Type && DwPcie->Type != Version) {
    DEBUG ((DEBUG_WARN, "(%a) DwPcie %u: Type don't match (0x%08x != 0x%08x)\n",
            __func__, DwPcie->Id, DwPcie->Type, Version));
  } else {
    DwPcie->Type = Version;
  }
}

STATIC
VOID
DwPcieIatuDetect (
  IN OUT  DW_PCIE   *DwPcie
  )
{
  UINT32  Value;
  INTN    MaxRegion;
  INTN    Ob;
  INTN    Ib;
  UINT32  Dir;
  UINT32  Min;
  UINT64  Max;

  Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_ATU_VIEWPORT);
  if (Value == 0xFFFFFFFF) {
    DW_PCIE_CAP_SET (DwPcie, DW_PCIE_CAP_IATU_UNROLL);

    MaxRegion = MIN ((INTN)(DwPcie->Reg.AtuSize) / 512, 256);
  } else {
    DwPcie->Reg.AtuBase = DwPcie->Reg.DbiBase + DW_PCIE_ATU_VIEWPORT_BASE;
    DwPcie->Reg.AtuSize = DW_PCIE_ATU_VIEWPORT_SIZE;

    DwPcieWriteDbi32 (DwPcie, DW_PCIE_ATU_VIEWPORT, 0xFF);
    MaxRegion = DwPcieReadDbi32 (DwPcie, DW_PCIE_ATU_VIEWPORT) + 1;
  }

  for (Ob = 0; Ob < MaxRegion; Ob++) {
    DwPcieWriteAtuOb32 (DwPcie, Ob, DW_PCIE_ATU_LOWER_TARGET, 0x11110000);
    Value = DwPcieReadAtuOb32 (DwPcie, Ob, DW_PCIE_ATU_LOWER_TARGET);
    if (Value != 0x11110000) {
      break;
    }
  }

  for (Ib = 0; Ib < MaxRegion; Ib++) {
    DwPcieWriteAtuIb32 (DwPcie, Ib, DW_PCIE_ATU_LOWER_TARGET, 0x11110000);
    Value = DwPcieReadAtuIb32 (DwPcie, Ib, DW_PCIE_ATU_LOWER_TARGET);
    if (Value != 0x11110000) {
      break;
    }
  }

  if (Ob) {
    Dir = DW_PCIE_ATU_REGION_DIR_OB;
  } else if (Ib) {
    Dir = DW_PCIE_ATU_REGION_DIR_IB;
  } else {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: No iATU regions found\n", __func__, DwPcie->Id));
    return;
  }

  DwPcieWriteAtu32 (DwPcie, Dir, 0, DW_PCIE_ATU_LIMIT, 0x0);
  Min = DwPcieReadAtu32 (DwPcie, Dir, 0, DW_PCIE_ATU_LIMIT);

  if (DW_PCIE_VER_IS_GE(DwPcie, DW_PCIE_VER_460A)) {
    DwPcieWriteAtu32 (DwPcie, Dir, 0, DW_PCIE_ATU_UPPER_LIMIT, 0xFFFFFFFF);
    Max = DwPcieReadAtu32 (DwPcie, Dir, 0, DW_PCIE_ATU_UPPER_LIMIT);
  } else {
    Max = 0;
  }

  DwPcie->NumObWindows = Ob;
  DwPcie->NumIbWindows = Ib;
  DwPcie->RegionAlign = 1 << Fls (Min);
  DwPcie->RegionLimit = (Max << 32) | (SIZE_4GB - 1);

  DEBUG ((DEBUG_INFO,
          "DwPcie %u: iATU: unroll %a, %u ob, %u ib, align %uK, limit %luG\n",
          DwPcie->Id,
          DW_PCIE_CAP_IS (DwPcie, DW_PCIE_CAP_IATU_UNROLL) ? "T" : "F",
          DwPcie->NumObWindows,
          DwPcie->NumIbWindows,
          DwPcie->RegionAlign / SIZE_1KB,
          (DwPcie->RegionLimit + 1) / SIZE_1GB
          ));
}

STATIC
UINT8
DwPcieFindNextCap (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT8     CapPtr,
  IN        UINT8     Cap
  )
{
  UINT8   CapId;
  UINT8   NextCapPtr;
  UINT16  Reg;

  if (CapPtr == 0) {
    return 0;
  }

  Reg = DwPcieReadDbi16 (DwPcie, CapPtr);
  CapId = (Reg & 0x00ff);

  if (CapId > PCI_CAP_ID_MAX) {
    return 0;
  }

  if (CapId == Cap) {
    return CapPtr;
  }

  NextCapPtr = (Reg & 0xff00) >> 8;
  return DwPcieFindNextCap (DwPcie, NextCapPtr, Cap);
}

STATIC
UINT8
DwPcieFindCapability (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT8     Cap
  )
{
  UINT8   NextCapPtr;
  UINT16  Reg;

  Reg = DwPcieReadDbi16 (DwPcie, PCI_CAPABILITY_LIST);
  NextCapPtr = (Reg & 0x00ff);

  return DwPcieFindNextCap (DwPcie, NextCapPtr, Cap);
}

STATIC
VOID
DwPcieLinkSetMaxSpeed (
  IN OUT  DW_PCIE   *DwPcie
  )
{
  UINT32  Cap;
  UINT32  Ctrl2;
  UINT32  LinkSpeed;
  UINT8   Offset;

  Offset = DwPcieFindCapability (DwPcie, PCI_CAP_ID_EXP);

  Cap = DwPcieReadDbi32 (DwPcie, Offset + PCI_EXP_LNKCAP);

  //
  // Even if the platform doesn't want to limit the maximum link speed,
  // just cache the hardware default value so that the vendor drivers can
  // use it to do any link specific configuration.
  //
  if (DwPcie->MaxLinkSpeed < 1) {
    DwPcie->MaxLinkSpeed = (Cap & PCI_EXP_LNKCAP_SLS) >> 0;
    return;
  }

  Ctrl2 = DwPcieReadDbi32 (DwPcie, Offset + PCI_EXP_LNKCTL2);
  Ctrl2 &= ~PCI_EXP_LNKCTL2_TLS;

  switch (DwPcie->MaxLinkSpeed) {
    case PCI_EXP_LNKCAP_SLS_2_5GB:
      LinkSpeed = PCI_EXP_LNKCTL2_TLS_2_5GT;
      break;
    case PCI_EXP_LNKCAP_SLS_5_0GB:
      LinkSpeed = PCI_EXP_LNKCTL2_TLS_5_0GT;
      break;
    case PCI_EXP_LNKCAP_SLS_8_0GB:
      LinkSpeed = PCI_EXP_LNKCTL2_TLS_8_0GT;
      break;
    case PCI_EXP_LNKCAP_SLS_16_0GB:
      LinkSpeed = PCI_EXP_LNKCTL2_TLS_16_0GT;
      break;
    default:
      // Use hardware capability
      LinkSpeed = (Cap & PCI_EXP_LNKCAP_SLS) >> 0;
      Ctrl2 &= ~PCI_EXP_LNKCTL2_HASD;
      break;
  }

  DwPcieWriteDbi32 (DwPcie, Offset + PCI_EXP_LNKCTL2, Ctrl2 | LinkSpeed);

  Cap &= ~((UINT32) PCI_EXP_LNKCAP_SLS);
  DwPcieWriteDbi32 (DwPcie, Offset + PCI_EXP_LNKCAP, Cap | LinkSpeed);
}

STATIC
VOID
DwPcieLinkSetMaxLinkWidth (
  IN CONST  DW_PCIE   *DwPcie,
  IN        UINT32    NumLanes
  )
{
  UINT32  Plc;
  UINT32  Lwsc;
  UINT32  LnkCap;
  UINT8   Cap;

  if (NumLanes == 0) {
    return;
  }

  // Set the number of lanes
  Plc = DwPcieReadDbi32 (DwPcie, DW_PCIE_PORT_LINK_CONTROL);
  Plc &= ~DW_PCIE_PORT_LINK_FAST_LINK_MODE;
  Plc &= ~DW_PCIE_PORT_LINK_MODE_MASK;

  // Set link width speed control register
  Lwsc = DwPcieReadDbi32 (DwPcie, DW_PCIE_LINK_WIDTH_SPEED_CONTROL);
  Lwsc &= ~DW_PCIE_PORT_LOGIC_LINK_WIDTH_MASK;
  Lwsc |= DW_PCIE_PORT_LOGIC_LINK_WIDTH_1_LANES;
  switch (NumLanes) {
    case 1:
      Plc |= DW_PCIE_PORT_LINK_MODE_1_LANES;
      break;
    case 2:
      Plc |= DW_PCIE_PORT_LINK_MODE_2_LANES;
      break;
    case 4:
      Plc |= DW_PCIE_PORT_LINK_MODE_4_LANES;
      break;
    case 8:
      Plc |= DW_PCIE_PORT_LINK_MODE_8_LANES;
      break;
    case 16:
      Plc |= DW_PCIE_PORT_LINK_MODE_16_LANES;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: num-lanes %u: invalid value\n",
              __func__, DwPcie->Id, NumLanes));
      return;
  }
  DwPcieWriteDbi32 (DwPcie, DW_PCIE_PORT_LINK_CONTROL, Plc);
  DwPcieWriteDbi32 (DwPcie, DW_PCIE_LINK_WIDTH_SPEED_CONTROL, Lwsc);

  Cap = DwPcieFindCapability (DwPcie, PCI_CAP_ID_EXP);
  LnkCap = DwPcieReadDbi32 (DwPcie, Cap + PCI_EXP_LNKCAP);
  LnkCap &= ~PCI_EXP_LNKCAP_MLW;
  LnkCap |= (NumLanes & 0x3f) << 4;
  DwPcieWriteDbi32 (DwPcie, Cap + PCI_EXP_LNKCAP, LnkCap);
}

STATIC
VOID
DwPcieSetup (
  IN OUT  DW_PCIE   *DwPcie
  )
{
  UINT32 Value;

  DwPcieLinkSetMaxSpeed (DwPcie);

  // Configure Gen1 N_FTS
  if (DwPcie->NFts[0]) {
    Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_PORT_AFR);
    Value &= ~(DW_PCIE_PORT_AFR_N_FTS_MASK |
               DW_PCIE_PORT_AFR_CC_N_FTS_MASK);
    Value |= DW_PCIE_PORT_AFR_N_FTS (DwPcie->NFts[0]);
    Value |= DW_PCIE_PORT_AFR_CC_N_FTS (DwPcie->NFts[0]);
    DwPcieWriteDbi32 (DwPcie, DW_PCIE_PORT_AFR, Value);
  }

  // Configure Gen2+ N_FTS
  if (DwPcie->NFts[1]) {
    Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_LINK_WIDTH_SPEED_CONTROL);
    Value &= ~DW_PCIE_PORT_LOGIC_N_FTS_MASK;
    Value |= DwPcie->NFts[1];
    DwPcieWriteDbi32 (DwPcie, DW_PCIE_LINK_WIDTH_SPEED_CONTROL, Value);
  }

  if (DW_PCIE_CAP_IS (DwPcie, DW_PCIE_CAP_CDM_CHECK)) {
    Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_PL_CHK_REG_CONTROL_STATUS);
    Value |= DW_PCIE_PL_CHK_REG_CHK_REG_CONTINUOUS |
             DW_PCIE_PL_CHK_REG_CHK_REG_START;
    DwPcieWriteDbi32 (DwPcie, DW_PCIE_PL_CHK_REG_CONTROL_STATUS, Value);
  }

  Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_PORT_LINK_CONTROL);
  Value &= ~DW_PCIE_PORT_LINK_FAST_LINK_MODE;
  Value |= DW_PCIE_PORT_LINK_DLL_LINK_EN;
  DwPcieWriteDbi32 (DwPcie, DW_PCIE_PORT_LINK_CONTROL, Value);

  DwPcieLinkSetMaxLinkWidth (DwPcie, DwPcie->NumLanes);
}

STATIC
inline
VOID
DwPcieDisableAtu (
  IN CONST  DW_PCIE     *DwPcie,
  IN        UINT32      Dir,
  IN        INTN        Index
  )
{
  DwPcieWriteAtu32 (DwPcie, Dir, Index, DW_PCIE_ATU_REGION_CTRL2, 0);
}

STATIC
VOID
DwPcieDisableAllObIatus (
  IN CONST  DW_PCIE     *DwPcie
  )
{
  INTN  Index;
  for (Index = 0; Index < DwPcie->NumObWindows; Index++) {
    DwPcieDisableAtu (DwPcie, DW_PCIE_ATU_REGION_DIR_OB, Index);
  }
}

STATIC
VOID
DwPcieDisableAllIbIatus (
  IN CONST  DW_PCIE     *DwPcie
  )
{
  INTN  Index;
  for (Index = 0; Index < DwPcie->NumIbWindows; Index++) {
    DwPcieDisableAtu (DwPcie, DW_PCIE_ATU_REGION_DIR_IB, Index);
  }
}

STATIC
inline
UINT32
DwPcieEnableEcrc (
  IN  UINT32   Value
  )
{
  //
  // DesignWare core version 4.90A has a design issue where the 'TD'
  // bit in the Control register-1 of the ATU outbound region acts
  // like an override for the ECRC setting, i.e., the presence of TLP
  // Digest (ECRC) in the outgoing TLPs is solely determined by this
  // bit. This is contrary to the PCIe spec which says that the
  // enablement of the ECRC is solely determined by the AER
  // registers.
  //
  // Because of this, even when the ECRC is enabled through AER
  // registers, the transactions going through ATU won't have TLP
  // Digest as there is no way the PCI core AER code could program
  // the TD bit which is specific to the DesignWare core.
  //
  // The best way to handle this scenario is to program the TD bit
  // always. It affects only the traffic from root port to downstream
  // devices.
  //
  // At this point,
  // When ECRC is enabled in AER registers, everything works normally
  // When ECRC is NOT enabled in AER registers, then,
  // on Root Port:- TLP Digest (DWord size) gets appended to each packet
  //                even through it is not required. Since downstream
  //                TLPs are mostly for configuration accesses and BAR
  //                accesses, they are not in critical path and won't
  //                have much negative effect on the performance.
  // on End Point:- TLP Digest is received for some/all the packets coming
  //                from the root port. TLP Digest is ignored because,
  //                as per the PCIe Spec r5.0 v1.0 section 2.2.3
  //                "TLP Digest Rules", when an endpoint receives TLP
  //                Digest when its ECRC check functionality is disabled
  //                in AER registers, received TLP Digest is just ignored.
  // Since there is no issue or error reported either side, best way to
  // handle the scenario is to program TD bit by default.
  //

  return Value | DW_PCIE_ATU_TD;
}

STATIC
EFI_STATUS
DwPcieSetupEcam (
  IN CONST  DW_PCIE   *DwPcie
  )
{
  UINT32  Value;

  if (DwPcie->Rp.ConfigSize < SIZE_256MB) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: ConfigSize 0x%lx too small, expected: >= 0x%lx\n",
            __func__, DwPcie->Id, DwPcie->Rp.ConfigSize, SIZE_256MB));
    return EFI_INVALID_PARAMETER;
  }

  DwPcieWriteDbi32 (DwPcie, DW_PCIE_ECAM_BASE_ADDR_LWR, LOWER_32_BITS (DwPcie->Rp.ConfigBase));
  DwPcieWriteDbi32 (DwPcie, DW_PCIE_ECAM_BASE_ADDR_UPPER, UPPER_32_BITS (DwPcie->Rp.ConfigBase));

  Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_ECAM_CTRL);
  Value |= DW_PCIE_ECAM_CTRL_DSP_ECAM_EN;
  DwPcieWriteDbi32 (DwPcie, DW_PCIE_ECAM_CTRL, Value);

  return EFI_SUCCESS;
}

/**
  Configure ATU for outbound accesses.

  @param  DwPcie        Pointer to DW_PCIE instance.
  @param  AtuCfg        Pointer to ATU config.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieProgOutboundAtu (
  IN CONST  DW_PCIE             *DwPcie,
  IN CONST  DW_PCIE_OB_ATU_CFG  *AtuCfg
  )
{
  UINT64  LimitAddr;
  UINT32  Value;
  UINT32 Retries;

  DEBUG ((DEBUG_VERBOSE, "DwPcie %u: Outbound ATU programmed with: "
          "Index: %d, Type: %d, CPU Addr: 0x%lx, PCI Addr: 0x%lx, Size: 0x%lx\n",
          DwPcie->Id, AtuCfg->Index, AtuCfg->Type, AtuCfg->CpuAddr, AtuCfg->PciAddr, AtuCfg->Size));

  LimitAddr = AtuCfg->CpuAddr + AtuCfg->Size - 1;
  if ((LimitAddr & ~DwPcie->RegionLimit) != (AtuCfg->CpuAddr & ~DwPcie->RegionLimit) ||
      !IS_ALIGNED (AtuCfg->CpuAddr, DwPcie->RegionAlign) ||
      !IS_ALIGNED (AtuCfg->PciAddr, DwPcie->RegionAlign) ||
      AtuCfg->Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_LOWER_BASE, LOWER_32_BITS (AtuCfg->CpuAddr));
  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_UPPER_BASE, UPPER_32_BITS (AtuCfg->CpuAddr));

  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_LIMIT, LOWER_32_BITS (LimitAddr));
  if (DW_PCIE_VER_IS_GE (DwPcie, DW_PCIE_VER_460A)) {
    DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_UPPER_LIMIT, UPPER_32_BITS (LimitAddr));
  }

  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_LOWER_TARGET, LOWER_32_BITS (AtuCfg->PciAddr));
  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_UPPER_TARGET, UPPER_32_BITS (AtuCfg->PciAddr));

  Value = AtuCfg->Type | AtuCfg->Routing | DW_PCIE_ATU_FUNC_NUM (AtuCfg->FuncNo);
  if (UPPER_32_BITS (LimitAddr) > UPPER_32_BITS (AtuCfg->CpuAddr) &&
      DW_PCIE_VER_IS_GE (DwPcie, DW_PCIE_VER_460A)) {
    Value |= DW_PCIE_ATU_INCREASE_REGION_SIZE;
  }
  if (DW_PCIE_VER_IS (DwPcie, DW_PCIE_VER_490A)) {
    Value = DwPcieEnableEcrc (Value);
  }
  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_REGION_CTRL1, Value);

  Value = DW_PCIE_ATU_ENABLE | AtuCfg->Ctrl2;
  if (AtuCfg->Type == DW_PCIE_ATU_TYPE_MSG) {
    // The data-less messages only for now
    Value |= DW_PCIE_ATU_INHIBIT_PAYLOAD | AtuCfg->Code;
  }
  DwPcieWriteAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_REGION_CTRL2, Value);

  //
  // Make sure ATU enable takes effect before any subsequent config
  // and I/O accesses.
  //
  for (Retries = 0; Retries < DW_PCIE_LINK_WAIT_MAX_IATU_RETRIES; Retries++) {
    Value = DwPcieReadAtuOb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_REGION_CTRL2);
    if (Value & DW_PCIE_ATU_ENABLE) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (DW_PCIE_LINK_WAIT_SLEEP_MS * 1000);
  }

  DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Outbound iATU is not being enabled\n",
          __func__, DwPcie->Id));

  return EFI_TIMEOUT;
}

/**
  Configure ATU for inbound accesses.

  @param  DwPcie        Pointer to DW_PCIE instance.
  @param  AtuCfg        Pointer to ATU config.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieProgInboundAtu (
  IN CONST  DW_PCIE             *DwPcie,
  IN CONST  DW_PCIE_IB_ATU_CFG  *AtuCfg
  )
{
  UINT64  LimitAddr;
  UINT32  Value;
  UINT32  Retries;

  DEBUG ((DEBUG_VERBOSE, "DwPcie %u: Inbound ATU programmed with: "
          "Index: %d, Type: %d, CPU Addr: 0x%lx, PCI Addr: 0x%lx, Size: 0x%lx\n",
          DwPcie->Id, AtuCfg->Index, AtuCfg->Type, AtuCfg->CpuAddr, AtuCfg->PciAddr, AtuCfg->Size));

  LimitAddr = AtuCfg->PciAddr + AtuCfg->Size - 1;
  if ((LimitAddr & ~DwPcie->RegionLimit) != (AtuCfg->PciAddr & ~DwPcie->RegionLimit) ||
      !IS_ALIGNED (AtuCfg->CpuAddr, DwPcie->RegionAlign) ||
      !IS_ALIGNED (AtuCfg->PciAddr, DwPcie->RegionAlign) ||
      AtuCfg->Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_LOWER_BASE, LOWER_32_BITS (AtuCfg->PciAddr));
  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_UPPER_BASE, UPPER_32_BITS (AtuCfg->PciAddr));

  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_LIMIT, LOWER_32_BITS (LimitAddr));
  if (DW_PCIE_VER_IS_GE (DwPcie, DW_PCIE_VER_460A)) {
    DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_UPPER_LIMIT, UPPER_32_BITS (LimitAddr));
  }

  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_LOWER_TARGET, LOWER_32_BITS (AtuCfg->CpuAddr));
  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_UPPER_TARGET, UPPER_32_BITS (AtuCfg->CpuAddr));

  Value = AtuCfg->Type;
  if (UPPER_32_BITS (LimitAddr) > UPPER_32_BITS (AtuCfg->PciAddr) &&
      DW_PCIE_VER_IS_GE (DwPcie, DW_PCIE_VER_460A)) {
    Value |= DW_PCIE_ATU_INCREASE_REGION_SIZE;
  }
  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_REGION_CTRL1, Value);

  Value = DW_PCIE_ATU_ENABLE | AtuCfg->Ctrl2;
  DwPcieWriteAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_REGION_CTRL2, Value);

  //
  // Make sure ATU enable takes effect before any subsequent config
  // and I/O accesses.
  //
  for (Retries = 0; Retries < DW_PCIE_LINK_WAIT_MAX_IATU_RETRIES; Retries++) {
    Value = DwPcieReadAtuIb32 (DwPcie, AtuCfg->Index, DW_PCIE_ATU_REGION_CTRL2);
    if (Value & DW_PCIE_ATU_ENABLE) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (DW_PCIE_LINK_WAIT_SLEEP_MS * 1000);
  }

  DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Inbound iATU is not being enabled\n",
          __func__, DwPcie->Id));

  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
DwPcieSetupObIatuForCfgSpace (
  IN CONST  DW_PCIE   *DwPcie
  )
{
  UINT32                NumBuses;
  DW_PCIE_OB_ATU_CFG    AtuCfg;
  EFI_STATUS            Status;

  //
  // If CFG Shift Mode disabled, the outbound ATU for CFG IO will be configured
  // before each IO read/write. We needn't configure it here.
  //
  if (!DwPcie->CfgShiftModeEnabled) {
    return EFI_SUCCESS;
  }

  NumBuses = DwPcie->Rp.BusMax - DwPcie->Rp.BusMin + 1;

  // Only root bus exists.
  if (NumBuses == 1) {
    //
    // Root bus under the root port doesn't require any iATU configuration
    // as DBI space will represent Root bus configuration space.
    //
    return EFI_SUCCESS;
  }

  if (DwPcie->Rp.ConfigSize < SIZE_1MB * NumBuses) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: ConfigSize 0x%lx too small, expected: >= 0x%lx\n",
            __func__, DwPcie->Id, DwPcie->Rp.ConfigSize, SIZE_1MB * NumBuses));
    return EFI_INVALID_PARAMETER;
  }

  // Immediate bus under root bus needs type 0 iATU configuration.
  ZeroMem (&AtuCfg, sizeof (AtuCfg));
  AtuCfg.Index = 0;
  AtuCfg.Type = DW_PCIE_ATU_TYPE_CFG0;
  AtuCfg.CpuAddr = DwPcie->Rp.ConfigBase + SIZE_1MB;
  AtuCfg.Size = SIZE_1MB;
  AtuCfg.Ctrl2 |= DW_PCIE_ATU_CFG_SHIFT_MODE_ENABLE;
  Status = DwPcieProgOutboundAtu (DwPcie, &AtuCfg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to setup outbound iATU for the"
            "config space of immediate bus under root bus\n", __func__, DwPcie->Id));
    return Status;
  }

  // The remaining buses need type 1 iATU configuration.
  if (NumBuses > 2) {
    ZeroMem (&AtuCfg, sizeof (AtuCfg));
    AtuCfg.Index = 1;
    AtuCfg.Type = DW_PCIE_ATU_TYPE_CFG1;
    AtuCfg.CpuAddr = DwPcie->Rp.ConfigBase + SIZE_2MB;
    AtuCfg.Size = SIZE_1MB * (NumBuses - 2);
    AtuCfg.Ctrl2 |= DW_PCIE_ATU_CFG_SHIFT_MODE_ENABLE;
    Status = DwPcieProgOutboundAtu (DwPcie, &AtuCfg);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to setup outbound iATU for the"
              "config space of remaining buses\n", __func__, DwPcie->Id));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DwPcieSetupObIatuForIoOrMemSpace (
  IN CONST  DW_PCIE   *DwPcie
  )
{
  INTN                  Index;
  INTN                  FirstAvailObWindowIndex;
  LIST_ENTRY            *Entry;
  DW_PCIE_RESOURCE      *Resource;
  DW_PCIE_OB_ATU_CFG    AtuCfg;
  EFI_STATUS            Status;

  //
  // Determine the first available outbound ATU index can be used for IO/MEM space.
  //
  if (DwPcie->EcamEnabled) {
    // If ECAM is enabled, no outbound ATU is used for CFG space. All of them
    // can be used for IO/MEM space.
    FirstAvailObWindowIndex = 0;
  } else if (DwPcie->CfgShiftModeEnabled) {
    // If ECAM is disabled and CFG Shift Mode is enabled, the first two outbound
    // ATUs have been used for CFG space.
    FirstAvailObWindowIndex = 2;
  } else {
    // Otherwise, the first one outbound ATU will be used for CFG space.
    FirstAvailObWindowIndex = 1;
  }

  if (DwPcie->NumObWindows < FirstAvailObWindowIndex) {
    DEBUG ((DEBUG_ERROR,
            "(%a) DwPcie %u: No enough outbound iATU windows found (NumObWindows: %u, expected: >=%d)\n",
            __func__,
            DwPcie->Id,
            DwPcie->NumObWindows,
            FirstAvailObWindowIndex));
    return EFI_INVALID_PARAMETER;
  }

  Index = FirstAvailObWindowIndex - 1;
  LIST_FOR_EACH_ENTRY (Entry, &DwPcie->Rp.Resources) {
    if (DwPcie->NumObWindows <= ++Index) {
      DEBUG ((DEBUG_WARN,
              "DwPcie %u: The number of outbound resources exceed the number of outbound iATU windows\n",
              DwPcie->Id));
      break;
    }

    Resource = LIST_ENTRY_TO_DW_PCIE_RESOURCE (Entry);

    // Ensure all of members in AtuCfg is initialized with zeros.
    ZeroMem (&AtuCfg, sizeof (AtuCfg));

    AtuCfg.Index = Index;
    if (DW_PCIE_RESOURCE_TYPE_IS_MEM (Resource->Flags)) {
      AtuCfg.Type = DW_PCIE_ATU_TYPE_MEM;
    } else if (DW_PCIE_RESOURCE_TYPE_IS_IO (Resource->Flags)) {
      AtuCfg.Type = DW_PCIE_ATU_TYPE_IO;
    } else {
      continue;
    }
    AtuCfg.CpuAddr = Resource->CpuBase;
    AtuCfg.PciAddr = Resource->PciBase;
    AtuCfg.Size = Resource->Size;

    Status = DwPcieProgOutboundAtu (DwPcie, &AtuCfg);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to setup outbound iATU for space "
              "[CpuAddr: 0x%lx, PciAddr: 0x%lx, Size: 0x%lx]\n",
              __func__, DwPcie->Id, AtuCfg.CpuAddr, AtuCfg.PciAddr, AtuCfg.Size));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DwPcieSetupIbIatuForMemSpace (
  IN CONST  DW_PCIE   *DwPcie
  )
{
  INTN                  Index;
  LIST_ENTRY            *Entry;
  DW_PCIE_RESOURCE      *IbResource;
  DW_PCIE_IB_ATU_CFG    AtuCfg;
  EFI_STATUS            Status;

  Index = 0;
  LIST_FOR_EACH_ENTRY (Entry, &DwPcie->Rp.IbResources) {
    if (DwPcie->NumIbWindows <= Index) {
      DEBUG ((DEBUG_WARN,
              "DwPcie %u: The number of inbound resources exceed the number of inbound iATU windows\n",
              DwPcie->Id));
      break;
    }

    IbResource = LIST_ENTRY_TO_DW_PCIE_RESOURCE (Entry);

    if (!DW_PCIE_RESOURCE_TYPE_IS_MEM (IbResource->Flags)) {
      continue;
    }

    //
    // If CpuBase == PciBase, not need to setup inbound iATU.
    //
    if (IbResource->CpuBase == IbResource->PciBase) {
      continue;
    }

    // Ensure all of members in AtuCfg is initialized with zeros.
    ZeroMem (&AtuCfg, sizeof (AtuCfg));

    AtuCfg.Index = Index;
    AtuCfg.Type = DW_PCIE_ATU_TYPE_MEM;
    AtuCfg.CpuAddr = IbResource->CpuBase;
    AtuCfg.PciAddr = IbResource->PciBase;
    AtuCfg.Size = IbResource->Size;

    Status = DwPcieProgInboundAtu (DwPcie, &AtuCfg);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to setup inbound iATU for space "
              "[CpuAddr: 0x%lx, PciAddr: 0x%lx, Size: 0x%lx]\n",
              __func__, DwPcie->Id, AtuCfg.CpuAddr, AtuCfg.PciAddr, AtuCfg.Size));
      return Status;
    }

    Index++;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DwPcieSetupRc (
  IN OUT  DW_PCIE   *DwPcie
  )
{
  UINT32        Value;
  EFI_STATUS    Status;

  //
  // Enable DBI read-only registers for writing/updating configuration.
  // Write permission gets disabled towards the end of this function.
  //
  DwPcieDbiRoWrEnable (DwPcie);

  DwPcieSetup (DwPcie);

  // Setup RC BARs
  DwPcieWriteDbi32 (DwPcie, PCI_BASE_ADDRESS_0, 0x00000004);
  DwPcieWriteDbi32 (DwPcie, PCI_BASE_ADDRESS_1, 0x00000000);

  // Setup interrupt pins
  Value = DwPcieReadDbi32 (DwPcie, PCI_INTERRUPT_LINE);
  Value &= 0xffff00ff;
  Value |= 0x00000100;
  DwPcieWriteDbi32 (DwPcie, PCI_INTERRUPT_LINE, Value);

  // Setup bus numbers
  Value = DwPcieReadDbi32 (DwPcie, PCI_PRIMARY_BUS);
  Value &= 0xff000000;
  Value |= 0x00ff0100;
  DwPcieWriteDbi32 (DwPcie, PCI_PRIMARY_BUS, Value);

  // Setup command register
  Value = DwPcieReadDbi32 (DwPcie, PCI_COMMAND);
  Value &= 0xffff0000;
  Value |= PCI_COMMAND_IO |
           PCI_COMMAND_MEMORY |
           PCI_COMMAND_MASTER |
           PCI_COMMAND_SERR;
  DwPcieWriteDbi32 (DwPcie, PCI_COMMAND, Value);

  //
  // Ensure all outbound/inbound windows are disabled before proceeding with
  // CFG/MEM/IO ranges/dma-ranges setups.
  //
  DwPcieDisableAllObIatus (DwPcie);
  DwPcieDisableAllIbIatus (DwPcie);

  // Setup CFG space
  if (DwPcie->EcamEnabled) {
    // ECAM is enabled, not need to setup outbound ATU for CFG space.
    Status = DwPcieSetupEcam (DwPcie);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to setup ECAM\n",
               __func__, DwPcie->Id));
      return Status;
    }
  } else {
    // ECAM is disabled, setup outbound ATU for CFG space.
    Status = DwPcieSetupObIatuForCfgSpace (DwPcie);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR,
              "(%a) DwPcie %u: Failed to setup outbound iATU for config space\n",
              __func__,
              DwPcie->Id));
      return Status;
    }
  }

  // No matter ECAM is enabled or not, outbound ATU setups for IO/MEM spaces are needed.
  Status = DwPcieSetupObIatuForIoOrMemSpace (DwPcie);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
            "(%a) DwPcie %u: Failed to setup outbound iATU for IO or Mem space\n",
            __func__,
            DwPcie->Id));
    return Status;
  }

  // Setup inbound ATU for MEM spaces.
  Status = DwPcieSetupIbIatuForMemSpace (DwPcie);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
            "(%a) DwPcie %u: Failed to setup inbound iATU for Mem space\n",
            __func__,
            DwPcie->Id));
    return Status;
  }

  DwPcieWriteDbi32 (DwPcie, PCI_BASE_ADDRESS_0, 0);

  // Program correct class for RC
  DwPcieWriteDbi16 (DwPcie, PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

  Value = DwPcieReadDbi32 (DwPcie, DW_PCIE_LINK_WIDTH_SPEED_CONTROL);
  Value |= DW_PCIE_PORT_LOGIC_SPEED_CHANGE;
  DwPcieWriteDbi32 (DwPcie, DW_PCIE_LINK_WIDTH_SPEED_CONTROL, Value);

  // Disable write permission to DBI read-only registers.
  DwPcieDbiRoWrDisable (DwPcie);

  return EFI_SUCCESS;
}

/**
  Get configurations, such as register address, for DW_PCIE instance.

  @param  DwPcie                    Pointer to DW_PCIE instance.
  @param  DwPcieControllerConfig    Pointer to DW PCIe controller config.
  @param  RootBridgeResourceConfig  Pointer to Root Bridge resource config.
  @param  DmaMappings               Pointer to DMA mappings config. It can be
                                    NULL if we don't need to setup inbound
                                    resources.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieGetConfigs (
  IN OUT    DW_PCIE                                 *DwPcie,
  IN CONST  DW_PCIE_CONTROLLER_CONFIG_DATA          *DwPcieControllerConfig,
  IN CONST  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_DATA    *RootBridgeResourceConfig,
  IN CONST  DMA_IOMMU_MAPPINGS                      *DmaMappings                OPTIONAL
  )
{
  EFI_STATUS  Status;

  Status = DwPcieGetDwPcieControllerConfig (DwPcie, DwPcieControllerConfig);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to get DW PCIe controller config\n",
            __func__, DwPcie->Id));
    return Status;
  }

  Status = DwPcieGetRootBridgeResourceConfig (DwPcie, RootBridgeResourceConfig);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to get Root Bridge resource config\n",
            __func__, DwPcie->Id));
    return Status;
  }

  if (DmaMappings != NULL) {
    Status = DwPcieGetDmaMappingsConfig (DwPcie, DmaMappings);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to get DMA mappings config\n",
              __func__, DwPcie->Id));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Init DesignWare PCIe controller (host mode)

  @param  DwPcie                    Pointer to DW_PCIE instance.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieHostInit (
  IN OUT  DW_PCIE     *DwPcie
  )
{
  EFI_STATUS  Status;

  DwPcieVersionDetect (DwPcie);

  DwPcieIatuDetect (DwPcie);

  Status = DwPcieSetupRc (DwPcie);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "(%a) DwPcie %u: Failed to setup RC\n", __func__, DwPcie->Id));
    return Status;
  }

  DwPciePrintInformations (DwPcie);

  return EFI_SUCCESS;
}

/**
  Init the array gDwPcieRootPorts.

  All of the elements in gDwPcieRootPorts are initialized with NULL. Then
  DwPcieRootPortArrayRegisterElement() should be called to register the elements.

  Remember to call DwPcieRootPortArrayDeinit() if the gDwPcieRootPorts is no
  more used.

  @param  SegmentNumberMax    The max segment number.

  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieRootPortArrayInit (
  IN UINTN  SegmentNumberMax
  )
{
  CONST DW_PCIE_ROOT_PORT **Rps = NULL;

  Rps = (CONST DW_PCIE_ROOT_PORT **) AllocateZeroPool (
        sizeof (DW_PCIE_ROOT_PORT *) * (SegmentNumberMax + 1));
  if (Rps == NULL) {
    return EFI_UNSUPPORTED;
  }

  gDwPcieRootPorts = Rps;

  return EFI_SUCCESS;
}

/**
  Deinit the array gDwPcieRootPorts.

  @param  VOID

  @retval VOID

**/
VOID
EFIAPI
DwPcieRootPortArrayDeinit (
  VOID
  )
{
  if (gDwPcieRootPorts == NULL) {
    return;
  }

  FreePool (gDwPcieRootPorts);
}

/**
  Register an element to gDwPcieRootPorts.

  @param  Rp            The pointer to a DW_PCIE_ROOT_PORT instance.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
DwPcieRootPortArrayRegisterElement (
  IN CONST  DW_PCIE_ROOT_PORT   *Rp
  )
{
  if (gDwPcieRootPorts == NULL) {
    DEBUG ((DEBUG_ERROR, "(%a) gDwPcieRootPorts array not inited\n", __func__));
    return EFI_UNSUPPORTED;
  }

  if (gDwPcieRootPorts[Rp->Segment] != NULL) {
    DEBUG ((DEBUG_ERROR, "(%a) Root port of Segment %u already registered\n",
            __func__, Rp->Segment));
    return EFI_UNSUPPORTED;
  }

  gDwPcieRootPorts[Rp->Segment] = Rp;

  return EFI_SUCCESS;
}
