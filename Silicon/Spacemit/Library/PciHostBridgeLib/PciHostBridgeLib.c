/** @file
  PCI Host Bridge Library instance

  Copyright (C) 2016, Red Hat, Inc.
  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Library/PciRootBrdigeResourceConfig.h>
#include <Library/MemoryManagementLib.h>

#pragma pack(1)
typedef struct {
  ACPI_HID_DEVICE_PATH      AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;
#pragma pack ()

#define ACPI_DEVICE_PATH_DEF {{ ACPI_DEVICE_PATH, ACPI_DP, \
                                     { (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)), \
                                       (UINT8) (sizeof (ACPI_HID_DEVICE_PATH) >> 8)} \
                                     }, \
                                     EISA_PNP_ID (0x0A03), 0 \
                                  }

#define END_DEVICE_PATH_DEF { END_DEVICE_PATH_TYPE, \
                              END_ENTIRE_DEVICE_PATH_SUBTYPE, \
                              { END_DEVICE_PATH_LENGTH, 0 } \
                            }

STATIC CONST EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mEfiPciRootBridgeDevicePathTemplate[] = {
  {
    ACPI_DEVICE_PATH_DEF,
    END_DEVICE_PATH_DEF
  },
};

GLOBAL_REMOVE_IF_UNREFERENCED
STATIC CHAR16 *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

/**
  Print root bridges resources.

  @param[in] RootBridges  Pointer to root bridge resources.

**/
STATIC
VOID
PrintRootBridgeResources (
  IN PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY  *RootBridges
  )
{
  DEBUG_CODE_BEGIN ();
    UINTN Index;

    if (RootBridges == NULL) {
      return;
    }

    DEBUG ((DEBUG_INFO, "RootBridges->ArrayNum = %d\n", RootBridges->ArrayNum));

    for (Index = 0; Index < RootBridges->ArrayNum; Index++) {
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Segment = %d\n", Index, RootBridges->ArrayData[Index].Segment));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].ConfigBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].ConfigBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].ConfigSize = 0x%lX\n", Index, RootBridges->ArrayData[Index].ConfigSize));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].BusBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].BusBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].BusLimit = 0x%lX\n", Index, RootBridges->ArrayData[Index].BusLimit));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Io.PciBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].Io.PciBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Io.PciSize = 0x%lX\n", Index, RootBridges->ArrayData[Index].Io.PciSize));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Io.CpuBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].Io.CpuBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Mem.PciBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].Mem.PciBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Mem.PciSize = 0x%lX\n", Index, RootBridges->ArrayData[Index].Mem.PciSize));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Mem.CpuBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].Mem.CpuBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Mem64.PciBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].Mem64.PciBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Mem64.PciSize = 0x%lX\n", Index, RootBridges->ArrayData[Index].Mem64.PciSize));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].Mem64.CpuBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].Mem64.CpuBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].PMem.PciBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].PMem.PciBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].PMem.PciSize = 0x%lX\n", Index, RootBridges->ArrayData[Index].PMem.PciSize));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].PMem.CpuBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].PMem.CpuBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].PMem64.PciBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].PMem64.PciBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].PMem64.PciSize = 0x%lX\n", Index, RootBridges->ArrayData[Index].PMem64.PciSize));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].PMem64.CpuBase = 0x%lX\n", Index, RootBridges->ArrayData[Index].PMem64.CpuBase));
      DEBUG ((DEBUG_INFO, "RootBridges->ArrayData[%2d].IsEnabled = %u\n", Index, RootBridges->ArrayData[Index].IsEnabled));
    }
  DEBUG_CODE_END ();

  return;
}

/**
  Return all the root bridge instances in an array.

  @param[out] Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.

**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  OUT UINTN     *Count
  )
{
  UINTN                                   Index;
  UINTN                                   RootBridgeCount;
  INTN                                    BoardIndex;
  INTN                                    PreviousBoardIndex;
  PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY   *BoardRootBridges;
  PCI_ROOT_BRIDGE                         *RootBridges;
  EFI_PCI_ROOT_BRIDGE_DEVICE_PATH         *DevicePath;

  Index            = 0;
  BoardRootBridges = NULL;
  RootBridgeCount  = 0;
  RootBridges      = NULL;
  DevicePath       = NULL;

  if (Count == NULL) {
    return NULL;
  }

  *Count = 0;

  BoardRootBridges = (PCI_ROOT_BRIDGE_RESOURCE_CONFIG_ARRAY *) PcdGetPtr (PcdBoardPciRootBridgeResourceConfigTable);
  if (BoardRootBridges == NULL) {
    return NULL;
  }

  PrintRootBridgeResources (BoardRootBridges);

  RootBridgeCount = 0;
  for (BoardIndex = 0; BoardIndex < BoardRootBridges->ArrayNum; BoardIndex++) {
    if (BoardRootBridges->ArrayData[BoardIndex].IsEnabled) {
      RootBridgeCount++;
    }
  }
  DEBUG ((DEBUG_INFO, "%a() RootBridgeCount = %d.\n", __func__, RootBridgeCount));
  if (RootBridgeCount == 0) {
    return NULL;
  }

  RootBridges = (PCI_ROOT_BRIDGE *) AllocateZeroPool (RootBridgeCount * sizeof (*RootBridges));
  if (RootBridges == NULL) {
    return NULL;
  }

  PreviousBoardIndex = -1;
  for (Index = 0; Index < RootBridgeCount; Index++) {
    //
    // Find the next index of resource config whose 'IsEnabled' is TRUE in BoardRootBridges->ArrayData[].
    //
    for (BoardIndex = PreviousBoardIndex + 1; BoardIndex < BoardRootBridges->ArrayNum; BoardIndex++) {
      if (BoardRootBridges->ArrayData[BoardIndex].IsEnabled) {
        PreviousBoardIndex = BoardIndex;
        break;
      }
    }
    if (BoardIndex >= BoardRootBridges->ArrayNum) {
      break;
    }

    RootBridges[Index].Segment                   = BoardRootBridges->ArrayData[BoardIndex].Segment;
    RootBridges[Index].Supports                  = 0;
    RootBridges[Index].Attributes                = 0;
    RootBridges[Index].DmaAbove4G                = TRUE;
    RootBridges[Index].NoExtendedConfigSpace     = FALSE;
    RootBridges[Index].ResourceAssigned          = FALSE;

    if (!(BoardRootBridges->ArrayData[BoardIndex].PMem.PciSize || \
          BoardRootBridges->ArrayData[BoardIndex].PMem64.PciSize)) {
      RootBridges[Index].AllocationAttributes    |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
    }
    if (BoardRootBridges->ArrayData[BoardIndex].Mem64.PciSize || \
        BoardRootBridges->ArrayData[BoardIndex].PMem64.PciSize) {
      RootBridges[Index].AllocationAttributes    |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
    }

    RootBridges[Index].Bus.Base                  = BoardRootBridges->ArrayData[BoardIndex].BusBase;
    RootBridges[Index].Bus.Limit                 = BoardRootBridges->ArrayData[BoardIndex].BusLimit;

    if (BoardRootBridges->ArrayData[BoardIndex].Io.PciSize) {
      RootBridges[Index].Io.Base                 = BoardRootBridges->ArrayData[BoardIndex].Io.PciBase;
      RootBridges[Index].Io.Limit                = BoardRootBridges->ArrayData[BoardIndex].Io.PciBase + BoardRootBridges->ArrayData[BoardIndex].Io.PciSize - 1;
      RootBridges[Index].Io.Translation          = MAX_UINT64 - (BoardRootBridges->ArrayData[BoardIndex].Io.CpuBase - BoardRootBridges->ArrayData[BoardIndex].Io.PciBase) + 1;
    } else {
      RootBridges[Index].Io.Base                 = MAX_UINT64;
      RootBridges[Index].Io.Limit                = 0;
    }
    if (BoardRootBridges->ArrayData[BoardIndex].Mem.PciSize) {
      RootBridges[Index].Mem.Base                = BoardRootBridges->ArrayData[BoardIndex].Mem.PciBase;
      RootBridges[Index].Mem.Limit               = BoardRootBridges->ArrayData[BoardIndex].Mem.PciBase + BoardRootBridges->ArrayData[BoardIndex].Mem.PciSize - 1;
      RootBridges[Index].Mem.Translation         = MAX_UINT64 - (BoardRootBridges->ArrayData[BoardIndex].Mem.CpuBase - BoardRootBridges->ArrayData[BoardIndex].Mem.PciBase) + 1;
    } else {
      RootBridges[Index].Mem.Base                = MAX_UINT64;
      RootBridges[Index].Mem.Limit               = 0;
    }
    if (BoardRootBridges->ArrayData[BoardIndex].Mem64.PciSize) {
      RootBridges[Index].MemAbove4G.Base         = BoardRootBridges->ArrayData[BoardIndex].Mem64.PciBase;
      RootBridges[Index].MemAbove4G.Limit        = BoardRootBridges->ArrayData[BoardIndex].Mem64.PciBase + BoardRootBridges->ArrayData[BoardIndex].Mem64.PciSize - 1;
      RootBridges[Index].MemAbove4G.Translation  = MAX_UINT64 - (BoardRootBridges->ArrayData[BoardIndex].Mem64.CpuBase - BoardRootBridges->ArrayData[BoardIndex].Mem64.PciBase) + 1;
    } else {
      RootBridges[Index].MemAbove4G.Base         = MAX_UINT64;
      RootBridges[Index].MemAbove4G.Limit        = 0;
    }
    if (BoardRootBridges->ArrayData[BoardIndex].PMem.PciSize) {
      RootBridges[Index].PMem.Base               = BoardRootBridges->ArrayData[BoardIndex].PMem.PciBase;
      RootBridges[Index].PMem.Limit              = BoardRootBridges->ArrayData[BoardIndex].PMem.PciBase + BoardRootBridges->ArrayData[BoardIndex].PMem.PciSize - 1;
      RootBridges[Index].PMem.Translation        = MAX_UINT64 - (BoardRootBridges->ArrayData[BoardIndex].PMem.CpuBase - BoardRootBridges->ArrayData[BoardIndex].PMem.PciBase) + 1;
    } else {
      RootBridges[Index].PMem.Base               = MAX_UINT64;
      RootBridges[Index].PMem.Limit              = 0;
    }
    if (BoardRootBridges->ArrayData[BoardIndex].PMem64.PciSize) {
      RootBridges[Index].PMemAbove4G.Base        = BoardRootBridges->ArrayData[BoardIndex].PMem64.PciBase;
      RootBridges[Index].PMemAbove4G.Limit       = BoardRootBridges->ArrayData[BoardIndex].PMem64.PciBase + BoardRootBridges->ArrayData[BoardIndex].PMem64.PciSize - 1;
      RootBridges[Index].PMemAbove4G.Translation = MAX_UINT64 - (BoardRootBridges->ArrayData[BoardIndex].PMem64.CpuBase - BoardRootBridges->ArrayData[BoardIndex].PMem64.PciBase) + 1;
    } else {
      RootBridges[Index].PMemAbove4G.Base        = MAX_UINT64;
      RootBridges[Index].PMemAbove4G.Limit       = 0;
    }

    DevicePath = (EFI_PCI_ROOT_BRIDGE_DEVICE_PATH *) AllocateCopyPool (sizeof (mEfiPciRootBridgeDevicePathTemplate), mEfiPciRootBridgeDevicePathTemplate);
    if (DevicePath != NULL) {
      DevicePath->AcpiDevicePath.UID = RootBridges[Index].Segment;
      RootBridges[Index].DevicePath = (EFI_DEVICE_PATH_PROTOCOL *) DevicePath;
    }

    //
    // Map the ECAM space in the GCD memory map
    //
    MapRegToGcdMmioSpace (
                      BoardRootBridges->ArrayData[BoardIndex].ConfigBase,
                      BoardRootBridges->ArrayData[BoardIndex].ConfigSize
                      );

    if (BoardRootBridges->ArrayData[BoardIndex].Io.PciSize) {
      //
      // Map the MMIO window that provides I/O access - the PCI host bridge code
      // is not aware of this translation and so it will only map the I/O view
      // in the GCD I/O map.
      //
      MapRegToGcdMmioSpace (
                      RootBridges[Index].Io.Base - RootBridges[Index].Io.Translation,
                      RootBridges[Index].Io.Limit - RootBridges[Index].Io.Base + 1
                      );
    }
  }

  *Count = RootBridgeCount;

  return RootBridges;
}


/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param[in] Bridges The root bridge instances array.
  @param[in] Count   The count of the array.

**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  IN PCI_ROOT_BRIDGE    *Bridges,
  IN UINTN              Count
  )
{
  UINTN     Index;

  if (Bridges == NULL) {
    return;
  }

  for (Index = 0; Index < Count; Index++) {
    if (Bridges[Index].DevicePath != NULL) {
      FreePool ((VOID *) Bridges[Index].DevicePath);
    }
  }

  FreePool ((VOID *) Bridges);

  return;
}


/**
  Inform the platform that the resource conflict happens.

  @param[in] HostBridgeHandle Handle of the Host Bridge.
  @param[in] Configuration    Pointer to PCI I/O and PCI memory resource
                              descriptors. The Configuration contains the resources
                              for all the root bridges. The resource for each root
                              bridge is terminated with END descriptor and an
                              additional END is appended indicating the end of the
                              entire resources. The resource descriptor field
                              values follow the description in
                              EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                              SubmitResources().

**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  IN EFI_HANDLE         HostBridgeHandle,
  IN VOID               *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptor;
  BOOLEAN                           IsPrefetchable;

  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    for (; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (Descriptor->ResType <
              ARRAY_SIZE (mPciHostBridgeLibAcpiAddressSpaceTypeStr));
      DEBUG ((DEBUG_INFO, " %s: Length/Alignment = 0x%lx / 0x%lx\n",
              mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
              Descriptor->AddrLen,
              Descriptor->AddrRangeMax
              ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {

        IsPrefetchable = (Descriptor->SpecificFlag &
          EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0;

        DEBUG ((DEBUG_INFO, "     Granularity/SpecificFlag = %ld / %02x%s\n",
          Descriptor->AddrSpaceGranularity,
          Descriptor->SpecificFlag,
          (IsPrefetchable) ? L" (Prefetchable)" : L""
          ));
      }
    }
    //
    // Skip the end descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) (
                   (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                   );
  }

  return;
}
