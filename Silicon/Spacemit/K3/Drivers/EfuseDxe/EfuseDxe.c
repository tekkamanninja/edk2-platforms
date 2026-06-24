/** @file
  SpacemiT K3 eFuse DXE driver.

  Reads eFuse bank shadow registers once at entry and exposes the cached
  data via SPACEMIT_EFUSE_PROTOCOL.

  Bootloader loads the physical fuse bits into the shadow registers before
  UEFI is entered; this driver only needs to enable the clock briefly to
  make the shadow registers accessible, then caches the data and powers
  the block back down.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/MemoryManagementLib.h>
#include <Protocol/SpacemitEfuse.h>

#define APMU_AES_CLK_RES_CTRL_OFFSET  0x068u
#define AES_CLK_GATE_BIT              BIT5   ///< 1 = clock enabled
#define AES_RST_BIT                   BIT4   ///< 0 = deasserted (running)

/* efuse manufactor parameter bank */
#define EFUSE_PARA_BANK_BASE  (FixedPcdGet64 (PcdSpacemitEfuseBankBase) + 0x190)

//
// eFuse field descriptor.
// BitOffset and BitSize are derived from the DTS nvmem-cells definitions:
//   reg = <byte_offset size_bytes>; bits = <bit_offset_within_reg bit_count>;
//   → BitOffset = byte_offset * 8 + bit_offset_within_reg
//
// All offsets are relative to the start of the bank7 cache (mEfuseCache[0]).
// Bit ordering follows the Linux nvmem convention: LSB of bit_offset is
// the LSB of the extracted value.
//
typedef struct {
  CONST CHAR8    *Name;
  UINT32         BitOffset; ///< first bit's position in mEfuseCache (0 = LSB of byte 0)
  UINT32         BitSize;   ///< number of bits to extract
} EFUSE_FIELD;

//
// K3 eFuse field table
//
// Field         reg             bits       BitOffset        BitSize
// --------------------------------------------------------------------------
// wafer_id    <0x11 3>         <3 16>     0x11*8+3 = 139      16
// svt_dro     <0x15 2>         <5  9>     0x15*8+5 = 173       9
// product_id  <0x16 2>         <6  9>     0x16*8+6 = 182       9
//
STATIC CONST EFUSE_FIELD  mEfuseFields[] = {
  { "wafer_id",   139, 16 },
  { "svt_dro",    173, 9  },
  { "product_id", 182, 9  }
};

STATIC UINT8  mEfuseCache[32];  ///< bank7 shadow register cache (32 bytes)

STATIC
EFI_STATUS
EFIAPI
EfuseRead (
  IN  SPACEMIT_EFUSE_PROTOCOL  *This,
  IN  CONST CHAR8              *Name,
  OUT VOID                     *Buffer,
  IN  UINTN                    BufferSize
  )
{
  CONST EFUSE_FIELD  *Field;
  UINTN              I, NeedBytes;
  UINT8              *Out;
  UINT32             DstBit, SrcBit;

  if ((Name == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Field = NULL;
  for (I = 0; I < ARRAY_SIZE (mEfuseFields); I++) {
    if (AsciiStrCmp (Name, mEfuseFields[I].Name) == 0) {
      Field = &mEfuseFields[I];
      break;
    }
  }

  if (Field == NULL) {
    return EFI_NOT_FOUND;
  }

  // Verify field does not exceed the cached bank
  if ((Field->BitOffset + Field->BitSize + 7) / 8 > This->TotalSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // Verify caller buffer is large enough
  NeedBytes = (Field->BitSize + 7) / 8;
  if (BufferSize < NeedBytes) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Extract bits from cache, LSB-first, into output buffer
  Out = (UINT8 *)Buffer;
  ZeroMem (Out, BufferSize);
  for (DstBit = 0; DstBit < Field->BitSize; DstBit++) {
    SrcBit = Field->BitOffset + DstBit;
    if (mEfuseCache[SrcBit / 8] & (UINT8)(1u << (SrcBit % 8))) {
      Out[DstBit / 8] |= (UINT8)(1u << (DstBit % 8));
    }
  }

  return EFI_SUCCESS;
}

STATIC
VOID
EfuseMmioRemap (
  VOID
  )
{
  MapRegToGcdMmioSpace (EFUSE_PARA_BANK_BASE, SIZE_4KB);
}

STATIC
UINT32
EfuseLoad (
  VOID
  )
{
  UINT32  I, Word;

  EfuseMmioRemap ();

  // efuse bank has 8 words maximun
  for (I = 0; I < sizeof (mEfuseCache) / sizeof (UINT32) && I < 8; I++) {
    Word = MmioRead32 (EFUSE_PARA_BANK_BASE + I * sizeof (UINT32));
    CopyMem (mEfuseCache + I * sizeof (UINT32), &Word, sizeof (UINT32));
  }

  return I * sizeof (UINT32);
}

STATIC SPACEMIT_EFUSE_PROTOCOL  mEfuseProtocol = {
  EfuseRead,
  0,  // TotalSize set in entry point
};

EFI_STATUS
EFIAPI
EfuseDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT32  BankSize;

  BankSize = EfuseLoad ();
  DEBUG ((DEBUG_INFO, "EfuseDxe: cached %u bytes from efuse\n", BankSize));

  mEfuseProtocol.TotalSize = BankSize;

  return gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gSpacemitEfuseProtocolGuid,
                &mEfuseProtocol,
                NULL
                );
}
