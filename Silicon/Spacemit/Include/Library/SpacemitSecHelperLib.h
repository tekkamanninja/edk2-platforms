/**
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __SPACEMIT_SEC_HELPER_LIB_H__
#define __SPACEMIT_SEC_HELPER_LIB_H__

/**
  Common implementation of Memory PEIM initialization.

  @param  DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS        The platform initialized successfully.
  @retval  Others          - As the error code indicates
**/
EFI_STATUS
EFIAPI
MemoryPeimInitializationCommon (
  IN  VOID  *DeviceTreeAddress
  );

/**
  Common implementation of CPU PEIM initialization.

  @param  DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS        The platform initialized successfully.
  @retval  Others          - As the error code indicates
**/
EFI_STATUS
EFIAPI
CpuPeimInitializationCommon (
  IN  VOID  *DeviceTreeAddress
  );

/**
  Common implementation of Platform PEIM initialization.

  @param DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS       The platform initialized successfully.
  @retval  Others         - As the error code indicates
**/
EFI_STATUS
EFIAPI
PlatformPeimInitializationCommon (
  IN  VOID  *DeviceTreeAddress
  );

/**
  Populate IO resources from FDT that not added to GCD by its
  driver in the DXE phase.

  @param  FdtBase       Fdt base address
  @param  Compatible    Compatible string
**/
VOID
EFIAPI
PopulateIoResources (
  VOID         *FdtBase,
  CONST CHAR8  *Compatible
  );

#endif /* ifndef __SPACEMIT_SEC_HELPER_LIB_H__ */
