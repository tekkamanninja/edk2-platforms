/** @file
*
*  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
*  Copyright (c) 2015, Hisilicon Limited. All rights reserved.
*  Copyright (c) 2015, Linaro Limited. All rights reserved.
*  Copyright (c) 2024, Spacemit Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "FlashFvbDxe.h"

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.ReadBlocks
//
EFI_STATUS
EFIAPI
FlashBlockIoReadBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSizeInBytes,
  OUT VOID                   *Buffer
  )
{
  FV_FLASH_INSTANCE  *Instance;
  EFI_STATUS         Status;

  Instance = INSTANCE_FROM_BLKIO_THIS (This);

  DEBUG (
         (DEBUG_VARIABLE, "%a(MediaId=%d, Lba=%ld, BufferSize=0x%x bytes (%d kB), BufferPtr @ 0x%08x)\n",
          __func__, MediaId, Lba, BufferSizeInBytes, BufferSizeInBytes / 1024, Buffer)
         );

  if ( !This->Media->MediaPresent ) {
    Status = EFI_NO_MEDIA;
  } else if ( This->Media->MediaId != MediaId ) {
    Status = EFI_MEDIA_CHANGED;
  } else {
    Status = FlashReadBlocks (Instance, Lba, BufferSizeInBytes, Buffer);
  }

  return Status;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.WriteBlocks
//
EFI_STATUS
EFIAPI
FlashBlockIoWriteBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSizeInBytes,
  IN  VOID                   *Buffer
  )
{
  FV_FLASH_INSTANCE  *Instance;
  EFI_STATUS         Status;

  Instance = INSTANCE_FROM_BLKIO_THIS (This);

  DEBUG (
         (DEBUG_VARIABLE, "%a(MediaId=%d, Lba=%ld, BufferSize=0x%x bytes (%d kB), BufferPtr @ 0x%08x)\n",
          __func__, MediaId, Lba, BufferSizeInBytes, BufferSizeInBytes / 1024, Buffer)
         );

  if ( !This->Media->MediaPresent ) {
    Status = EFI_NO_MEDIA;
  } else if ( This->Media->MediaId != MediaId ) {
    Status = EFI_MEDIA_CHANGED;
  } else if ( This->Media->ReadOnly ) {
    Status = EFI_WRITE_PROTECTED;
  } else {
    Status = FlashWriteBlocks (Instance, Lba, BufferSizeInBytes, Buffer);
  }

  return Status;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.FlushBlocks
//
EFI_STATUS
EFIAPI
FlashBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  // No Flush required for the NOR Flash driver
  // because cache operations are not permitted.

  // Nothing to do so just return without error
  return EFI_SUCCESS;
}
