/** @file
  Platform memory management library.

  Copyright (c) 2024, Spacemit Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi/UefiBaseType.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>

/**
  Internal function to set memory region specified by Base and Size aligned to EFI_PAGE_SIZE.

  @param[in]   Base               Base address of memory region.
  @param[in]   Size               Length of memory region.
  @param[out]  AlignedBase        Aligned base address of memory region.
  @param[out]  AlignedSize        Aligned length of memory region.

  @retval EFI_SUCCESS             Aligned successfully.
  @retval EFI_INVALID_PARAMETER   Parameter is invalid.

**/
STATIC
EFI_STATUS
MapRegAlignmentInternal (
  IN EFI_PHYSICAL_ADDRESS   Base,
  IN UINT64                 Size,
  OUT EFI_PHYSICAL_ADDRESS  *AlignedBase,
  OUT UINT64                *AlignedSize
  )
{
  if ((AlignedBase == NULL) ||
      (AlignedSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Memory space base and size may be not aligned to the page size,
  // which is not accepted when setting memory space attributes.
  //
  *AlignedBase = Base & ~(EFI_PAGE_SIZE - 1);
  *AlignedSize = ALIGN_VALUE (Size + (Base - *AlignedBase), EFI_PAGE_SIZE);

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: %a: Base = 0x%llX, Size = 0x%llX, AlignedBase = 0x%llX, AlignedSize = 0x%llX.\n",
    gEfiCallerBaseName,
    __func__,
    Base,
    Size,
    *AlignedBase,
    *AlignedSize
    ));

  return EFI_SUCCESS;
}

/**
  Internal function to map memory region to GCD memory map io space.

  @param[in]  Base                Base address of memory region.
  @param[in]  Size                Length of memory region.
  @param[in]  Attributes          Memory attributes of memory region.

  @retval EFI_SUCCESS             Map memory region to GCD memory map io space successfully.
  @retval EFI_NOT_AVAILABLE_YET   The attributes cannot be set because CPU architectural protocol is
                                  not available yet.
  @retval Others                  As the error code indicates.

**/
STATIC
EFI_STATUS
MapRegToGcdMmioSpaceInternal (
  IN EFI_PHYSICAL_ADDRESS   Base,
  IN UINT64                 Size,
  IN UINT64                 Attributes
  )
{
  EFI_STATUS                        Status;
  EFI_PHYSICAL_ADDRESS              AlignedBase;
  UINT64                            AlignedSize;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR   GcdMemoryDescriptor;
  EFI_PHYSICAL_ADDRESS              StartAddress, EndAddress;
  UINT64                            Length;

  Status = MapRegAlignmentInternal (Base, Size, &AlignedBase, &AlignedSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (StartAddress = AlignedBase, EndAddress = AlignedBase + AlignedSize, Length = 0;
       StartAddress < EndAddress;
       StartAddress += Length) {
    ZeroMem (&GcdMemoryDescriptor, sizeof (GcdMemoryDescriptor));
    Status = gDS->GetMemorySpaceDescriptor (StartAddress, &GcdMemoryDescriptor);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_VERBOSE,
        "%a: %a: failed to get GCD memory space descriptor for region [0x%llX].\n",
        gEfiCallerBaseName,
        __func__,
        StartAddress
        ));
      return Status;
    }

    Length = MIN (GcdMemoryDescriptor.BaseAddress + GcdMemoryDescriptor.Length, EndAddress) - StartAddress;

    if (GcdMemoryDescriptor.GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo) {
      if ((GcdMemoryDescriptor.Attributes & Attributes) == Attributes) {
        DEBUG ((
          DEBUG_VERBOSE,
          "%a: %a: GCD memory space descriptor for region [0x%llX+0x%llX] had been registered.\n",
          gEfiCallerBaseName,
          __func__,
          StartAddress,
          Length
          ));
        continue;
      }

      if ((GcdMemoryDescriptor.Capabilities & Attributes) != Attributes) {
        Status = gDS->SetMemorySpaceCapabilities (StartAddress, Length, GcdMemoryDescriptor.Capabilities | Attributes);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_VERBOSE,
            "%a: %a: failed to update memory space descriptor capabilities for region [0x%llX+0x%llX].\n",
            gEfiCallerBaseName,
            __func__,
            StartAddress,
            Length
            ));
          return Status;
        }
      }

      Status = gDS->SetMemorySpaceAttributes (StartAddress, Length, GcdMemoryDescriptor.Attributes | Attributes);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_VERBOSE,
          "%a: %a: failed to update memory space descriptor attributes for region [0x%llX+0x%llX].\n",
          gEfiCallerBaseName,
          __func__,
          StartAddress,
          Length
          ));
        return Status;
      }

      continue;
    }

    if (GcdMemoryDescriptor.GcdMemoryType != EfiGcdMemoryTypeNonExistent) {
      Status = gDS->FreeMemorySpace (StartAddress, Length);
      if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
        DEBUG ((
          DEBUG_VERBOSE,
          "%a: %a: failed to free GCD memory space for region [0x%llX+0x%llX].\n",
          gEfiCallerBaseName,
          __func__,
          StartAddress,
          Length
          ));
        return Status;
      }

      Status = gDS->RemoveMemorySpace (StartAddress, Length);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_VERBOSE,
          "%a: %a: failed to remove GCD memory space descriptor for region [0x%llX+0x%llX].\n",
          gEfiCallerBaseName,
          __func__,
          StartAddress,
          Length
          ));
        return Status;
      }
    }

    Status = gDS->AddMemorySpace (EfiGcdMemoryTypeMemoryMappedIo, StartAddress, Length, Attributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_VERBOSE,
        "%a: %a: failed to add GCD memory space descriptor for region [0x%llX+0x%llX].\n",
        gEfiCallerBaseName,
        __func__,
        StartAddress,
        Length
        ));
      return Status;
    }

    Status = gDS->AllocateMemorySpace (
                            EfiGcdAllocateAddress,
                            EfiGcdMemoryTypeMemoryMappedIo,
                            0,
                            Length,
                            &StartAddress,
                            gImageHandle,
                            NULL
                            );
    if (EFI_ERROR (Status)) {
      gDS->RemoveMemorySpace (StartAddress, Length);
      DEBUG ((
        DEBUG_VERBOSE,
        "%a: %a: failed to allocate memory space for region [0x%llX+0x%llX].\n",
        gEfiCallerBaseName,
        __func__,
        StartAddress,
        Length
        ));
      return Status;
    }

    Status = gDS->SetMemorySpaceAttributes (StartAddress, Length, Attributes);
    if (EFI_ERROR (Status)) {
      gDS->FreeMemorySpace (StartAddress, Length);
      gDS->RemoveMemorySpace (StartAddress, Length);
      DEBUG ((
        DEBUG_VERBOSE,
        "%a: %a: failed to set memory space descriptor attributes for region [0x%llX+0x%llX].\n",
        gEfiCallerBaseName,
        __func__,
        StartAddress,
        Length
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Map memory region to GCD memory map io space.

  @param[in]  Base                Base address of memory region.
  @param[in]  Size                Length of memory region.

  @retval EFI_SUCCESS             Map memory region to GCD MMIO space successfully.
  @retval EFI_NOT_AVAILABLE_YET   The attributes cannot be set because CPU architectural protocol is
                                  not available yet.
  @retval Others                  As the error code indicates.

**/
EFI_STATUS
EFIAPI
MapRegToGcdMmioSpace (
  IN EFI_PHYSICAL_ADDRESS   Base,
  IN UINT64                 Size
  )
{
  return MapRegToGcdMmioSpaceInternal (Base, Size, EFI_MEMORY_UC);
}

/**
  Map memory region to GCD runtime memory map io space.

  @param[in]  Base                Base address of memory region.
  @param[in]  Size                Length of memory region.

  @retval EFI_SUCCESS             Map memory region to GCD MMIO space successfully.
  @retval EFI_NOT_AVAILABLE_YET   The attributes cannot be set because CPU architectural protocol is
                                  not available yet.
  @retval Others                  As the error code indicates.

**/
EFI_STATUS
EFIAPI
MapRegToGcdRunTimeMmioSpace (
  IN EFI_PHYSICAL_ADDRESS   Base,
  IN UINT64                 Size
  )
{
  return MapRegToGcdMmioSpaceInternal (Base, Size, EFI_MEMORY_UC | EFI_MEMORY_RUNTIME);
}

/**
  Empty constructor function that is required to resolve dependencies between
  libraries.

    ** DO NOT REMOVE **

  @param[in]  ImageHandle         The firmware allocated handle for the EFI image.
  @param[in]  SystemTable         A pointer to the EFI System Table.

  @retval EFI_SUCCESS             The constructor executed correctly.

**/
EFI_STATUS
EFIAPI
MemoryManagementLibConstructor (
  IN EFI_HANDLE             ImageHandle,
  IN EFI_SYSTEM_TABLE       *SystemTable
  )
{
  return EFI_SUCCESS;
}
