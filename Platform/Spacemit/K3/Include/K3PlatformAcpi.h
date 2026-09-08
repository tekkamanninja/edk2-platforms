/** @file

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __K3_PLATFORM_ACPI_H__
#define __K3_PLATFORM_ACPI_H__

#include <IndustryStandard/Acpi.h>

//
// ACPI table information used to initialize tables.
//
#define EFI_ACPI_OEM_ID           'S','P','M','T',' ',' '         // 6 bytes
#define EFI_ACPI_OEM_TABLE_ID     SIGNATURE_64 ('K','3',' ',' ',' ',' ',' ',' ') // 8 bytes
#define EFI_ACPI_OEM_REVISION     0x00000001                      // 4 bytes
#define EFI_ACPI_CREATOR_ID       SIGNATURE_32('S','P','M','T')   // 4 bytes
#define EFI_ACPI_CREATOR_REVISION 0x00000001                      // 4 bytes

// A macro to initialise the common header part of EFI ACPI tables as defined by
// EFI_ACPI_DESCRIPTION_HEADER structure.
#define ACPI_HEADER(Signature, Type, Revision)                    \
  {                                                               \
    Signature,                      /* UINT32  Signature */       \
    sizeof (Type),                  /* UINT32  Length */          \
    Revision,                       /* UINT8   Revision */        \
    0,                              /* UINT8   Checksum */        \
    { EFI_ACPI_OEM_ID },            /* UINT8   OemId[6] */        \
    EFI_ACPI_OEM_TABLE_ID,          /* UINT64  OemTableId */      \
    EFI_ACPI_OEM_REVISION,          /* UINT32  OemRevision */     \
    EFI_ACPI_CREATOR_ID,            /* UINT32  CreatorId */       \
    EFI_ACPI_CREATOR_REVISION       /* UINT32  CreatorRevision */ \
  }

#endif /* ifndef __K1_PLATFORM_ACPI_H__ */
