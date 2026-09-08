/** @file
  Main Header file for the MMC DXE driver

  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
  Copyright (c) 2025, Spacemit Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __MMC_H
#define __MMC_H

#include <Uefi.h>

#include <Protocol/DiskIo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/MmcHost.h>
#include <Protocol/DiskInfo.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

#include <IndustryStandard/Sd.h>
#include <IndustryStandard/Emmc.h>

#define MMC_TRACE(txt)  DEBUG((DEBUG_BLKIO, "MMC: " txt "\n"))

#define MMC_IOBLOCKS_READ   0
#define MMC_IOBLOCKS_WRITE  1

#define MMC_OCR_POWERUP  0x80000000

#define MMC_OCR_ACCESS_MASK    0x3          /* bit[30-29] */
#define MMC_OCR_ACCESS_BYTE    0x1          /* bit[29] */
#define MMC_OCR_ACCESS_SECTOR  0x2          /* bit[30] */

#define MMC_R0_READY_FOR_DATA  (1 << 8)

#define MMC_R0_CURRENTSTATE(Response)  ((Response[0] >> 9) & 0xF)

#define MMC_R0_STATE_IDLE   0
#define MMC_R0_STATE_READY  1
#define MMC_R0_STATE_IDENT  2
#define MMC_R0_STATE_STDBY  3
#define MMC_R0_STATE_TRAN   4
#define MMC_R0_STATE_DATA   5

#define EMMC_CMD6_ARG_ACCESS(x)   (((x) & 0x3) << 24)
#define EMMC_CMD6_ARG_INDEX(x)    (((x) & 0xFF) << 16)
#define EMMC_CMD6_ARG_VALUE(x)    (((x) & 0xFF) << 8)
#define EMMC_CMD6_ARG_CMD_SET(x)  (((x) & 0x7) << 0)

#define CMD8_SD_ARG              (0x0UL << 12 | BIT8 | 0xCEUL << 0)
#define CMD8_MMC_ARG             (0)

#define SWITCH_CMD_DATA_LENGTH   64
#define SD_HIGH_SPEED_SUPPORTED  0x20000
#define SD_DEFAULT_SPEED         25000000
#define SD_HIGH_SPEED            50000000
#define SWITCH_CMD_SUCCESS_MASK  0x0f000000

#define SD_CARD_CAPACITY  0x00000002

#define BUSWIDTH_4  4

typedef enum {
  UNKNOWN_CARD,
  MMC_CARD,              // MMC card
  MMC_CARD_HIGH,         // MMC Card with High capacity
  EMMC_CARD,             // eMMC 4.41 card
  SD_CARD,               // SD 1.1 card
  SD_CARD_2,             // SD 2.0 or above standard card
  SD_CARD_2_HIGH         // SD 2.0 or above high capacity card
} CARD_TYPE;

typedef struct {
  UINT32    Reserved0  :  7;   // 0
  UINT32    V170_V195  :  1;   // 1.70V - 1.95V
  UINT32    V200_V260  :  7;   // 2.00V - 2.60V
  UINT32    V270_V360  :  9;   // 2.70V - 3.60V
  UINT32    RESERVED_1 :  5;   // Reserved
  UINT32    AccessMode :  2;   // 00b (byte mode), 10b (sector mode)
  UINT32    PowerUp    :  1;   // This bit is set to LOW if the card has not finished the power up routine
} OCR;

typedef struct  {
  UINT16       RCA;
  CARD_TYPE    CardType;
  OCR          OCRData;
  UINT8        Cid[16];
  UINT8        Csd[16];
  EMMC_EXT_CSD *ExtCsd;        // MMC V4 extended card specific
} CARD_INFO;

typedef struct _MMC_HOST_INSTANCE {
  UINTN                       Signature;
  LIST_ENTRY                  Link;
  EFI_HANDLE                  MmcHandle;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;

  MMC_STATE                   State;
  EFI_BLOCK_IO_PROTOCOL       BlockIo;
  CARD_INFO                   CardInfo;
  EFI_MMC_HOST_PROTOCOL       *MmcHost;
  EFI_UNICODE_STRING_TABLE    *ControllerNameTable;
  EFI_DISK_INFO_PROTOCOL      DiskInfo;

  BOOLEAN                     Initialized;
} MMC_HOST_INSTANCE;

#define MMC_HOST_INSTANCE_SIGNATURE  SIGNATURE_32('m', 'm', 'c', 'h')
#define MMC_HOST_INSTANCE_FROM_BLOCK_IO_THIS(a)  CR (a, MMC_HOST_INSTANCE, BlockIo, MMC_HOST_INSTANCE_SIGNATURE)
#define MMC_HOST_INSTANCE_FROM_LINK(a)           CR (a, MMC_HOST_INSTANCE, Link, MMC_HOST_INSTANCE_SIGNATURE)
#define MMC_HOST_INSTANCE_FROM_MMCHOST(a)        CR (a, MMC_HOST_INSTANCE, MmcHost, MMC_HOST_INSTANCE_SIGNATURE)
#define MMC_HOST_INSTANCE_FROM_DISKINFO(a)       CR (a, MMC_HOST_INSTANCE, DiskInfo, MMC_HOST_INSTANCE_SIGNATURE)

EFI_STATUS
EFIAPI
MmcGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

EFI_STATUS
EFIAPI
MmcGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle        OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  );

extern EFI_COMPONENT_NAME_PROTOCOL   gMmcComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gMmcComponentName2;

extern EFI_DRIVER_DIAGNOSTICS2_PROTOCOL  gMmcDriverDiagnostics2;

extern LIST_ENTRY  mMmcHostPool;
extern EFI_DRIVER_BINDING_PROTOCOL  gMmcDriverBinding;

/**
  Reset the block device.

  This function implements EFI_BLOCK_IO_PROTOCOL.Reset().
  It resets the block device hardware.
  ExtendedVerification is ignored in this implementation.

  @param  This                   Indicates a pointer to the calling context.
  @param  ExtendedVerification   Indicates that the driver may perform a more exhaustive
                                 verification operation of the device during reset.

  @retval EFI_SUCCESS            The block device was reset.
  @retval EFI_DEVICE_ERROR       The block device is not functioning correctly and could not be reset.

**/
EFI_STATUS
EFIAPI
MmcReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  );

/**
  Reads the requested number of blocks from the device.

  This function implements EFI_BLOCK_IO_PROTOCOL.ReadBlocks().
  It reads the requested number of blocks from the device.
  All the blocks are read, or an error is returned.

  @param  This                   Indicates a pointer to the calling context.
  @param  MediaId                The media ID that the read request is for.
  @param  Lba                    The starting logical block address to read from on the device.
  @param  BufferSize             The size of the Buffer in bytes.
                                 This must be a multiple of the intrinsic block size of the device.
  @param  Buffer                 A pointer to the destination buffer for the data. The caller is
                                 responsible for either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS            The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR       The device reported an error while attempting to perform the read operation.
  @retval EFI_NO_MEDIA           There is no media in the device.
  @retval EFI_MEDIA_CHANGED      The MediaId is not for the current media.
  @retval EFI_BAD_BUFFER_SIZE    The BufferSize parameter is not a multiple of the intrinsic block size of the device.
  @retval EFI_INVALID_PARAMETER  The read request contains LBAs that are not valid,
                                 or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
MmcReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  OUT VOID                  *Buffer
  );

/**
  Writes a specified number of blocks to the device.

  This function implements EFI_BLOCK_IO_PROTOCOL.WriteBlocks().
  It writes a specified number of blocks to the device.
  All blocks are written, or an error is returned.

  @param  This                   Indicates a pointer to the calling context.
  @param  MediaId                The media ID that the write request is for.
  @param  Lba                    The starting logical block address to be written.
  @param  BufferSize             The size of the Buffer in bytes.
                                 This must be a multiple of the intrinsic block size of the device.
  @param  Buffer                 Pointer to the source buffer for the data.

  @retval EFI_SUCCESS            The data were written correctly to the device.
  @retval EFI_WRITE_PROTECTED    The device cannot be written to.
  @retval EFI_NO_MEDIA           There is no media in the device.
  @retval EFI_MEDIA_CHANGED      The MediaId is not for the current media.
  @retval EFI_DEVICE_ERROR       The device reported an error while attempting to perform the write operation.
  @retval EFI_BAD_BUFFER_SIZE    The BufferSize parameter is not a multiple of the intrinsic
                                 block size of the device.
  @retval EFI_INVALID_PARAMETER  The write request contains LBAs that are not valid,
                                 or the buffer is not on proper alignment.

**/
EFI_STATUS
EFIAPI
MmcWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  );

/**
  Flushes all modified data to a physical block device.

  @param  This                   Indicates a pointer to the calling context.

  @retval EFI_SUCCESS            All outstanding data were written correctly to the device.
  @retval EFI_DEVICE_ERROR       The device reported an error while attempting to write data.
  @retval EFI_NO_MEDIA           There is no media in the device.

**/
EFI_STATUS
EFIAPI
MmcFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  );

EFI_STATUS
MmcNotifyState (
  IN MMC_HOST_INSTANCE  *MmcHostInstance,
  IN MMC_STATE          State
  );

EFI_STATUS
InitializeMmcDevice (
  IN  MMC_HOST_INSTANCE  *MmcHost
  );

VOID
EFIAPI
CheckCardsCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  );

VOID
PrintCSD (
  IN UINT32  *Csd
  );

VOID
PrintRCA (
  IN UINT32  Rca
  );

VOID
PrintOCR (
  IN UINT32  Ocr
  );

VOID
PrintResponseR1 (
  IN  UINT32  Response
  );

VOID
PrintSdCid (
  IN SD_CID  *Cid
  );

VOID
PrintSdCsd (
  IN SD_CSD  *Csd
  );

VOID
PrintEmmcCid (
  IN EMMC_CID  *Cid
  );

VOID
PrintEmmcCsd (
  IN EMMC_CSD  *Csd
  );

VOID
PrintEmmcExtCsd (
  IN EMMC_EXT_CSD  *ExtCsd
  );
#endif
