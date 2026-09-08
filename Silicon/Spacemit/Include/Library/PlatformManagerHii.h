/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>
  Copyright (c) 2025, Spacemit Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_MANAGER_HII_GUID_H_
#define PLATFORM_MANAGER_HII_GUID_H_

#define PLATFORM_MANAGER_FORMSET_GUID  \
  { \
  0x2faa29ba, 0x0cf4, 0x4dcf, { 0x86, 0x69, 0x4a, 0x44, 0x53, 0x2a, 0x69, 0x4f } \
  }

#define PLATFORM_MANAGER_ENTRY_EVENT_GUID  \
  { \
  0x9ce8ecb7, 0x0e87, 0x4ae0, { 0x90, 0x66, 0xc1, 0x3d, 0xfa, 0xfb, 0x6d, 0x36 } \
  }

#define PLATFORM_MANAGER_EXIT_EVENT_GUID  \
  { \
  0x8dca33d6, 0x13cb, 0x4272, { 0x81, 0x67, 0xe9, 0x97, 0x97, 0x9f, 0xdd, 0x50 } \
  }

extern EFI_GUID  gPlatformManagerFormsetGuid;
extern EFI_GUID  gPlatformManagerEntryEventGuid;
extern EFI_GUID  gPlatformManagerExitEventGuid;

#endif
