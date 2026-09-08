/** @file
  Null Instance for Platform memory management library.

  Copyright (c) 2025, Spacemit Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

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
  return EFI_SUCCESS;
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
  return EFI_SUCCESS;
}
