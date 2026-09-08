/**
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __SPACEMIT_SEC_LIB_H__
#define __SPACEMIT_SEC_LIB_H__

/**
  Perform Memory PEIM initialization.

  @param  DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS        The platform initialized successfully.
  @retval  Others          - As the error code indicates
**/
EFI_STATUS
EFIAPI
MemoryPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  );

/**
  Perform CPU PEIM initialization.

  @param  DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS        The platform initialized successfully.
  @retval  Others          - As the error code indicates
**/
EFI_STATUS
EFIAPI
CpuPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  );

/**
  Perform Platform PEIM initialization.

  @param DeviceTreeAddress  Pointer to FDT.
  @return EFI_SUCCESS       The platform initialized successfully.
  @retval  Others         - As the error code indicates
**/
EFI_STATUS
EFIAPI
PlatformPeimInitialization (
  IN  VOID  *DeviceTreeAddress
  );

#endif /* ifndef __SPACEMIT_SEC_LIB_H__ */
