/** @file
*
*  Copyright (c) 2025, Spacemit Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#ifndef __FDT_FIXUP_DXE_H__
#define __FDT_FIXUP_DXE_H__

#include <Uefi/UefiBaseType.h>

#define IS_DIGIT(Ch)                   (('0' <= (Ch)) && ((Ch) <= '9'))

///
/// Global ID for the fdt fixup Protocol
///
#define EFI_DT_FIXUP_PROTOCOL_GUID \
  { \
    0xe617d64c, 0xfe08, 0x46da, {0xf4, 0xdc, 0xbb, 0xd5, 0x87, 0x0c, 0x73, 0x00 } \
  }

#define EFI_DT_FIXUP_PROTOCOL_REVISION 0x00010000
/* Add nodes and update properties */
#define EFI_DT_APPLY_FIXUPS            0x00000001

/*
 * Reserve memory according to the /reserved-memory node
 * and the memory reservation block
 */
#define EFI_DT_RESERVE_MEMORY          0x00000002
/* Install the device-tree as configuration table */
#define EFI_DT_INSTALL_TABLE           0x00000004

#define EFI_DT_ALL (EFI_DT_APPLY_FIXUPS | \
			  EFI_DT_RESERVE_MEMORY     | \
			  EFI_DT_INSTALL_TABLE)

typedef struct _EFI_DT_FIXUP_PROTOCOL EFI_DT_FIXUP_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_FDT_FIXUP_PROTOTOCL_FIXUP) (
  IN  EFI_DT_FIXUP_PROTOCOL  *This,
  IN  VOID                   *Dtb,
  OUT UINTN                  *Buffer_size,
  IN  UINT32                 Flags
  );

struct _EFI_DT_FIXUP_PROTOCOL {
  UINT64                         Revision;
  EFI_FDT_FIXUP_PROTOTOCL_FIXUP  Fixup;
  };
#endif // __FDT_FIXUP_DXE_H__
