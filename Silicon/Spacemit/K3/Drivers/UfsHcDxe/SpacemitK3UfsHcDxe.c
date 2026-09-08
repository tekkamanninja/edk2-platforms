/** @file
  SPACEMIT K3 UFS Host Controller DXE Driver

  Copyright (C) 2025 SPACEMIT Corporation
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SpacemitK3UfsHcDxe.h"
#include <Library/DmaIoMmuConfig.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Protocol/ScsiPassThruExt.h>
#include <Protocol/UfsDeviceConfig.h>

typedef struct {
  MEMMAP_DEVICE_PATH          MemMap;
  EFI_DEVICE_PATH_PROTOCOL    End;
} SPACEMIT_K3_UFS_HC_DEVICE_PATH;

typedef struct {
  UINT32                  Signature;
  LIST_ENTRY              Link;
  EFI_PHYSICAL_ADDRESS    HostAddress;
  UINTN                   Pages;
  UINT64                  OriginalAttributes;
} SPACEMIT_K3_UFS_DMA_ALLOC_INFO;

#define SPACEMIT_K3_UFS_DMA_ALLOC_INFO_SIGNATURE                               \
  SIGNATURE_32(0x55, 0x41, 0x4C, 0x43)
#define SPACEMIT_K3_UFS_DMA_ALLOC_INFO_FROM_LINK(a)                            \
  CR(a, SPACEMIT_K3_UFS_DMA_ALLOC_INFO, Link,                                  \
     SPACEMIT_K3_UFS_DMA_ALLOC_INFO_SIGNATURE)

typedef struct {
  UINT32                                 Signature;
  EDKII_UFS_HOST_CONTROLLER_OPERATION    Operation;
  VOID                                   *HostAddress;
  UINTN                                  NumberOfBytes;
} SPACEMIT_K3_UFS_DMA_MAP_INFO;

#define SPACEMIT_K3_UFS_DMA_MAP_INFO_SIGNATURE                                 \
  SIGNATURE_32(0x55, 0x4D, 0x41, 0x50)

/*
 * Linux-derived safe transfer limit for K3 UFS host:
 *   sg_tablesize = SG_ALL = SG_CHUNK_SIZE = 128
 *   max_segment  = PAGE_SIZE (4KB on K3)
 *   max_sectors  = 1MB / 512
 * Therefore, use min(128 * 4KB, 1MB) = 512KB as a strict safe ceiling.
 */
#define SPACEMIT_K3_UFS_KERNEL_SG_TABLE_SIZE     128U
#define SPACEMIT_K3_UFS_KERNEL_SG_SEGMENT_BYTES  SIZE_4KB
#define SPACEMIT_K3_UFS_KERNEL_MAX_DMA_BYTES                                   \
  (SPACEMIT_K3_UFS_KERNEL_SG_TABLE_SIZE *                                      \
   SPACEMIT_K3_UFS_KERNEL_SG_SEGMENT_BYTES)
#define SPACEMIT_K3_UFS_KERNEL_MAX_SECTORS  (SIZE_1MB / 512U)
#define SPACEMIT_K3_UFS_KERNEL_MAX_SECTOR_BYTES                                \
  (SPACEMIT_K3_UFS_KERNEL_MAX_SECTORS * 512U)
#define SPACEMIT_K3_UFS_SAFE_MAX_TRANSFER_BYTES                                \
  ((SPACEMIT_K3_UFS_KERNEL_MAX_DMA_BYTES <                                     \
    SPACEMIT_K3_UFS_KERNEL_MAX_SECTOR_BYTES)                                   \
       ? SPACEMIT_K3_UFS_KERNEL_MAX_DMA_BYTES                                  \
       : SPACEMIT_K3_UFS_KERNEL_MAX_SECTOR_BYTES)

typedef struct {
  UINT32    AdapterId;
  UINT32    Attributes;
  UINT32    IoAlign;
  UINT32    MaxTransferBytes;
} SPACEMIT_K3_EXT_SCSI_PASS_THRU_MODE_EX;

typedef struct {
  UINT32                                      Signature;
  LIST_ENTRY                                  Link;
  EFI_HANDLE                                  Handle;
  EFI_EXT_SCSI_PASS_THRU_PROTOCOL             *ExtScsiPassThru;
  EFI_EXT_SCSI_PASS_THRU_PASSTHRU             OriginalPassThru;
  EFI_EXT_SCSI_PASS_THRU_BUILD_DEVICE_PATH    OriginalBuildDevicePath;
  EFI_EXT_SCSI_PASS_THRU_MODE                 *OriginalMode;
  SPACEMIT_K3_EXT_SCSI_PASS_THRU_MODE_EX      *PatchedMode;
} SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO;

#define SPACEMIT_K3_SCSI_OP_READ_10       0x28
#define SPACEMIT_K3_SCSI_OP_WRITE_10      0x2A
#define SPACEMIT_K3_SCSI_OP_READ_12       0xA8
#define SPACEMIT_K3_SCSI_OP_WRITE_12      0xAA
#define SPACEMIT_K3_SCSI_OP_READ_16       0x88
#define SPACEMIT_K3_SCSI_OP_WRITE_16      0x8A
#define SPACEMIT_K3_UFS_PASS_THRU_SIG     SIGNATURE_32('U', 'F', 'S', 'P')
#define SPACEMIT_K3_UFS_RPMB_EXPOSED_BIT  BIT11

typedef struct {
  UINT8     Lun[12];
  UINT16    BitMask : 12;
  UINT16    Rsvd    : 4;
} SPACEMIT_K3_UFS_EXPOSED_LUNS;

typedef struct {
  UINT32                                Signature;
  EFI_HANDLE                            Handle;
  EFI_EXT_SCSI_PASS_THRU_MODE           ExtScsiPassThruMode;
  EFI_EXT_SCSI_PASS_THRU_PROTOCOL       ExtScsiPassThru;
  EFI_UFS_DEVICE_CONFIG_PROTOCOL        UfsDevConfig;
  EDKII_UFS_HOST_CONTROLLER_PROTOCOL    *UfsHostController;
  UINTN                                 UfsHcBase;
  EDKII_UFS_HC_INFO                     UfsHcInfo;
  EDKII_UFS_HC_DRIVER_INTERFACE         UfsHcDriverInterface;
  UINT8                                 TaskTag;
  VOID                                  *UtpTrlBase;
  UINT8                                 Nutrs;
  VOID                                  *TrlMapping;
  VOID                                  *UtpTmrlBase;
  UINT8                                 Nutmrs;
  VOID                                  *TmrlMapping;
  SPACEMIT_K3_UFS_EXPOSED_LUNS          Luns;
} SPACEMIT_K3_UFS_PASS_THRU_PRIVATE_DATA;

#define SPACEMIT_K3_UFS_PASS_THRU_PRIVATE_DATA_FROM_EXT_SCSI(a)               \
  CR(a, SPACEMIT_K3_UFS_PASS_THRU_PRIVATE_DATA, ExtScsiPassThru,               \
     SPACEMIT_K3_UFS_PASS_THRU_SIG)

typedef struct {
  BOOLEAN    IsRead;
  UINT8      TransferLenOffset;
  UINT8      TransferLenWidth;
  UINT8      LbaOffset;
  UINT8      LbaWidth;
  UINT64     StartLba;
  UINT32     TotalBlocks;
  UINT32     BlockBytes;
  UINT32     DataBytes;
} SPACEMIT_K3_SCSI_RW_CMD_INFO;

#define SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO_SIGNATURE                     \
  SIGNATURE_32(0x55, 0x53, 0x4D, 0x50)
#define SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO_FROM_LINK(a)                  \
  CR(a, SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO, Link,                        \
     SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO_SIGNATURE)

STATIC EFI_EVENT   mSpacemitK3UfsExtScsiPassThruNotifyEvent         = NULL;
STATIC VOID        *mSpacemitK3UfsExtScsiPassThruNotifyRegistration = NULL;
STATIC LIST_ENTRY  mSpacemitK3UfsExtScsiModePatchList               =
  INITIALIZE_LIST_HEAD_VARIABLE (mSpacemitK3UfsExtScsiModePatchList);

#define SPACEMIT_K3_UFS_ACLK_NAME  "UFSACLK"

STATIC
BOOLEAN
SpacemitK3UfsHideRpmbLunFromExtScsi (
  IN EFI_EXT_SCSI_PASS_THRU_PROTOCOL  *ExtScsiPassThru
  )
{
  SPACEMIT_K3_UFS_PASS_THRU_PRIVATE_DATA  *Private;
  UINT16                                  OriginalBitMask;

  if (ExtScsiPassThru == NULL) {
    return FALSE;
  }

  Private = SPACEMIT_K3_UFS_PASS_THRU_PRIVATE_DATA_FROM_EXT_SCSI (ExtScsiPassThru);
  if (Private == NULL) {
    return FALSE;
  }

  OriginalBitMask        = Private->Luns.BitMask;
  Private->Luns.BitMask &= (UINT16) ~SPACEMIT_K3_UFS_RPMB_EXPOSED_BIT;

  if (OriginalBitMask != Private->Luns.BitMask) {
    DEBUG ((
      DEBUG_INFO,
      "UFS: hide RPMB WLUN from generic ExtScsi enumeration "
      "(bitmask=0x%x->0x%x)\n",
      OriginalBitMask,
      Private->Luns.BitMask
      ));
  }

  return TRUE;
}

STATIC
SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO
*
SpacemitK3UfsFindExtScsiModePatchInfo (
  IN EFI_HANDLE  Handle
  )
{
  LIST_ENTRY                                *Entry;
  SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO  *PatchInfo;

  for (Entry = GetFirstNode (&mSpacemitK3UfsExtScsiModePatchList);

       !IsNull (&mSpacemitK3UfsExtScsiModePatchList, Entry);
       Entry = GetNextNode (&mSpacemitK3UfsExtScsiModePatchList, Entry))
  {
    PatchInfo = SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO_FROM_LINK (Entry);
    if (PatchInfo->Handle == Handle) {
      return PatchInfo;
    }
  }

  return NULL;
}

STATIC
SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO
*
SpacemitK3UfsFindExtScsiModePatchByProtocol (
  IN EFI_EXT_SCSI_PASS_THRU_PROTOCOL  *ExtScsiPassThru
  )
{
  LIST_ENTRY                                *Entry;
  SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO  *PatchInfo;

  for (Entry = GetFirstNode (&mSpacemitK3UfsExtScsiModePatchList);
       !IsNull (&mSpacemitK3UfsExtScsiModePatchList, Entry);
       Entry = GetNextNode (&mSpacemitK3UfsExtScsiModePatchList, Entry))
  {
    PatchInfo = SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO_FROM_LINK (Entry);
    if (PatchInfo->ExtScsiPassThru == ExtScsiPassThru) {
      return PatchInfo;
    }
  }

  return NULL;
}

STATIC
UINT16
SpacemitK3ReadBe16 (
  IN CONST UINT8  *Buf
  )
{
  return (UINT16)((Buf[0] << 8) | Buf[1]);
}

STATIC
UINT32
SpacemitK3ReadBe32 (
  IN CONST UINT8  *Buf
  )
{
  return ((UINT32)Buf[0] << 24) | ((UINT32)Buf[1] << 16) |
         ((UINT32)Buf[2] << 8) | (UINT32)Buf[3];
}

STATIC
UINT64
SpacemitK3ReadBe64 (
  IN CONST UINT8  *Buf
  )
{
  return ((UINT64)Buf[0] << 56) | ((UINT64)Buf[1] << 48) |
         ((UINT64)Buf[2] << 40) | ((UINT64)Buf[3] << 32) |
         ((UINT64)Buf[4] << 24) | ((UINT64)Buf[5] << 16) |
         ((UINT64)Buf[6] << 8) | (UINT64)Buf[7];
}

STATIC
VOID
SpacemitK3WriteBe16 (
  OUT UINT8  *Buf,
  IN UINT16  Value
  )
{
  Buf[0] = (UINT8)(Value >> 8);
  Buf[1] = (UINT8)(Value & 0xFF);
}

STATIC
VOID
SpacemitK3WriteBe32 (
  OUT UINT8  *Buf,
  IN UINT32  Value
  )
{
  Buf[0] = (UINT8)(Value >> 24);
  Buf[1] = (UINT8)(Value >> 16);
  Buf[2] = (UINT8)(Value >> 8);
  Buf[3] = (UINT8)(Value & 0xFF);
}

STATIC
VOID
SpacemitK3WriteBe64 (
  OUT UINT8  *Buf,
  IN UINT64  Value
  )
{
  Buf[0] = (UINT8)(Value >> 56);
  Buf[1] = (UINT8)(Value >> 48);
  Buf[2] = (UINT8)(Value >> 40);
  Buf[3] = (UINT8)(Value >> 32);
  Buf[4] = (UINT8)(Value >> 24);
  Buf[5] = (UINT8)(Value >> 16);
  Buf[6] = (UINT8)(Value >> 8);
  Buf[7] = (UINT8)(Value & 0xFF);
}

STATIC
VOID
SpacemitK3SetScsiLba (
  IN OUT UINT8  *Cdb,
  IN UINT8      LbaOffset,
  IN UINT8      LbaWidth,
  IN UINT64     Lba
  )
{
  if (LbaWidth == 8) {
    SpacemitK3WriteBe64 (&Cdb[LbaOffset], Lba);
  } else {
    SpacemitK3WriteBe32 (&Cdb[LbaOffset], (UINT32)Lba);
  }
}

STATIC
VOID
SpacemitK3SetScsiTransferBlocks (
  IN OUT UINT8  *Cdb,
  IN UINT8      TransferLenOffset,
  IN UINT8      TransferLenWidth,
  IN UINT32     Blocks
  )
{
  if (TransferLenWidth == 2) {
    SpacemitK3WriteBe16 (&Cdb[TransferLenOffset], (UINT16)Blocks);
  } else {
    SpacemitK3WriteBe32 (&Cdb[TransferLenOffset], Blocks);
  }
}

STATIC
BOOLEAN
SpacemitK3ParseScsiRwCmd (
  IN EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET  *Packet,
  OUT SPACEMIT_K3_SCSI_RW_CMD_INFO               *CmdInfo
  )
{
  UINT8   Opcode;
  UINT32  DataBytes;
  UINT32  TotalBlocks;

  if ((Packet == NULL) || (CmdInfo == NULL) || (Packet->Cdb == NULL) ||
      (Packet->CdbLength < 10))
  {
    return FALSE;
  }

  ZeroMem (CmdInfo, sizeof (*CmdInfo));
  Opcode = ((UINT8 *)Packet->Cdb)[0];

  switch (Opcode) {
    case SPACEMIT_K3_SCSI_OP_READ_10:
      CmdInfo->IsRead            = TRUE;
      CmdInfo->LbaOffset         = 2;
      CmdInfo->LbaWidth          = 4;
      CmdInfo->TransferLenOffset = 7;
      CmdInfo->TransferLenWidth  = 2;
      break;
    case SPACEMIT_K3_SCSI_OP_WRITE_10:
      CmdInfo->IsRead            = FALSE;
      CmdInfo->LbaOffset         = 2;
      CmdInfo->LbaWidth          = 4;
      CmdInfo->TransferLenOffset = 7;
      CmdInfo->TransferLenWidth  = 2;
      break;
    case SPACEMIT_K3_SCSI_OP_READ_12:
      CmdInfo->IsRead            = TRUE;
      CmdInfo->LbaOffset         = 2;
      CmdInfo->LbaWidth          = 4;
      CmdInfo->TransferLenOffset = 6;
      CmdInfo->TransferLenWidth  = 4;
      break;
    case SPACEMIT_K3_SCSI_OP_WRITE_12:
      CmdInfo->IsRead            = FALSE;
      CmdInfo->LbaOffset         = 2;
      CmdInfo->LbaWidth          = 4;
      CmdInfo->TransferLenOffset = 6;
      CmdInfo->TransferLenWidth  = 4;
      break;
    case SPACEMIT_K3_SCSI_OP_READ_16:
      CmdInfo->IsRead            = TRUE;
      CmdInfo->LbaOffset         = 2;
      CmdInfo->LbaWidth          = 8;
      CmdInfo->TransferLenOffset = 10;
      CmdInfo->TransferLenWidth  = 4;
      break;
    case SPACEMIT_K3_SCSI_OP_WRITE_16:
      CmdInfo->IsRead            = FALSE;
      CmdInfo->LbaOffset         = 2;
      CmdInfo->LbaWidth          = 8;
      CmdInfo->TransferLenOffset = 10;
      CmdInfo->TransferLenWidth  = 4;
      break;
    default:
      return FALSE;
  }

  if (CmdInfo->TransferLenWidth == 2) {
    TotalBlocks =
      SpacemitK3ReadBe16 (&((UINT8 *)Packet->Cdb)[CmdInfo->TransferLenOffset]);
  } else {
    TotalBlocks =
      SpacemitK3ReadBe32 (&((UINT8 *)Packet->Cdb)[CmdInfo->TransferLenOffset]);
  }

  if (TotalBlocks == 0) {
    return FALSE;
  }

  if (CmdInfo->LbaWidth == 8) {
    CmdInfo->StartLba =
      SpacemitK3ReadBe64 (&((UINT8 *)Packet->Cdb)[CmdInfo->LbaOffset]);
  } else {
    CmdInfo->StartLba =
      SpacemitK3ReadBe32 (&((UINT8 *)Packet->Cdb)[CmdInfo->LbaOffset]);
  }

  if (CmdInfo->IsRead) {
    if (Packet->DataDirection != EFI_EXT_SCSI_DATA_DIRECTION_READ) {
      return FALSE;
    }

    DataBytes = Packet->InTransferLength;
  } else {
    if (Packet->DataDirection != EFI_EXT_SCSI_DATA_DIRECTION_WRITE) {
      return FALSE;
    }

    DataBytes = Packet->OutTransferLength;
  }

  if ((DataBytes == 0) || (DataBytes % TotalBlocks != 0)) {
    return FALSE;
  }

  CmdInfo->DataBytes   = DataBytes;
  CmdInfo->TotalBlocks = TotalBlocks;
  CmdInfo->BlockBytes  = DataBytes / TotalBlocks;
  return (CmdInfo->BlockBytes != 0);
}

STATIC
EFI_STATUS
EFIAPI
SpacemitK3UfsExtScsiPassThruWithLimit (
  IN EFI_EXT_SCSI_PASS_THRU_PROTOCOL                 *This,
  IN UINT8                                           *Target,
  IN UINT64                                          Lun,
  IN OUT EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET  *Packet,
  IN EFI_EVENT                                       Event OPTIONAL
  )
{
  SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO    *PatchInfo;
  SPACEMIT_K3_SCSI_RW_CMD_INFO                CmdInfo;
  EFI_EXT_SCSI_PASS_THRU_SCSI_REQUEST_PACKET  SubPacket;
  EFI_STATUS                                  Status;
  UINT8                                       SubCdb[16];
  UINT32                                      MaxChunkBlocks;
  UINT32                                      RemainingBlocks;
  UINT64                                      CurrentLba;
  UINT32                                      TransferredBytes;
  UINT32                                      RequestedChunkBlocks;
  UINT32                                      RequestedChunkBytes;
  UINT32                                      ActualChunkBytes;
  UINT32                                      ActualChunkBlocks;

  PatchInfo = SpacemitK3UfsFindExtScsiModePatchByProtocol (This);
  if ((PatchInfo == NULL) || (PatchInfo->OriginalPassThru == NULL)) {
    return EFI_DEVICE_ERROR;
  }

  if ((Event != NULL) || (Packet == NULL) || (Packet->Cdb == NULL) ||
      (Packet->CdbLength > sizeof (SubCdb)))
  {
    return PatchInfo->OriginalPassThru (This, Target, Lun, Packet, Event);
  }

  if (!SpacemitK3ParseScsiRwCmd (Packet, &CmdInfo) ||
      (CmdInfo.DataBytes <= SPACEMIT_K3_UFS_SAFE_MAX_TRANSFER_BYTES))
  {
    return PatchInfo->OriginalPassThru (This, Target, Lun, Packet, Event);
  }

  MaxChunkBlocks = SPACEMIT_K3_UFS_SAFE_MAX_TRANSFER_BYTES / CmdInfo.BlockBytes;
  if ((MaxChunkBlocks == 0) || (CmdInfo.TotalBlocks <= MaxChunkBlocks)) {
    return PatchInfo->OriginalPassThru (This, Target, Lun, Packet, Event);
  }

  RemainingBlocks  = CmdInfo.TotalBlocks;
  CurrentLba       = CmdInfo.StartLba;
  TransferredBytes = 0;

  while (RemainingBlocks > 0) {
    RequestedChunkBlocks =
      (RemainingBlocks > MaxChunkBlocks) ? MaxChunkBlocks : RemainingBlocks;
    RequestedChunkBytes = RequestedChunkBlocks * CmdInfo.BlockBytes;

    CopyMem (&SubPacket, Packet, sizeof (SubPacket));
    CopyMem (SubCdb, Packet->Cdb, Packet->CdbLength);

    SpacemitK3SetScsiLba (
      SubCdb,
      CmdInfo.LbaOffset,
      CmdInfo.LbaWidth,
      CurrentLba
      );
    SpacemitK3SetScsiTransferBlocks (
      SubCdb,
      CmdInfo.TransferLenOffset,
      CmdInfo.TransferLenWidth,
      RequestedChunkBlocks
      );

    SubPacket.Cdb             = SubCdb;
    SubPacket.SenseDataLength = Packet->SenseDataLength;
    if (CmdInfo.IsRead) {
      SubPacket.InDataBuffer =
        (VOID *)((UINT8 *)Packet->InDataBuffer + TransferredBytes);
      SubPacket.InTransferLength = RequestedChunkBytes;
    } else {
      SubPacket.OutDataBuffer =
        (VOID *)((UINT8 *)Packet->OutDataBuffer + TransferredBytes);
      SubPacket.OutTransferLength = RequestedChunkBytes;
    }

    Status = PatchInfo->OriginalPassThru (This, Target, Lun, &SubPacket, NULL);

    Packet->HostAdapterStatus = SubPacket.HostAdapterStatus;
    Packet->TargetStatus      = SubPacket.TargetStatus;
    Packet->SenseDataLength   = SubPacket.SenseDataLength;

    ActualChunkBytes = CmdInfo.IsRead ? SubPacket.InTransferLength
                                      : SubPacket.OutTransferLength;
    if ((ActualChunkBytes == 0) ||
        (ActualChunkBytes % CmdInfo.BlockBytes != 0))
    {
      if (CmdInfo.IsRead) {
        Packet->InTransferLength = TransferredBytes;
      } else {
        Packet->OutTransferLength = TransferredBytes;
      }

      return EFI_DEVICE_ERROR;
    }

    if (ActualChunkBytes > RequestedChunkBytes) {
      ActualChunkBytes = RequestedChunkBytes;
    }

    ActualChunkBlocks = ActualChunkBytes / CmdInfo.BlockBytes;
    TransferredBytes += ActualChunkBytes;
    CurrentLba       += ActualChunkBlocks;
    RemainingBlocks  -= ActualChunkBlocks;

    if (EFI_ERROR (Status)) {
      if (CmdInfo.IsRead) {
        Packet->InTransferLength = TransferredBytes;
      } else {
        Packet->OutTransferLength = TransferredBytes;
      }

      return Status;
    }

    if (ActualChunkBlocks < RequestedChunkBlocks) {
      if (CmdInfo.IsRead) {
        Packet->InTransferLength = TransferredBytes;
      } else {
        Packet->OutTransferLength = TransferredBytes;
      }

      return EFI_BAD_BUFFER_SIZE;
    }
  }

  if (CmdInfo.IsRead) {
    Packet->InTransferLength = TransferredBytes;
  } else {
    Packet->OutTransferLength = TransferredBytes;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
SpacemitK3UfsExtScsiBuildDevicePathWithConformance (
  IN EFI_EXT_SCSI_PASS_THRU_PROTOCOL  *This,
  IN UINT8                            *Target,
  IN UINT64                           Lun,
  IN OUT EFI_DEVICE_PATH_PROTOCOL     **DevicePath
  )
{
  SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO  *PatchInfo;
  UINT8                                     ExpectedTarget[TARGET_MAX_BYTES];

  if ((This == NULL) || (Target == NULL) || (DevicePath == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  PatchInfo = SpacemitK3UfsFindExtScsiModePatchByProtocol (This);
  if ((PatchInfo == NULL) || (PatchInfo->OriginalBuildDevicePath == NULL)) {
    return EFI_DEVICE_ERROR;
  }

  SetMem (ExpectedTarget, sizeof (ExpectedTarget), 0x00);
  if (CompareMem (Target, ExpectedTarget, sizeof (ExpectedTarget)) != 0) {
    return EFI_NOT_FOUND;
  }

  return PatchInfo->OriginalBuildDevicePath (This, Target, Lun, DevicePath);
}

STATIC
VOID EFIAPI
SpacemitK3UfsExtScsiPassThruNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                                Status;
  EFI_HANDLE                                Handle;
  UINTN                                     HandleSize;
  EDKII_UFS_HOST_CONTROLLER_PROTOCOL        *UfsHostController;
  EFI_EXT_SCSI_PASS_THRU_PROTOCOL           *ExtScsiPassThru;
  SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO  *PatchInfo;
  SPACEMIT_K3_EXT_SCSI_PASS_THRU_MODE_EX    *PatchedMode;

  (VOID)Event;
  (VOID)Context;

  while (TRUE) {
    HandleSize = sizeof (Handle);
    Status     = gBS->LocateHandle (
                        ByRegisterNotify,
                        NULL,
                        mSpacemitK3UfsExtScsiPassThruNotifyRegistration,
                        &HandleSize,
                        &Handle
                        );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiExtScsiPassThruProtocolGuid,
                    (VOID **)&ExtScsiPassThru
                    );
    if (EFI_ERROR (Status) || (ExtScsiPassThru == NULL) ||
        (ExtScsiPassThru->Mode == NULL))
    {
      continue;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEdkiiUfsHostControllerProtocolGuid,
                    (VOID **)&UfsHostController
                    );
    if (EFI_ERROR (Status) || (UfsHostController == NULL) ||
        !SpacemitK3UfsHideRpmbLunFromExtScsi (ExtScsiPassThru))
    {
      continue;
    }

    PatchInfo = SpacemitK3UfsFindExtScsiModePatchInfo (Handle);
    if (PatchInfo != NULL) {
      if (PatchInfo->PatchedMode != NULL) {
        PatchInfo->PatchedMode->AdapterId        = ExtScsiPassThru->Mode->AdapterId;
        PatchInfo->PatchedMode->Attributes       = ExtScsiPassThru->Mode->Attributes;
        PatchInfo->PatchedMode->IoAlign          = ExtScsiPassThru->Mode->IoAlign;
        PatchInfo->PatchedMode->MaxTransferBytes =
          SPACEMIT_K3_UFS_SAFE_MAX_TRANSFER_BYTES;

        if ((ExtScsiPassThru->PassThru !=
             SpacemitK3UfsExtScsiPassThruWithLimit) &&
            (ExtScsiPassThru->PassThru != NULL))
        {
          PatchInfo->OriginalPassThru = ExtScsiPassThru->PassThru;
        }

        if ((ExtScsiPassThru->BuildDevicePath !=
             SpacemitK3UfsExtScsiBuildDevicePathWithConformance) &&
            (ExtScsiPassThru->BuildDevicePath != NULL))
        {
          PatchInfo->OriginalBuildDevicePath = ExtScsiPassThru->BuildDevicePath;
        }

        PatchInfo->ExtScsiPassThru = ExtScsiPassThru;
        PatchInfo->OriginalMode    = ExtScsiPassThru->Mode;
        ExtScsiPassThru->Mode      =
          (EFI_EXT_SCSI_PASS_THRU_MODE *)PatchInfo->PatchedMode;
        ExtScsiPassThru->PassThru        = SpacemitK3UfsExtScsiPassThruWithLimit;
        ExtScsiPassThru->BuildDevicePath =
          SpacemitK3UfsExtScsiBuildDevicePathWithConformance;
        DEBUG (
          (DEBUG_INFO,
           "UFS: refreshed ExtScsi hooks for Handle=%p (max transfer=0x%x)\n",
           Handle, PatchInfo->PatchedMode->MaxTransferBytes));
      }

      continue;
    }

    PatchedMode = AllocateZeroPool (sizeof (*PatchedMode));
    if (PatchedMode == NULL) {
      DEBUG (
        (DEBUG_WARN,
         "UFS: failed to allocate ExtScsi mode patch buffer for Handle=%p\n",
         Handle));
      continue;
    }

    PatchInfo = AllocateZeroPool (sizeof (*PatchInfo));
    if (PatchInfo == NULL) {
      DEBUG ((
        DEBUG_WARN,
        "UFS: failed to allocate ExtScsi patch info for Handle=%p\n",
        Handle
        ));
      FreePool (PatchedMode);
      continue;
    }

    PatchedMode->AdapterId        = ExtScsiPassThru->Mode->AdapterId;
    PatchedMode->Attributes       = ExtScsiPassThru->Mode->Attributes;
    PatchedMode->IoAlign          = ExtScsiPassThru->Mode->IoAlign;
    PatchedMode->MaxTransferBytes = SPACEMIT_K3_UFS_SAFE_MAX_TRANSFER_BYTES;

    PatchInfo->Signature               = SPACEMIT_K3_UFS_EXT_SCSI_MODE_PATCH_INFO_SIGNATURE;
    PatchInfo->Handle                  = Handle;
    PatchInfo->ExtScsiPassThru         = ExtScsiPassThru;
    PatchInfo->OriginalPassThru        = ExtScsiPassThru->PassThru;
    PatchInfo->OriginalBuildDevicePath = ExtScsiPassThru->BuildDevicePath;
    PatchInfo->OriginalMode            = ExtScsiPassThru->Mode;
    PatchInfo->PatchedMode             = PatchedMode;
    InsertTailList (&mSpacemitK3UfsExtScsiModePatchList, &PatchInfo->Link);

    ExtScsiPassThru->Mode            = (EFI_EXT_SCSI_PASS_THRU_MODE *)PatchedMode;
    ExtScsiPassThru->PassThru        = SpacemitK3UfsExtScsiPassThruWithLimit;
    ExtScsiPassThru->BuildDevicePath =
      SpacemitK3UfsExtScsiBuildDevicePathWithConformance;
    DEBUG ((
      DEBUG_INFO,
      "UFS: installed ExtScsi hooks for Handle=%p (max transfer=0x%x)\n",
      Handle,
      PatchedMode->MaxTransferBytes
      ));
  }
}

STATIC
EFI_STATUS
SpacemitK3UfsRegisterExtScsiPassThruNotify (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mSpacemitK3UfsExtScsiPassThruNotifyEvent != NULL) {
    return EFI_ALREADY_STARTED;
  }

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  SpacemitK3UfsExtScsiPassThruNotify,
                  NULL,
                  &mSpacemitK3UfsExtScsiPassThruNotifyEvent
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->RegisterProtocolNotify (
                  &gEfiExtScsiPassThruProtocolGuid,
                  mSpacemitK3UfsExtScsiPassThruNotifyEvent,
                  &mSpacemitK3UfsExtScsiPassThruNotifyRegistration
                  );
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (mSpacemitK3UfsExtScsiPassThruNotifyEvent);
    mSpacemitK3UfsExtScsiPassThruNotifyEvent        = NULL;
    mSpacemitK3UfsExtScsiPassThruNotifyRegistration = NULL;
    return Status;
  }

  SpacemitK3UfsExtScsiPassThruNotify (
    mSpacemitK3UfsExtScsiPassThruNotifyEvent,
    NULL
    );
  return EFI_SUCCESS;
}

STATIC
SPACEMIT_K3_UFS_DMA_ALLOC_INFO
*
SpacemitK3UfsFindDmaAllocInfo (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN EFI_PHYSICAL_ADDRESS             HostAddress
  )
{
  LIST_ENTRY                      *Entry;
  SPACEMIT_K3_UFS_DMA_ALLOC_INFO  *AllocInfo;

  for (Entry = GetFirstNode (&Private->DmaAllocList);
       !IsNull (&Private->DmaAllocList, Entry);
       Entry = GetNextNode (&Private->DmaAllocList, Entry))
  {
    AllocInfo = SPACEMIT_K3_UFS_DMA_ALLOC_INFO_FROM_LINK (Entry);
    if (AllocInfo->HostAddress == HostAddress) {
      return AllocInfo;
    }
  }

  return NULL;
}

STATIC
EFI_STATUS
SpacemitK3UfsSetDmaBufferUncached (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN EFI_PHYSICAL_ADDRESS             HostAddress,
  IN UINTN                            Pages,
  OUT UINT64                          *OriginalAttributes
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  UINT64                           NewAttributes;

  if ((OriginalAttributes == NULL) || (Pages == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Private->Cpu == NULL) || FeaturePcdGet (PcdDmaCoherent)) {
    return EFI_UNSUPPORTED;
  }

  Status = gDS->GetMemorySpaceDescriptor (HostAddress, &Descriptor);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *OriginalAttributes = Descriptor.Attributes;

  NewAttributes =
    (Descriptor.Attributes & ~EFI_CACHE_ATTRIBUTE_MASK) | EFI_MEMORY_UC;
  if (NewAttributes == Descriptor.Attributes) {
    return EFI_ALREADY_STARTED;
  }

  Status = Private->Cpu->FlushDataCache (
                           Private->Cpu,
                           HostAddress,
                           EFI_PAGES_TO_SIZE (Pages),
                           EfiCpuFlushTypeWriteBackInvalidate
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Private->Cpu->SetMemoryAttributes (
                           Private->Cpu,
                           HostAddress,
                           EFI_PAGES_TO_SIZE (Pages),
                           NewAttributes
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
SpacemitK3UfsRestoreDmaBufferAttributes (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN SPACEMIT_K3_UFS_DMA_ALLOC_INFO   *AllocInfo
  )
{
  EFI_STATUS  Status;

  if ((Private->Cpu == NULL) || (AllocInfo == NULL)) {
    return;
  }

  Status = Private->Cpu->SetMemoryAttributes (
                           Private->Cpu,
                           AllocInfo->HostAddress,
                           EFI_PAGES_TO_SIZE (AllocInfo->Pages),
                           AllocInfo->OriginalAttributes
                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "UFS: restore DMA buffer attributes failed Host=0x%lx Pages=0x%lx "
      "Status=%r\n",
      (UINTN)AllocInfo->HostAddress,
      AllocInfo->Pages,
      Status
      ));
    return;
  }

  (VOID)Private->Cpu->FlushDataCache (
                        Private->Cpu,
                        AllocInfo->HostAddress,
                        EFI_PAGES_TO_SIZE (AllocInfo->Pages),
                        EfiCpuFlushTypeInvalidate
                        );
}

STATIC
EFI_STATUS
SpacemitK3UfsTranslateDmaAddress (
  IN EFI_PHYSICAL_ADDRESS   HostAddress,
  IN OUT UINTN              *NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS  *DeviceAddress,
  OUT BOOLEAN               *Translated
  )
{
  DMA_IOMMU_MAPPINGS  *Mappings;
  UINTN               MappingSize;
  UINTN               MaxEntries;
  UINTN               Entries;
  UINTN               Index;
  UINT64              RangeRemaining;

  if ((NumberOfBytes == NULL) || (DeviceAddress == NULL) ||
      (Translated == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *Translated = FALSE;

  MappingSize = PcdGetSize (PcdDmaIoMmuMappings);
  Mappings    = (DMA_IOMMU_MAPPINGS *)PcdGetPtr (PcdDmaIoMmuMappings);
  if ((Mappings == NULL) ||
      (MappingSize <= OFFSET_OF (DMA_IOMMU_MAPPINGS, Data)))
  {
    *DeviceAddress = HostAddress;
    return EFI_SUCCESS;
  }

  MaxEntries = (MappingSize - OFFSET_OF (DMA_IOMMU_MAPPINGS, Data)) /
               sizeof (DMA_IOMMU_MAPPING);
  Entries = Mappings->Num;
  if (Entries > MaxEntries) {
    Entries = MaxEntries;
  }

  for (Index = 0; Index < Entries; Index++) {
    if ((HostAddress >= Mappings->Data[Index].CpuAddr) &&
        (HostAddress <
         (Mappings->Data[Index].CpuAddr + Mappings->Data[Index].Size)))
    {
      *DeviceAddress = Mappings->Data[Index].DmaAddr +
                       (HostAddress - Mappings->Data[Index].CpuAddr);
      RangeRemaining = Mappings->Data[Index].CpuAddr +
                       Mappings->Data[Index].Size - HostAddress;
      if (RangeRemaining < *NumberOfBytes) {
        *NumberOfBytes = (UINTN)RangeRemaining;
      }

      *Translated = TRUE;
      return EFI_SUCCESS;
    }
  }

  *DeviceAddress = HostAddress;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsBuildDevicePath (
  IN OUT SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  SPACEMIT_K3_UFS_HC_DEVICE_PATH  *DevicePath;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DevicePath = AllocateZeroPool (sizeof (*DevicePath));
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  DevicePath->MemMap.Header.Type      = HARDWARE_DEVICE_PATH;
  DevicePath->MemMap.Header.SubType   = HW_MEMMAP_DP;
  DevicePath->MemMap.Header.Length[0] = (UINT8)sizeof (MEMMAP_DEVICE_PATH);
  DevicePath->MemMap.Header.Length[1] =
    (UINT8)(sizeof (MEMMAP_DEVICE_PATH) >> 8);
  DevicePath->MemMap.MemoryType      = EfiMemoryMappedIO;
  DevicePath->MemMap.StartingAddress = (EFI_PHYSICAL_ADDRESS)Private->UfsHcBase;
  DevicePath->MemMap.EndingAddress   =
    (EFI_PHYSICAL_ADDRESS)(Private->UfsHcBase + SPACEMIT_K3_UFS_HC_MMIO_SIZE -
                           1);

  DevicePath->End.Type      = END_DEVICE_PATH_TYPE;
  DevicePath->End.SubType   = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  DevicePath->End.Length[0] = (UINT8)sizeof (EFI_DEVICE_PATH_PROTOCOL);
  DevicePath->End.Length[1] = (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL) >> 8);

  Private->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsMapMmioRegion (
  IN UINTN        Base,
  IN UINTN        Size,
  IN CONST CHAR8  *Name
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  UINT64                           Attributes;

  if ((Base == 0) || (Size == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gDS->GetMemorySpaceDescriptor (Base, &Descriptor);
  if (!EFI_ERROR (Status) &&
      (Descriptor.GcdMemoryType != EfiGcdMemoryTypeNonExistent))
  {
    Attributes = Descriptor.Attributes | EFI_MEMORY_UC;
    Status     = gDS->SetMemorySpaceAttributes (Base, Size, Attributes);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "UFS: SetMemorySpaceAttributes(%a) failed: %r\n",
        Name,
        Status
        ));
      return Status;
    }

    return EFI_SUCCESS;
  }

  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeMemoryMappedIo,
                  Base,
                  Size,
                  EFI_MEMORY_UC
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: AddMemorySpace(%a 0x%lx+0x%lx) failed: %r\n",
      Name,
      Base,
      Size,
      Status
      ));
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (Base, Size, EFI_MEMORY_UC);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "UFS: SetMemorySpaceAttributes(%a) failed after add: %r\n",
      Name,
      Status
      ));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
SpacemitK3UfsDmeSetLog (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  IN UINT32                           Value,
  IN CONST CHAR8                      *Name
  )
{
  EFI_STATUS  Status;

  Status = SpacemitK3UfsDmeSet (Private, UicArg1, Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: dme-set %a (attr 0x%x, val 0x%x) failed: %r\n",
      Name,
      (UicArg1 >> 16) & 0xFFFF,
      Value,
      Status
      ));
  }
}

STATIC
EFI_STATUS
SpacemitK3UfsDmeSetChecked (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  IN UINT32                           Value,
  IN CONST CHAR8                      *Name
  )
{
  EFI_STATUS  Status;

  Status = SpacemitK3UfsDmeSet (Private, UicArg1, Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: dme-set %a (attr 0x%x, val 0x%x) failed: %r\n",
      Name,
      (UicArg1 >> 16) & 0xFFFF,
      Value,
      Status
      ));
  }

  return Status;
}

STATIC
EFI_STATUS
SpacemitK3UfsWaitMphyPllLock (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN CONST CHAR8                      *Stage
  )
{
  UINT32  RegVal;
  UINT32  Timeout;

  Timeout = MPHY_PLL_LOCK_TIMEOUT_US;
  while (Timeout > 0) {
    RegVal = SpacemitK3UfsReadReg32 (
               Private,
               Private->Priv.PhyMngBase +
               UFS_MPHY_PU_CTRL
               );
    if ((RegVal & UFS_MPHY_PU_PLL_LOCK) != 0) {
      return EFI_SUCCESS;
    }

    gBS->Stall (1);
    Timeout--;
  }

  DEBUG ((
    DEBUG_ERROR,
    "UFS: MPHY PLL lock timeout in %a, UFS_MPHY_PU_CTRL=0x%x\n",
    Stage,
    SpacemitK3UfsReadReg32 (
      Private,
      Private->Priv.PhyMngBase + UFS_MPHY_PU_CTRL
      )
    ));
  return EFI_TIMEOUT;
}

STATIC
UINT64
SpacemitK3UfsGetTargetAclkRate (
  VOID
  )
{
  UINT64  ClockRate;

  ClockRate = PcdGet64 (PcdUfsAclkRate);
  if (ClockRate == 0) {
    ClockRate = UFS_ACLK_DEFAULT_HZ;
    DEBUG ((
      DEBUG_WARN,
      "UFS: PcdUfsAclkRate is 0, fallback to default %lluHz\n",
      ClockRate
      ));
  }

  return ClockRate;
}

STATIC
EFI_STATUS
SpacemitK3UfsSetAclkRate (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT64                           ClockRate
  )
{
  EFI_STATUS  Status;

  if ((Private == NULL) || (Private->ClockCtrl == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = Private->ClockCtrl->SetClockRate (
                                 Private->ClockCtrl,
                                 SPACEMIT_K3_UFS_ACLK_NAME,
                                 ClockRate
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: failed to set %a rate to %lluHz: %r\n",
      SPACEMIT_K3_UFS_ACLK_NAME,
      ClockRate,
      Status
      ));
  }

  return Status;
}

STATIC
UINT32
SpacemitK3UfsGetSys1clk1us (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;
  UINT64      ClockRate;

  if ((Private == NULL) || (Private->ClockCtrl == NULL)) {
    return 0;
  }

  Status = Private->ClockCtrl->GetClockRate (
                                 Private->ClockCtrl,
                                 SPACEMIT_K3_UFS_ACLK_NAME,
                                 &ClockRate
                                 );
  if (EFI_ERROR (Status) || (ClockRate == 0)) {
    ClockRate = SpacemitK3UfsGetTargetAclkRate ();
  }

  return (UINT32)((ClockRate + 999999ULL) / 1000000ULL);
}

STATIC
UINT32
SpacemitK3UfsGetTxSymbolClkNsUs (
  IN UINT32  Sys1Clk1Us
  )
{
  UINT32  TxSymbolClkNsUs;

  if (Sys1Clk1Us == 0) {
    return UFS_TX_SYMBOL_CLK_NS_US_409MHZ;
  }

  TxSymbolClkNsUs = (UINT32)((1000U + (Sys1Clk1Us / 2)) / Sys1Clk1Us);
  if (TxSymbolClkNsUs == 0) {
    TxSymbolClkNsUs = 1;
  }

  return TxSymbolClkNsUs << 10;
}

/**
  Clock enable function

  @param[in] Private  Pointer to private data structure

**/
EFI_STATUS
SpacemitK3UfsClkEnable (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if ((Private == NULL) || (Private->ClockCtrl == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  (VOID)Private->ClockCtrl->SetClockState (
                              Private->ClockCtrl,
                              SPACEMIT_K3_UFS_ACLK_NAME,
                              DISABLE_CLOCK
                              );

  Status = Private->ClockCtrl->SetClockState (
                                 Private->ClockCtrl,
                                 SPACEMIT_K3_UFS_ACLK_NAME,
                                 ENABLE_CLOCK
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: failed to enable %a: %r\n",
      SPACEMIT_K3_UFS_ACLK_NAME,
      Status
      ));
    return Status;
  }

  Status = SpacemitK3UfsSetAclkRate (Private, SpacemitK3UfsGetTargetAclkRate ());
  if (EFI_ERROR (Status)) {
    (VOID)Private->ClockCtrl->SetClockState (
                                Private->ClockCtrl,
                                SPACEMIT_K3_UFS_ACLK_NAME,
                                DISABLE_CLOCK
                                );
    return Status;
  }

  // HYNIX1 phone need delay
  gBS->Stall (5000);

  return EFI_SUCCESS;
}

/**
  Clock disable function

  @param[in] Private  Pointer to private data structure

**/
EFI_STATUS
SpacemitK3UfsClkDisable (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if ((Private == NULL) || (Private->ClockCtrl == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = Private->ClockCtrl->SetClockState (
                                 Private->ClockCtrl,
                                 SPACEMIT_K3_UFS_ACLK_NAME,
                                 DISABLE_CLOCK
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: failed to disable %a: %r\n",
      SPACEMIT_K3_UFS_ACLK_NAME,
      Status
      ));
  }

  return Status;
}

/**
  Check if controller is ready for UIC command

  @param[in] Private  Pointer to private data

  @retval TRUE   Controller is ready
  @retval FALSE  Controller is not ready
**/
STATIC
BOOLEAN
SpacemitK3UfsReadyForUicCmd (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  UINT32  ControllerStatus;

  ControllerStatus = SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_STATUS);

  // Controller should be enabled
  if (!(SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_ENABLE) &
        CONTROLLER_ENABLE))
  {
    return FALSE;
  }

  // UIC command path should be ready
  if ((ControllerStatus & UIC_COMMAND_READY) == 0) {
    return FALSE;
  }

  return TRUE;
}

STATIC
VOID
SpacemitK3UfsDumpUicErrorRegs (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           IntrStatus,
  IN CONST CHAR8                      *Stage
  )
{
  DEBUG ((
    DEBUG_WARN,
    "UFS: %a IS=0x%x UECPA=0x%x UECDL=0x%x UECN=0x%x UECT=0x%x UECDME=0x%x\n",
    Stage,
    IntrStatus,
    SpacemitK3UfsReadReg32 (Private, UFS_HC_UECPA_OFFSET),
    SpacemitK3UfsReadReg32 (Private, UFS_HC_UECDL_OFFSET),
    SpacemitK3UfsReadReg32 (Private, UFS_HC_UECN_OFFSET),
    SpacemitK3UfsReadReg32 (Private, UFS_HC_UECT_OFFSET),
    SpacemitK3UfsReadReg32 (Private, UFS_HC_UECDME_OFFSET)
    ));
}

/**
  Send UIC command

  @param[in]  Private     Pointer to private data
  @param[in]  Command     UIC command opcode
  @param[in]  Argument1   UIC argument 1
  @param[in]  Argument2   UIC argument 2
  @param[in]  Argument3   UIC argument 3
  @param[out] ResultArg2  Pointer to receive result argument 2
  @param[out] ResultArg3  Pointer to receive result argument 3

  @retval EFI_SUCCESS     Command executed successfully
  @retval EFI_NOT_READY   Controller not ready
  @retval EFI_TIMEOUT     Command timeout
  @retval EFI_DEVICE_ERROR  Device error
**/
STATIC
EFI_STATUS
SpacemitK3UfsSendUicCmd (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           Command,
  IN UINT32                           Argument1,
  IN UINT32                           Argument2,
  IN UINT32                           Argument3,
  OUT UINT32                          *ResultArg2,
  OUT UINT32                          *ResultArg3
  )
{
  UINT64   StartTime;
  UINT32   IntrStatus;
  UINT32   EnabledIntrStatus;
  UINT32   IntrMask;
  UINT32   CmdResultArg2;
  UINT32   FatalMask;
  BOOLEAN  SawUicError;

  // Check if controller is ready
  if (!SpacemitK3UfsReadyForUicCmd (Private)) {
    DEBUG ((DEBUG_ERROR, "UFS: Controller not ready to accept UIC commands\n"));
    return EFI_NOT_READY;
  }

  // Write Args
  SpacemitK3UfsWriteReg32 (Private, Argument1, REG_UIC_COMMAND_ARG_1);
  SpacemitK3UfsWriteReg32 (Private, Argument2, REG_UIC_COMMAND_ARG_2);
  SpacemitK3UfsWriteReg32 (Private, Argument3, REG_UIC_COMMAND_ARG_3);

  // Write UIC Cmd
  SpacemitK3UfsWriteReg32 (
    Private,
    Command & COMMAND_OPCODE_MASK,
    REG_UIC_COMMAND
    );

  // Setup interrupt mask
  IntrMask    = UFSHCD_UIC_MASK | UFSHCD_ERROR_MASK;
  FatalMask   = UFSHCD_ERROR_MASK & ~UIC_ERROR;
  SawUicError = FALSE;

  // Poll for completion
  StartTime = GetPerformanceCounter ();
  do {
    IntrStatus        = SpacemitK3UfsReadReg32 (Private, REG_INTERRUPT_STATUS);
    EnabledIntrStatus = IntrStatus & IntrMask;

    // Clear interrupt status
    if (IntrStatus) {
      SpacemitK3UfsWriteReg32 (Private, IntrStatus, REG_INTERRUPT_STATUS);
    }

    // Fatal host errors are always hard failures.
    if ((EnabledIntrStatus & FatalMask) != 0) {
      DEBUG ((
        DEBUG_ERROR,
        "UFS: fatal interrupt status 0x%x while waiting UIC cmd 0x%x\n",
        EnabledIntrStatus,
        Command
        ));
      SpacemitK3UfsDumpUicErrorRegs (
        Private,
        EnabledIntrStatus,
        "fatal_uic_error"
        );
      return EFI_DEVICE_ERROR;
    }

    // UIC_ERROR can be raised together with UIC completion (e.g. LINERESET).
    if ((EnabledIntrStatus & UIC_ERROR) != 0) {
      SawUicError = TRUE;
    }

    // Check if UIC command completed
    if (EnabledIntrStatus & UFSHCD_UIC_MASK) {
      break;
    }

    // Check timeout
    if (GetTimeInNanoSecond (GetPerformanceCounter () - StartTime) >
        (UFS_UIC_CMD_TIMEOUT_MS * 1000000ULL))
    {
      DEBUG ((DEBUG_ERROR, "UFS: Timedout waiting for UIC response\n"));
      if (SawUicError) {
        SpacemitK3UfsDumpUicErrorRegs (
          Private,
          EnabledIntrStatus,
          "uic_timeout_after_error"
          );
      }

      return EFI_TIMEOUT;
    }

    gBS->Stall (1);
  } while (TRUE);

  // Read results
  CmdResultArg2 = SpacemitK3UfsReadReg32 (Private, REG_UIC_COMMAND_ARG_2);
  if (ResultArg2 != NULL) {
    *ResultArg2 = CmdResultArg2;
  }

  if (ResultArg3 != NULL) {
    *ResultArg3 = SpacemitK3UfsReadReg32 (Private, REG_UIC_COMMAND_ARG_3);
  }

  if ((CmdResultArg2 & 0xFF) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: UIC command 0x%x failed, ARG2=0x%x\n",
      Command,
      CmdResultArg2
      ));
    if (SawUicError) {
      SpacemitK3UfsDumpUicErrorRegs (Private, UIC_ERROR, "uic_cmd_result_error");
    }

    return EFI_DEVICE_ERROR;
  }

  if (SawUicError) {
    // Keep this non-fatal when UIC command result reports success.
    SpacemitK3UfsDumpUicErrorRegs (
      Private,
      UIC_ERROR,
      "uic_error_with_success_result"
      );
  }

  return EFI_SUCCESS;
}

/**
  UIC DME SET command

  @param[in]  Private  Pointer to private data
  @param[in]  UicArg1  UIC ARG1 value (attribute selector)
  @param[in]  Value    Value to set

  @retval EFI_SUCCESS  Command executed successfully
  @retval Others       Error occurred
**/
EFI_STATUS
SpacemitK3UfsDmeSet (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  IN UINT32                           Value
  )
{
  EFI_STATUS  Status;
  UINT32      ResultArg2;
  UINT32      Retries;

  // No retry for non-peer commands
  Retries = 1;

  do {
    Status = SpacemitK3UfsSendUicCmd (
               Private,
               UIC_CMD_DME_SET,
               UicArg1,
               0,
               Value,
               &ResultArg2,
               NULL
               );

    Retries--;
  } while (EFI_ERROR (Status) && (Retries > 0));

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: dme-set attr-id 0x%x val 0x%x failed\n",
      (UicArg1 >> 16) & 0xFFFF,
      Value
      ));
  }

  return Status;
}

/**
  UIC DME GET command

  @param[in]  Private  Pointer to private data
  @param[in]  UicArg1  UIC ARG1 value (attribute selector)
  @param[out] Value    Pointer to receive value

  @retval EFI_SUCCESS  Command executed successfully
  @retval Others       Error occurred
**/
EFI_STATUS
SpacemitK3UfsDmeGet (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  OUT UINT32                          *Value
  )
{
  EFI_STATUS  Status;
  UINT32      ResultArg2;
  UINT32      ResultArg3;
  UINT32      Retries;

  if (Value == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // No retry for non-peer commands
  Retries = 1;

  do {
    Status = SpacemitK3UfsSendUicCmd (
               Private,
               UIC_CMD_DME_GET,
               UicArg1,
               0,
               0,
               &ResultArg2,
               &ResultArg3
               );

    Retries--;
  } while (EFI_ERROR (Status) && (Retries > 0));

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: dme-get attr-id 0x%x failed\n",
      (UicArg1 >> 16) & 0xFFFF
      ));
  }

  // Return value in argument3
  if (!EFI_ERROR (Status)) {
    *Value = ResultArg3;
  }

  return Status;
}

/**
  UIC DME PEER GET command

  @param[in]  Private  Pointer to private data
  @param[in]  UicArg1  UIC ARG1 value (attribute selector)
  @param[out] Value    Value read

  @retval EFI_SUCCESS  Command executed successfully
  @retval Others       Error occurred
**/
STATIC
EFI_STATUS
SpacemitK3UfsDmePeerGet (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           UicArg1,
  OUT UINT32                          *Value
  )
{
  EFI_STATUS  Status;
  UINT32      ResultArg2;
  UINT32      ResultArg3;
  UINT32      Retries;

  Retries = 1;
  do {
    Status = SpacemitK3UfsSendUicCmd (
               Private,
               UIC_CMD_DME_PEER_GET,
               UicArg1,
               0,
               0,
               &ResultArg2,
               &ResultArg3
               );

    Retries--;
  } while (EFI_ERROR (Status) && (Retries > 0));

  if (!EFI_ERROR (Status)) {
    *Value = ResultArg3;
  }

  return Status;
}

STATIC
UINT8
SpacemitK3UfsGetUpmcrs (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  return (UINT8)((SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_STATUS) >> 8) &
                 0x7);
}

STATIC
EFI_STATUS
SpacemitK3UfsUicPwrCtrl (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT32                           Command,
  IN UINT32                           Argument1,
  IN UINT32                           Argument2,
  IN UINT32                           Argument3,
  OUT UINT32                          *ResultArg2,
  OUT UINT32                          *ResultArg3
  )
{
  UINT64   StartTime;
  UINT32   IntrStatus;
  UINT32   EnabledIntrStatus;
  UINT32   IntrMask;
  UINT8    Status;
  BOOLEAN  TimedOut;

  if (!SpacemitK3UfsReadyForUicCmd (Private)) {
    DEBUG ((DEBUG_ERROR, "UFS: Controller not ready to accept UIC commands\n"));
    return EFI_NOT_READY;
  }

  // Write Args
  SpacemitK3UfsWriteReg32 (Private, Argument1, REG_UIC_COMMAND_ARG_1);
  SpacemitK3UfsWriteReg32 (Private, Argument2, REG_UIC_COMMAND_ARG_2);
  SpacemitK3UfsWriteReg32 (Private, Argument3, REG_UIC_COMMAND_ARG_3);

  // Write UIC Cmd
  SpacemitK3UfsWriteReg32 (
    Private,
    Command & COMMAND_OPCODE_MASK,
    REG_UIC_COMMAND
    );

  IntrMask = UFSHCD_UIC_PWR_MASK | UFSHCD_ERROR_MASK;

  TimedOut  = FALSE;
  StartTime = GetPerformanceCounter ();
  do {
    IntrStatus        = SpacemitK3UfsReadReg32 (Private, REG_INTERRUPT_STATUS);
    EnabledIntrStatus = IntrStatus & IntrMask;

    if (IntrStatus) {
      SpacemitK3UfsWriteReg32 (Private, IntrStatus, REG_INTERRUPT_STATUS);
    }

    if (GetTimeInNanoSecond (GetPerformanceCounter () - StartTime) >
        (UFS_UIC_CMD_TIMEOUT_MS * 1000000ULL))
    {
      DEBUG ((
        DEBUG_ERROR,
        "UFS: power ctrl cmd 0x%x timeout, arg3=0x%x\n",
        Command,
        Argument3
        ));
      TimedOut = TRUE;
      break;
    }

    if (EnabledIntrStatus & UFSHCD_ERROR_MASK) {
      DEBUG ((DEBUG_ERROR, "UFS: Error in status:0x%x\n", EnabledIntrStatus));
      return EFI_DEVICE_ERROR;
    }

    gBS->Stall (1);
  } while ((EnabledIntrStatus & UIC_POWER_MODE) == 0);

  if (ResultArg2) {
    *ResultArg2 = SpacemitK3UfsReadReg32 (Private, REG_UIC_COMMAND_ARG_2);
  }

  if (ResultArg3) {
    *ResultArg3 = SpacemitK3UfsReadReg32 (Private, REG_UIC_COMMAND_ARG_3);
  }

  Status = SpacemitK3UfsGetUpmcrs (Private);
  if (Status != PWR_LOCAL) {
    DEBUG (
      (DEBUG_ERROR, "UFS: power mode change failed, upmcrs=0x%x\n", Status));
    return EFI_DEVICE_ERROR;
  }

  if (TimedOut) {
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsUicChangePwrMode (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UINT8                            Mode
  )
{
  return SpacemitK3UfsUicPwrCtrl (
           Private,
           UIC_CMD_DME_SET,
           UIC_ARG_MIB (PA_PWRMODE),
           0,
           Mode,
           NULL,
           NULL
           );
}

STATIC
EFI_STATUS
SpacemitK3UfsGetMaxPwrMode (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  OUT UFS_PA_LAYER_ATTR               *PwrInfo
  )
{
  EFI_STATUS  Status;
  UINT32      Value;

  if (PwrInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (PwrInfo, sizeof (*PwrInfo));

  PwrInfo->PwrTx  = FAST_MODE;
  PwrInfo->PwrRx  = FAST_MODE;
  PwrInfo->HsRate = PA_HS_MODE_B;

  Status = SpacemitK3UfsDmeGet (
             Private,
             UIC_ARG_MIB (PA_CONNECTEDRXDATALANES),
             &Value
             );
  if (EFI_ERROR (Status) || (Value == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: invalid connected RX lanes, status=%r val=%u\n",
      Status,
      Value
      ));
    return EFI_DEVICE_ERROR;
  }

  PwrInfo->LaneRx = (UINT8)Value;
  if (PwrInfo->LaneRx > UFS_SPACEMIT_K3_LIMIT_NUM_LANES_RX) {
    PwrInfo->LaneRx = UFS_SPACEMIT_K3_LIMIT_NUM_LANES_RX;
  }

  Status = SpacemitK3UfsDmeGet (
             Private,
             UIC_ARG_MIB (PA_CONNECTEDTXDATALANES),
             &Value
             );
  if (EFI_ERROR (Status) || (Value == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: invalid connected TX lanes, status=%r val=%u\n",
      Status,
      Value
      ));
    return EFI_DEVICE_ERROR;
  }

  PwrInfo->LaneTx = (UINT8)Value;
  if (PwrInfo->LaneTx > UFS_SPACEMIT_K3_LIMIT_NUM_LANES_TX) {
    PwrInfo->LaneTx = UFS_SPACEMIT_K3_LIMIT_NUM_LANES_TX;
  }

  Status = SpacemitK3UfsDmeGet (Private, UIC_ARG_MIB (PA_MAXRXHSGEAR), &Value);
  if (EFI_ERROR (Status) || (Value == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: invalid PA_MAXRXHSGEAR, status=%r val=%u\n",
      Status,
      Value
      ));
    return EFI_DEVICE_ERROR;
  }

  PwrInfo->GearRx = (UINT8)Value;
  if (PwrInfo->GearRx > UFS_SPACEMIT_K3_LIMIT_HSGEAR_RX) {
    PwrInfo->GearRx = UFS_SPACEMIT_K3_LIMIT_HSGEAR_RX;
  }

  Status =
    SpacemitK3UfsDmePeerGet (Private, UIC_ARG_MIB (PA_MAXRXHSGEAR), &Value);
  if (EFI_ERROR (Status) || (Value == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: invalid peer PA_MAXRXHSGEAR, status=%r val=%u\n",
      Status,
      Value
      ));
    return EFI_DEVICE_ERROR;
  }

  PwrInfo->GearTx = (UINT8)Value;
  if (PwrInfo->GearTx > UFS_SPACEMIT_K3_LIMIT_HSGEAR_TX) {
    PwrInfo->GearTx = UFS_SPACEMIT_K3_LIMIT_HSGEAR_TX;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsChangePowerMode (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UFS_PA_LAYER_ATTR                *PwrMode
  )
{
  EFI_STATUS  Status;
  UINT8       PwrModeCode;

  if (PwrMode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SpacemitK3UfsDmeSetChecked (
             Private,
             UIC_ARG_MIB (PA_RXGEAR),
             PwrMode->GearRx,
             "PA_RXGEAR"
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status =
    SpacemitK3UfsDmeSetChecked (
      Private,
      UIC_ARG_MIB (PA_ACTIVERXDATALANES),
      PwrMode->LaneRx,
      "PA_ACTIVERXDATALANES"
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpacemitK3UfsDmeSetChecked (
             Private,
             UIC_ARG_MIB (PA_RXTERMINATION),
             1,
             "PA_RXTERMINATION"
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpacemitK3UfsDmeSetChecked (
             Private,
             UIC_ARG_MIB (PA_TXGEAR),
             PwrMode->GearTx,
             "PA_TXGEAR"
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status =
    SpacemitK3UfsDmeSetChecked (
      Private,
      UIC_ARG_MIB (PA_ACTIVETXDATALANES),
      PwrMode->LaneTx,
      "PA_ACTIVETXDATALANES"
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpacemitK3UfsDmeSetChecked (
             Private,
             UIC_ARG_MIB (PA_TXTERMINATION),
             1,
             "PA_TXTERMINATION"
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpacemitK3UfsDmeSetChecked (
             Private,
             UIC_ARG_MIB (PA_HSSERIES),
             PwrMode->HsRate,
             "PA_HSSERIES"
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PwrModeCode = (UINT8)((PwrMode->PwrRx << 4) | PwrMode->PwrTx);
  Status      = SpacemitK3UfsUicChangePwrMode (Private, PwrModeCode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UFS: power mode change failed: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsPwrChangeNotifyPost (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  gBS->Stall (UFS_K3_POST_POWER_MODE_DELAY_US);
  Status = SpacemitK3UfsWaitMphyPllLock (Private, "pwr_change_notify");
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->Stall (UFS_K3_POST_PLL_LOCK_SETTLE_US);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsApplyDevQuirks (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  gBS->Stall (5000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (TX_LCC_ENABLE, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)),
    0,
    "TX_LCC_ENABLE_TX0"
    );
  gBS->Stall (1000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (TX_LCC_ENABLE, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (1)),
    0,
    "TX_LCC_ENABLE_TX1"
    );

  gBS->Stall (1000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (TX_MIN_ACTIVATETIME, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)),
    0x0,
    "TX_MIN_ACTIVATETIME_TX0"
    );
  gBS->Stall (1000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (TX_MIN_ACTIVATETIME, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (1)),
    0x0,
    "TX_MIN_ACTIVATETIME_TX1"
    );
  gBS->Stall (10000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (ANA_HSGEAR_CTRL_ATTR),
    0x25,
    "ANA_HSGEAR_CTRL_ATTR"
    );
  gBS->Stall (10000);

  Status = SpacemitK3UfsWaitMphyPllLock (Private, "apply_dev_quirks");
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsSetPowerMode (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS         Status;
  UFS_PA_LAYER_ATTR  PwrInfo;

  Status = SpacemitK3UfsGetMaxPwrMode (Private, &PwrInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UFS: failed getting max power mode: %r\n", Status));
    return Status;
  }

  Status = SpacemitK3UfsChangePowerMode (Private, &PwrInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UFS: failed setting power mode: %r\n", Status));
    return Status;
  }

  Status = SpacemitK3UfsPwrChangeNotifyPost (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  M-PHY initialization

  @param[in] Private  Pointer to private data

  @retval EFI_SUCCESS  Success
  @retval Others       Failure
**/
EFI_STATUS
SpacemitK3ArasanUfsMphyInit (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  SpacemitK3UfsWriteReg32 (
    Private,
    0x003,
    Private->Priv.PhyMngBase + UFS_MPHY_RST_CTRL
    );
  gBS->Stall (1000);

  SpacemitK3UfsWriteReg32 (
    Private,
    MPHY_PU_ALL,
    Private->Priv.PhyMngBase + UFS_MPHY_PU_CTRL
    );
  gBS->Stall (1000);

  SpacemitK3UfsWriteReg32 (
    Private,
    MPHY_PU_WITH_HB8_RESET,
    Private->Priv.PhyMngBase + UFS_MPHY_PU_CTRL
    );
  gBS->Stall (1000);

  SpacemitK3UfsWriteReg32 (
    Private,
    MPHY_PU_ALL,
    Private->Priv.PhyMngBase + UFS_MPHY_PU_CTRL
    );
  gBS->Stall (1000);

  SpacemitK3UfsWriteReg32 (
    Private,
    MPHY_DEVICE_RESET_DEASSERT,
    Private->Priv.PhyMngBase + UFS_DEVICE_IO_CTRL
    );
  gBS->Stall (1000);

  Status = SpacemitK3UfsWaitMphyPllLock (Private, "mphy_init");
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SpacemitK3UfsWriteReg32 (
    Private,
    0x1,
    Private->Priv.PhyMngBase + UFS_MPHY_BKDR_CTRL
    );
  gBS->Stall (UFS_MPHY_TX_GEAR_SWITCH_DELAY_US);

  SpacemitK3UfsWriteReg32 (
    Private,
    0x00,
    Private->Priv.AtopBase + (ANA_HSGEAR_CTRL_ATTR << 2)
    );
  SpacemitK3UfsWriteReg32 (Private, 0x00, Private->Priv.AtopBase + (0xC2 << 2));
  gBS->Stall (UFS_MPHY_TX_GEAR_SWITCH_DELAY_US);

  SpacemitK3UfsWriteReg32 (
    Private,
    0x0,
    Private->Priv.PhyMngBase + UFS_MPHY_BKDR_CTRL
    );
  gBS->Stall (UFS_MPHY_TX_GEAR_SWITCH_DELAY_US);

  gBS->Stall (UFS_MPHY_TUNING_SETTLE_US);
  return EFI_SUCCESS;
}

/**
  UniPro initialization

  @param[in] Private  Pointer to private data

  @retval EFI_SUCCESS  Success
**/
EFI_STATUS
SpacemitK3ArasanUfsUniproInit (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXHSG1SYNCLENGTH),
    0x4f,
    "PA_TXHSG1SYNCLENGTH"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXHSG1PREPARELENGTH),
    0xf,
    "PA_TXHSG1PREPARELENGTH"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXHSG2SYNCLENGTH),
    0x4f,
    "PA_TXHSG2SYNCLENGTH"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXHSG2PREPARELENGTH),
    0xf,
    "PA_TXHSG2PREPARELENGTH"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXHSG3SYNCLENGTH),
    0x4f,
    "PA_TXHSG3SYNCLENGTH"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXHSG3PREPARELENGTH),
    0xf,
    "PA_TXHSG3PREPARELENGTH"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXMK2EXTENSION),
    0x0,
    "PA_TXMK2EXTENSION"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_PEERSCRAMBLING),
    0x1,
    "PA_PEERSCRAMBLING"
    );
  SpacemitK3UfsDmeSetLog (Private, UIC_ARG_MIB (PA_TXSKIP), 0x1, "PA_TXSKIP");
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXSKIPPERIOD),
    250,
    "PA_TXSKIPPERIOD"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_LOCAL_TX_LCC_ENABLE),
    0x0,
    "PA_LOCAL_TX_LCC_ENABLE"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_PEER_TX_LCC_ENABLE),
    0x0,
    "PA_PEER_TX_LCC_ENABLE"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_SCRAMBLING),
    0x1,
    "PA_SCRAMBLING"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_GRANULARITY),
    0x1,
    "PA_GRANULARITY"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_MK2EXTENSIONGUARDBAND),
    0x0,
    "PA_MK2EXTENSIONGUARDBAND"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_STALLNOCONFIGTIME),
    15,
    "PA_STALLNOCONFIGTIME"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TACTIVATE),
    0x64,
    "PA_TACTIVATE"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (PA_TXTRAILINGCLOCKS),
    0x64,
    "PA_TXTRAILINGCLOCKS"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (RX_LS_PRE_LEN_CAP, 4),
    0x0B,
    "RX_LS_PREPARELEN_TIME_RX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (RX_LS_PRE_LEN_CAP, 5),
    0x0B,
    "RX_LS_PREPARELEN_TIME_RX1"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_LANE_HB8_BKDOOR_ATTR,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (0)
      ),
    0x9F,
    "RX_HIBERNATE_BKEN_RX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_LANE_HB8_BKDOOR_ATTR,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (1)
      ),
    0x9F,
    "RX_HIBERNATE_BKEN_RX1"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_PWRM_CLOSURE_LEN_CAP,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (0)
      ),
    15,
    "RX_PWRM_CLOSURE_LEN_CAP_RX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_PWRM_CLOSURE_LEN_CAP,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (1)
      ),
    15,
    "RX_PWRM_CLOSURE_LEN_CAP_RX1"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (RX_MIN_STALL_CAP, UIC_ARG_MPHY_RX_GEN_SEL_INDEX (0)),
    0xFF,
    "RX_MIN_STALL_CAP_RX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (RX_MIN_STALL_CAP, UIC_ARG_MPHY_RX_GEN_SEL_INDEX (1)),
    0xFF,
    "RX_MIN_STALL_CAP_RX1"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      TX_HIBERN8TIME_CAPABILITY,
      UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)
      ),
    0x64,
    "TX_HIBERN8TIME_CAPABILITY_TX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      TX_HIBERN8TIME_CAPABILITY,
      UIC_ARG_MPHY_TX_GEN_SEL_INDEX (1)
      ),
    0x64,
    "TX_HIBERN8TIME_CAPABILITY_TX1"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_HIBERN8TIME_CAPABILITY,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (0)
      ),
    0x64,
    "RX_HIBERN8TIME_CAPABILITY_RX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_HIBERN8TIME_CAPABILITY,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (1)
      ),
    0x64,
    "RX_HIBERN8TIME_CAPABILITY_RX1"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (ANA_EQ_CTRL_REG_ATTR, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)),
    0x5,
    "ANA_EQ_CTRL_REG_ATTR_TX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_GARBAGE_COUNT_OFFSET,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (0)
      ),
    0x9F,
    "RX_GARBAGE_COUNT_OFFSET_RX0"
    );
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (
      RX_GARBAGE_COUNT_OFFSET,
      UIC_ARG_MPHY_RX_GEN_SEL_INDEX (1)
      ),
    0x9F,
    "RX_GARBAGE_COUNT_OFFSET_RX1"
    );

  SpacemitK3UfsDmeSetLog (Private, UIC_ARG_MIB (0xfc), 0xfc, "PHY_ECO_BYPASS");
  return EFI_SUCCESS;
}

/**
  Silent reset

  @param[in] Private  Pointer to private data

  @retval EFI_SUCCESS  Success
**/
EFI_STATUS
SpacemitK3ArasanUfsSilentReset (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  SpacemitK3UfsWriteReg32 (
    Private,
    0x000,
    Private->Priv.PhyMngBase + UFS_DEVICE_IO_CTRL
    );
  gBS->Stall (UFS_K3_SILENT_RESET_DELAY_US);

  SpacemitK3UfsWriteReg32 (
    Private,
    0x000,
    Private->Priv.PhyMngBase + UFS_MPHY_RST_CTRL
    );
  gBS->Stall (UFS_K3_SILENT_RESET_DELAY_US);

  SpacemitK3UfsWriteReg32 (
    Private,
    0x000,
    Private->Priv.PhyMngBase + UFS_MPHY_PU_CTRL
    );
  gBS->Stall (UFS_K3_SILENT_RESET_DELAY_US);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsGetConnectedTxLanes (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  OUT UINT32                          *TxLanes
  )
{
  EFI_STATUS  Status;

  if (TxLanes == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SpacemitK3UfsDmeGet (
             Private,
             UIC_ARG_MIB (PA_CONNECTEDTXDATALANES),
             TxLanes
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: failed to read PA_CONNECTEDTXDATALANES: %r\n",
      Status
      ));
  }

  return Status;
}

STATIC
EFI_STATUS
SpacemitK3UfsLinkStartupPreChange (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  UINT32  Sys1Clk1Us;
  UINT32  RegVal;

  Sys1Clk1Us = SpacemitK3UfsGetSys1clk1us (Private);
  if (Sys1Clk1Us == 0) {
    Sys1Clk1Us = UFS_SYS1CLK_1US_409MHZ;
  }

  SpacemitK3UfsWriteReg32 (Private, Sys1Clk1Us, UFS_SYS1CLK_1US_REG);
  SpacemitK3UfsWriteReg32 (
    Private,
    SpacemitK3UfsGetTxSymbolClkNsUs (Sys1Clk1Us),
    UFS_TX_SYMBOL_CLK_NS_US_REG
    );
  RegVal  = Sys1Clk1Us * 100000U;
  RegVal &= ~0xFU;
  SpacemitK3UfsWriteReg32 (Private, RegVal, UFS_PA_LINK_STARTUP_TIMER_REG);
  gBS->Stall (5000);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsLinkStartupPostChange (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;
  UINT32      RegVal;

  RegVal = SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_STATUS);
  if ((RegVal & DEVICE_PRESENT) == 0) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: DEVICE_PRESENT not set after link startup, HCS=0x%x\n",
      RegVal
      ));
    return EFI_NO_MEDIA;
  }

  RegVal = SpacemitK3UfsReadReg32 (Private, REG_INTERRUPT_STATUS);
  if ((RegVal & (UIC_LINK_STARTUP | UIC_ERROR)) != 0) {
    SpacemitK3UfsWriteReg32 (
      Private,
      RegVal & (UIC_LINK_STARTUP | UIC_ERROR),
      REG_INTERRUPT_STATUS
      );
  }

  // Align with Linux flow: clear UECPA once after link startup LINERESET.
  (VOID)SpacemitK3UfsReadReg32 (Private, REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER);

  gBS->Stall (5000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (0xe8, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)),
    0x97,
    "TX_BACKDOOR_0xE8_STEP1"
    );
  gBS->Stall (1000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (0xe8, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)),
    0xd7,
    "TX_BACKDOOR_0xE8_STEP2"
    );
  gBS->Stall (1000);
  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB_SEL (0xe8, UIC_ARG_MPHY_TX_GEN_SEL_INDEX (0)),
    0x17,
    "TX_BACKDOOR_0xE8_STEP3"
    );

  SpacemitK3UfsDmeSetLog (
    Private,
    UIC_ARG_MIB (DL_AFC0REQTIMEOUTVAL),
    UFS_DL_AFC0REQTIMEOUTVAL_MAX,
    "DL_AFC0REQTIMEOUTVAL"
    );

  Status =
    SpacemitK3UfsGetConnectedTxLanes (Private, &Private->ConnectedTxLanes);
  if (EFI_ERROR (Status)) {
    // Linux flow does not make this read fatal for link startup.
    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  Link startup notification

  @param[in] Private  Pointer to private data
  @param[in] Status   PRE_CHANGE or POST_CHANGE

  @retval EFI_SUCCESS  Success
  @retval Others       Error
**/
EFI_STATUS
SpacemitK3ArasanUfsLinkStartupNotify (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UFS_NOTIFY_CHANGE_STATUS         Status
  )
{
  if (Status == PRE_CHANGE) {
    return SpacemitK3UfsLinkStartupPreChange (Private);
  }

  if (Status == POST_CHANGE) {
    return SpacemitK3UfsLinkStartupPostChange (Private);
  }

  return EFI_SUCCESS;
}

/**
  HCE enable notification

  @param[in] Private  Pointer to private data
  @param[in] Status   PRE_CHANGE or POST_CHANGE

  @retval EFI_SUCCESS  Success
**/
EFI_STATUS
SpacemitK3ArasanUfsHceEnableNotify (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private,
  IN UFS_NOTIFY_CHANGE_STATUS         Status
  )
{
  UINT64      StartTime;
  UINT32      RegVal;
  EFI_STATUS  Ret;

  if (Status == PRE_CHANGE) {
    if (!Private->FirstHceDone) {
      Private->FirstHceDone = TRUE;
      return EFI_SUCCESS;
    }

    RegVal = SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_ENABLE);
    if ((RegVal & CONTROLLER_ENABLE) == 0) {
      return EFI_SUCCESS;
    }

    SpacemitK3UfsWriteReg32 (Private, 0, REG_CONTROLLER_ENABLE);

    StartTime = GetPerformanceCounter ();
    do {
      RegVal = SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_ENABLE);
      if ((RegVal & CONTROLLER_ENABLE) == 0) {
        break;
      }

      if (GetTimeInNanoSecond (GetPerformanceCounter () - StartTime) >
          (UFS_UIC_CMD_TIMEOUT_MS * 1000000ULL))
      {
        DEBUG ((
          DEBUG_ERROR,
          "UFS: timeout waiting HCE disable, REG_CONTROLLER_ENABLE=0x%x\n",
          RegVal
          ));
        return EFI_TIMEOUT;
      }

      gBS->Stall (1);
    } while (TRUE);

    return EFI_SUCCESS;
  }

  if (Status != POST_CHANGE) {
    return EFI_SUCCESS;
  }

  Ret = SpacemitK3ArasanUfsMphyInit (Private);
  if (EFI_ERROR (Ret)) {
    return Ret;
  }

  Ret = SpacemitK3ArasanUfsUniproInit (Private);
  if (EFI_ERROR (Ret)) {
    return Ret;
  }

  SpacemitK3UfsWriteReg32 (Private, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SpacemitK3UfsPhyInitialization (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  (VOID)Private;
  return EFI_SUCCESS;
}

/**
  UFS initialization

  @param[in] Private  Pointer to private data

  @retval EFI_SUCCESS  Success
**/
EFI_STATUS
SpacemitK3ArasanUfsInit (
  IN SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  Private->FirstHceDone     = FALSE;
  Private->ConnectedTxLanes = 0;

  Status = SpacemitK3UfsClkEnable (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SpacemitK3UfsPhyInitialization (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

//
// EDKII_UFS_HOST_CONTROLLER_PROTOCOL implementation
//

EFI_STATUS
EFIAPI
SpacemitK3UfsHcGetMmioBar (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL  *This,
  OUT UINTN                              *MmioBar
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;

  if ((This == NULL) || (MmioBar == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private  = SPACEMIT_K3_UFS_HC_FROM_THIS (This);
  *MmioBar = Private->UfsHcBase;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcRead (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL        *This,
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL_WIDTH  Width,
  IN UINT64                                    Offset,
  IN UINTN                                     Count,
  IN OUT VOID                                  *Buffer
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  UINTN                            Index;
  UINTN                            Address;

  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (This);
  Address = Private->UfsHcBase + Offset;

  for (Index = 0; Index < Count; Index++) {
    switch (Width) {
      case EfiUfsHcWidthUint8:
        ((UINT8 *)Buffer)[Index] = MmioRead8 (Address);
        Address                 += 1;
        break;
      case EfiUfsHcWidthUint16:
        ((UINT16 *)Buffer)[Index] = MmioRead16 (Address);
        Address                  += 2;
        break;
      case EfiUfsHcWidthUint32:
        ((UINT32 *)Buffer)[Index] = MmioRead32 (Address);
        Address                  += 4;
        break;
      case EfiUfsHcWidthUint64:
        ((UINT64 *)Buffer)[Index] = MmioRead64 (Address);
        Address                  += 8;
        break;
      default:
        return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcWrite (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL        *This,
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL_WIDTH  Width,
  IN UINT64                                    Offset,
  IN UINTN                                     Count,
  IN OUT VOID                                  *Buffer
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  UINTN                            Index;
  UINTN                            Address;

  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (This);
  Address = Private->UfsHcBase + Offset;

  for (Index = 0; Index < Count; Index++) {
    switch (Width) {
      case EfiUfsHcWidthUint8:
        MmioWrite8 (Address, ((UINT8 *)Buffer)[Index]);
        Address += 1;
        break;
      case EfiUfsHcWidthUint16:
        MmioWrite16 (Address, ((UINT16 *)Buffer)[Index]);
        Address += 2;
        break;
      case EfiUfsHcWidthUint32:
        MmioWrite32 (Address, ((UINT32 *)Buffer)[Index]);
        Address += 4;
        break;
      case EfiUfsHcWidthUint64:
        MmioWrite64 (Address, ((UINT64 *)Buffer)[Index]);
        Address += 8;
        break;
      default:
        return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcAllocateBuffer (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL  *This,
  IN EFI_ALLOCATE_TYPE                   Type,
  IN EFI_MEMORY_TYPE                     MemoryType,
  IN UINTN                               Pages,
  OUT VOID                               **HostAddress,
  IN UINT64                              Attributes
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  SPACEMIT_K3_UFS_DMA_ALLOC_INFO   *AllocInfo;
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             Memory;
  UINT64                           OriginalAttributes;

  if ((This == NULL) || (HostAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  (VOID)Attributes;

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (This);

  (VOID)Type;

  Memory = 0;
  Status = gBS->AllocatePages (AllocateAnyPages, MemoryType, Pages, &Memory);

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: AllocateBuffer failed Status=%r Pages=0x%lx MemoryType=%d\n",
      Status,
      Pages,
      MemoryType
      ));
    return Status;
  }

  *HostAddress = (VOID *)(UINTN)Memory;

  if (!FeaturePcdGet (PcdDmaCoherent)) {
    Status = SpacemitK3UfsSetDmaBufferUncached (
               Private,
               Memory,
               Pages,
               &OriginalAttributes
               );
    if (Status == EFI_SUCCESS) {
      AllocInfo = AllocateZeroPool (sizeof (*AllocInfo));
      if (AllocInfo == NULL) {
        DEBUG ((
          DEBUG_WARN,
          "UFS: failed to track uncached DMA buffer metadata\n"
          ));
      } else {
        AllocInfo->Signature          = SPACEMIT_K3_UFS_DMA_ALLOC_INFO_SIGNATURE;
        AllocInfo->HostAddress        = Memory;
        AllocInfo->Pages              = Pages;
        AllocInfo->OriginalAttributes = OriginalAttributes;
        InsertTailList (&Private->DmaAllocList, &AllocInfo->Link);
      }
    } else if (Status != EFI_ALREADY_STARTED) {
      DEBUG ((
        DEBUG_ERROR,
        "UFS: AllocateBuffer cannot provide coherent common buffer: %r\n",
        Status
        ));
      *HostAddress = NULL;
      (VOID)gBS->FreePages (Memory, Pages);
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcFreeBuffer (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL  *This,
  IN UINTN                               Pages,
  IN VOID                                *HostAddress
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  SPACEMIT_K3_UFS_DMA_ALLOC_INFO   *AllocInfo;

  if ((This == NULL) || (HostAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private   = SPACEMIT_K3_UFS_HC_FROM_THIS (This);
  AllocInfo = SpacemitK3UfsFindDmaAllocInfo (
                Private,
                (EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress
                );
  if (AllocInfo != NULL) {
    if (AllocInfo->Pages != Pages) {
      DEBUG ((
        DEBUG_WARN,
        "UFS: FreeBuffer page count mismatch Host=0x%lx tracked=0x%lx "
        "req=0x%lx\n",
        (UINTN)HostAddress,
        AllocInfo->Pages,
        Pages
        ));
    }

    RemoveEntryList (&AllocInfo->Link);
    SpacemitK3UfsRestoreDmaBufferAttributes (Private, AllocInfo);
    FreePool (AllocInfo);
  }

  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress, Pages);
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcMap (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL   *This,
  IN EDKII_UFS_HOST_CONTROLLER_OPERATION  Operation,
  IN VOID                                 *HostAddress,
  IN OUT UINTN                            *NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS                *DeviceAddress,
  OUT VOID                                **Mapping
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  SPACEMIT_K3_UFS_DMA_MAP_INFO     *MapInfo;
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             HostPhysical;
  EFI_PHYSICAL_ADDRESS             MappedDeviceAddress;
  UINTN                            MappedBytes;
  BOOLEAN                          Translated;
  UINT32                           Capabilities;

  if ((This == NULL) || (HostAddress == NULL) || (NumberOfBytes == NULL) ||
      (DeviceAddress == NULL) || (Mapping == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (Operation >= EdkiiUfsHcOperationMaximum) {
    return EFI_INVALID_PARAMETER;
  }

  if (*NumberOfBytes > SIZE_1MB) {
    return EFI_DEVICE_ERROR;
  }

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (This);
  MapInfo = AllocateZeroPool (sizeof (*MapInfo));
  if (MapInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  HostPhysical = (EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress;
  MappedBytes  = *NumberOfBytes;
  Status       = SpacemitK3UfsTranslateDmaAddress (
                   HostPhysical,
                   &MappedBytes,
                   &MappedDeviceAddress,
                   &Translated
                   );
  if (EFI_ERROR (Status)) {
    FreePool (MapInfo);
    return Status;
  }

  Capabilities = SpacemitK3UfsReadReg32 (Private, REG_CONTROLLER_CAPABILITIES);
  if (((Capabilities & UFS_HC_CAP_64ADDR) == 0) &&
      (MappedDeviceAddress + MappedBytes > 0x100000000ULL))
  {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: Map failed due to 32-bit DMA limit Device=0x%lx Bytes=0x%lx "
      "CAP=0x%08x\n",
      (UINTN)MappedDeviceAddress,
      MappedBytes,
      Capabilities
      ));
    FreePool (MapInfo);
    return EFI_DEVICE_ERROR;
  }

  if (!FeaturePcdGet (PcdDmaCoherent) && (Private->Cpu == NULL)) {
    FreePool (MapInfo);
    return EFI_DEVICE_ERROR;
  }

  if (!FeaturePcdGet (PcdDmaCoherent) && (Private->Cpu != NULL) &&
      (MappedBytes > 0))
  {
    Status = Private->Cpu->FlushDataCache (
                             Private->Cpu,
                             HostPhysical,
                             MappedBytes,
                             EfiCpuFlushTypeWriteBack
                             );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "UFS: Map cache writeback failed: %r\n", Status));
      FreePool (MapInfo);
      return Status;
    }
  }

  MapInfo->Signature     = SPACEMIT_K3_UFS_DMA_MAP_INFO_SIGNATURE;
  MapInfo->Operation     = Operation;
  MapInfo->HostAddress   = HostAddress;
  MapInfo->NumberOfBytes = MappedBytes;

  *DeviceAddress = MappedDeviceAddress;
  *NumberOfBytes = MappedBytes;
  *Mapping       = MapInfo;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcUnmap (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL  *This,
  IN VOID                                *Mapping
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  SPACEMIT_K3_UFS_DMA_MAP_INFO     *MapInfo;
  EFI_STATUS                       Status;

  if ((This == NULL) || (Mapping == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (This);
  MapInfo = (SPACEMIT_K3_UFS_DMA_MAP_INFO *)Mapping;
  if (MapInfo->Signature != SPACEMIT_K3_UFS_DMA_MAP_INFO_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  if (!FeaturePcdGet (PcdDmaCoherent) && (Private->Cpu == NULL)) {
    Status = EFI_DEVICE_ERROR;
  }

  if (!FeaturePcdGet (PcdDmaCoherent) && (Private->Cpu != NULL) &&
      (MapInfo->NumberOfBytes > 0) &&
      ((MapInfo->Operation == EdkiiUfsHcOperationBusMasterWrite) ||
       (MapInfo->Operation == EdkiiUfsHcOperationBusMasterCommonBuffer)))
  {
    Status = Private->Cpu->FlushDataCache (
                             Private->Cpu,
                             (EFI_PHYSICAL_ADDRESS)(UINTN)MapInfo->HostAddress,
                             MapInfo->NumberOfBytes,
                             EfiCpuFlushTypeInvalidate
                             );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "UFS: Unmap cache invalidate failed: %r\n", Status));
    }
  }

  FreePool (MapInfo);
  return Status;
}

EFI_STATUS
EFIAPI
SpacemitK3UfsHcFlush (
  IN EDKII_UFS_HOST_CONTROLLER_PROTOCOL  *This
  )
{
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (This);

  // Flush() is for posted bus writes; use ordering barriers + readback.
  MemoryFence ();
  (VOID)SpacemitK3UfsReadReg32 (Private, REG_INTERRUPT_STATUS);
  MemoryFence ();

  return EFI_SUCCESS;
}

/**
  Platform Callback function
  This function is called by UfsPassThruDxe at specific phases during
initialization and maps EDK2 phases to Linux variant-op style callbacks.

  @param[in]      ControllerHandle  Handle of the UFS controller
  @param[in]      CallbackPhase     Phase when callback is invoked
  @param[in, out] CallbackData      Phase-specific data

  @retval EFI_SUCCESS            Callback completed successfully
  @retval EFI_INVALID_PARAMETER  Invalid parameters
  @retval Others                 Function failed
**/
EFI_STATUS
EFIAPI
SpacemitK3UfsHcPlatformCallback (
  IN EFI_HANDLE  ControllerHandle,
  IN EDKII_UFS_HC_PLATFORM_CALLBACK_PHASE
  CallbackPhase,
  IN OUT VOID  *CallbackData
  )
{
  EFI_STATUS                       Status;
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;
  EDKII_UFS_HC_DRIVER_INTERFACE    *UfsHcDriver;

  (VOID)ControllerHandle;

  if (CallbackData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UfsHcDriver = (EDKII_UFS_HC_DRIVER_INTERFACE *)CallbackData;
  if (UfsHcDriver->UfsHcProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SPACEMIT_K3_UFS_HC_FROM_THIS (UfsHcDriver->UfsHcProtocol);

  switch (CallbackPhase) {
    case EdkiiUfsHcPreHce:
      Status = SpacemitK3ArasanUfsHceEnableNotify (Private, PRE_CHANGE);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "UFS: hce_enable_notify(PRE_CHANGE) failed: %r\n",
          Status
          ));
        return Status;
      }

      break;

    case EdkiiUfsHcPostHce:
      Status = SpacemitK3ArasanUfsHceEnableNotify (Private, POST_CHANGE);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "UFS: hce_enable_notify(POST_CHANGE) failed: %r\n",
          Status
          ));
        return Status;
      }

      Status =
        SpacemitK3UfsSetAclkRate (Private, SpacemitK3UfsGetTargetAclkRate ());
      if (EFI_ERROR (Status)) {
        return Status;
      }

      SpacemitK3UfsWriteReg32 (Private, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
      break;

    case EdkiiUfsHcPreLinkStartup:
      Status = SpacemitK3ArasanUfsLinkStartupNotify (Private, PRE_CHANGE);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "UFS: link_startup_notify(PRE_CHANGE) failed: %r\n",
          Status
          ));
        return Status;
      }

      break;

    case EdkiiUfsHcPostLinkStartup:
      Status = SpacemitK3ArasanUfsLinkStartupNotify (Private, POST_CHANGE);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "UFS: link_startup_notify(POST_CHANGE) failed: %r\n",
          Status
          ));
        return Status;
      }

      Status = SpacemitK3UfsApplyDevQuirks (Private);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "UFS: apply_dev_quirks failed: %r\n", Status));
        return Status;
      }

      Status = SpacemitK3UfsSetPowerMode (Private);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "UFS: set_power_mode failed: %r\n", Status));
        return Status;
      }

      break;

    default:
      DEBUG ((DEBUG_ERROR, "UFS: Unknown callback phase: %d\n", CallbackPhase));
      return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

STATIC
EDKII_UFS_CARD_REF_CLK_FREQ_ATTRIBUTE
SpacemitK3UfsGetRefclkAttr (
  IN UINT32  RefClkVal
  )
{
  if (RefClkVal <= EdkiiUfsCardRefClkFreqObsolete) {
    return (EDKII_UFS_CARD_REF_CLK_FREQ_ATTRIBUTE)RefClkVal;
  }

  switch (RefClkVal) {
    case 19200000:
      return EdkiiUfsCardRefClkFreq19p2Mhz;
    case 26000000:
      return EdkiiUfsCardRefClkFreq26Mhz;
    case 38400000:
      return EdkiiUfsCardRefClkFreq38p4Mhz;
    case 52000000:
      return EdkiiUfsCardRefClkFreqObsolete;
    default:
      DEBUG ((
        DEBUG_WARN,
        "UFS: invalid ref clock value %u, defaulting to 19.2MHz\n",
        RefClkVal
        ));
      return EdkiiUfsCardRefClkFreq19p2Mhz;
  }
}

/**
  Driver entry point

  @param[in] ImageHandle  Image handle
  @param[in] SystemTable  System table pointer

  @retval EFI_SUCCESS  Success
**/
EFI_STATUS
EFIAPI
SpacemitK3UfsHcDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                       Status;
  SPACEMIT_K3_UFS_HC_PRIVATE_DATA  *Private;

  // Allocate private data
  Private = AllocateZeroPool (sizeof (SPACEMIT_K3_UFS_HC_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature = SPACEMIT_K3_UFS_HC_SIGNATURE;
  InitializeListHead (&Private->DmaAllocList);

  // Initialize from PCDs
  Private->UfsHcBase       = PcdGet64 (PcdUfsHcBase);
  Private->Priv.PhyMngBase = PcdGet32 (PcdUfsPhyMngBase);
  Private->Priv.AtopBase   = PcdGet32 (PcdUfsAtopBase);

  // Ensure MMIO ranges are mapped before UfsPassThruDxe starts probing HC
  // registers.
  Status = SpacemitK3UfsMapMmioRegion (
             Private->UfsHcBase,
             SPACEMIT_K3_UFS_HC_MMIO_SIZE,
             "UFS_HC"
             );
  if (EFI_ERROR (Status)) {
    FreePool (Private);
    return Status;
  }

  Status = SpacemitK3UfsBuildDevicePath (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UFS: Failed to build HC device path: %r\n", Status));
    FreePool (Private);
    return Status;
  }

  Status = gBS->LocateProtocol (
                  &gSpacemitSiliconClockCtrlProtocolGuid,
                  NULL,
                  (VOID **)&Private->ClockCtrl
                  );
  if (EFI_ERROR (Status) || (Private->ClockCtrl == NULL)) {
    if (!EFI_ERROR (Status)) {
      Status = EFI_NOT_FOUND;
    }

    DEBUG ((
      DEBUG_ERROR,
      "UFS: Failed to locate ClockCtrl Protocol: %r\n",
      Status
      ));
    if (Private->DevicePath != NULL) {
      FreePool (Private->DevicePath);
    }

    FreePool (Private);
    return Status;
  }

  // Locate CPU Protocol for cache flush operations
  Status = gBS->LocateProtocol (
                  &gEfiCpuArchProtocolGuid,
                  NULL,
                  (VOID **)&Private->Cpu
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "UFS: Failed to locate CPU Protocol: %r (cache flush will be "
      "skipped)\n",
      Status
      ));
    Private->Cpu = NULL; // Continue without cache flush support
  }

  // Fill UFS Host Controller Protocol function pointers
  Private->UfsHc.GetUfsHcMmioBar = SpacemitK3UfsHcGetMmioBar;
  Private->UfsHc.AllocateBuffer  = SpacemitK3UfsHcAllocateBuffer;
  Private->UfsHc.FreeBuffer      = SpacemitK3UfsHcFreeBuffer;
  Private->UfsHc.Map             = SpacemitK3UfsHcMap;
  Private->UfsHc.Unmap           = SpacemitK3UfsHcUnmap;
  Private->UfsHc.Flush           = SpacemitK3UfsHcFlush;
  Private->UfsHc.Read            = SpacemitK3UfsHcRead;
  Private->UfsHc.Write           = SpacemitK3UfsHcWrite;

  // Fill UFS Platform Protocol
  // Version 2 supports all callback phases
  Private->UfsHcPlatform.Version        = EDKII_UFS_HC_PLATFORM_PROTOCOL_VERSION;
  Private->UfsHcPlatform.OverrideHcInfo = NULL; // Not needed for K3
  Private->UfsHcPlatform.Callback       = SpacemitK3UfsHcPlatformCallback;
  Private->UfsHcPlatform.RefClkFreq     =
    SpacemitK3UfsGetRefclkAttr (PcdGet32 (PcdUfsRefClkFreq));

  // Platform initialization with retry logic
  for (UINT32 Retries = 3; Retries > 0; Retries--) {
    Status = SpacemitK3ArasanUfsInit (Private);
    if (!EFI_ERROR (Status)) {
      break;
    }

    // Device reset before retry
    if (Retries > 1) {
      SpacemitK3ArasanUfsSilentReset (Private);
      gBS->Stall (1000); // 1ms delay between retries
    }
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UFS: Platform init failed after 3 retries: %r\n",
      Status
      ));
    (VOID)SpacemitK3UfsClkDisable (Private);
    if (Private->DevicePath != NULL) {
      FreePool (Private->DevicePath);
    }

    FreePool (Private);
    return Status;
  }

  // HCE enable notify & Link startup notify
  // UfsPassThruDxe requires UfsHostController Protocol (mandatory)
  // UfsPassThruDxe will locate Platform Protocol (optional, but we provide it)
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  Private->DevicePath,
                  &gEdkiiUfsHostControllerProtocolGuid,
                  &Private->UfsHc,
                  &gEdkiiUfsHcPlatformProtocolGuid,
                  &Private->UfsHcPlatform,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    (VOID)SpacemitK3UfsClkDisable (Private);
    if (Private->DevicePath != NULL) {
      FreePool (Private->DevicePath);
    }

    FreePool (Private);
    return Status;
  }

  Status = SpacemitK3UfsRegisterExtScsiPassThruNotify ();
  if (EFI_ERROR (Status) && (Status != EFI_ALREADY_STARTED)) {
    DEBUG ((
      DEBUG_WARN,
      "UFS: RegisterProtocolNotify(ExtScsiPassThru) failed: %r\n",
      Status
      ));
  }

  // Force immediate binding so UfsPassThruDxe executes UfsControllerInit path.
  Status = gBS->ConnectController (Private->ControllerHandle, NULL, NULL, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "UFS: ConnectController failed: %r\n", Status));
  }

  DEBUG ((DEBUG_INFO, "SPACEMIT K3 UFS HC Protocol installed successfully\n"));
  return EFI_SUCCESS;
}
