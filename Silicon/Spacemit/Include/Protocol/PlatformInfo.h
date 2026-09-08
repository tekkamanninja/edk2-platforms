/** @file
  Provide API to access platform info.

  Copyright (c) 2025 Spacemit Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __PLATFORM_INFO__
#define __PLATFORM_INFO__

#include <Uefi.h>

typedef struct _PLATFORM_INFO_PROTOCOL PLATFORM_INFO_PROTOCOL;

typedef struct {
  UINT64    PhysicalAddress;
  UINT64    PhysicalSize;
} MEMORY_LAYOUT_INFO;

typedef
EFI_STATUS
(EFIAPI *GET_PLATFORM_INFO) (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN  CHAR8                  *Name,
  OUT VOID                   *Info,
  IN  UINTN                  MaxSize
  );

typedef
EFI_STATUS
(EFIAPI *SET_PLATFORM_INFO) (
  IN PLATFORM_INFO_PROTOCOL  *This,
  IN  CHAR8                  *Name,
  OUT VOID                   *Info,
  IN  UINTN                  InfoSize
  );

 struct _PLATFORM_INFO_PROTOCOL {
  UINT64               Revision;
  GET_PLATFORM_INFO    GetPlatformInfo;
  SET_PLATFORM_INFO    SetPlatformInfo;
};

#endif // __PLATFORM_INFO__
