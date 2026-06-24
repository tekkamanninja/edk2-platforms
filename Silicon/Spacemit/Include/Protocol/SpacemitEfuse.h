/** @file
  SpacemiT eFuse read protocol.

  Provides byte-level read access to the eFuse shadow registers.
  The driver reads the registers once at startup (after U-Boot has
  already loaded the physical fuse bits) and caches the data.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __SPACEMIT_EFUSE_PROTOCOL_H__
#define __SPACEMIT_EFUSE_PROTOCOL_H__

#define SPACEMIT_EFUSE_PROTOCOL_GUID \
  { 0xb3e7a2d1, 0x4f05, 0x4c8e, { 0xa9, 0x1c, 0x3d, 0x7f, 0x82, 0x56, 0xe0, 0xb4 } }

typedef struct _SPACEMIT_EFUSE_PROTOCOL SPACEMIT_EFUSE_PROTOCOL;

/**
  Read an eFuse field by name from the shadow register cache.

  Looks up Name in the driver's built-in field table, extracts the
  corresponding bits (LSB-first, matching the Linux nvmem convention),
  and writes ceil(BitSize/8) bytes to Buffer.

  @param[in]  This        Protocol instance pointer.
  @param[in]  Name        ASCII name of the eFuse field (e.g. "soc_die_id").
  @param[out] Buffer      Destination buffer for the extracted field value.
  @param[in]  BufferSize  Size of Buffer in bytes.

  @retval EFI_SUCCESS            Field found, bits extracted, Buffer written.
  @retval EFI_INVALID_PARAMETER  Name or Buffer is NULL.
  @retval EFI_NOT_FOUND          Name does not match any known field.
  @retval EFI_BAD_BUFFER_SIZE    Field bits extend past the cached bank size.
  @retval EFI_BUFFER_TOO_SMALL   BufferSize < ceil(field_bit_size / 8).
**/
typedef
EFI_STATUS
(EFIAPI *SPACEMIT_EFUSE_READ)(
  IN  SPACEMIT_EFUSE_PROTOCOL  *This,
  IN  CONST CHAR8              *Name,
  OUT VOID                     *Buffer,
  IN  UINTN                    BufferSize
  );

struct _SPACEMIT_EFUSE_PROTOCOL {
  SPACEMIT_EFUSE_READ    Read;
  UINT32                 TotalSize; ///< eFuse bank size in bytes
};

extern EFI_GUID  gSpacemitEfuseProtocolGuid;

#endif /* __SPACEMIT_EFUSE_PROTOCOL_H__ */
