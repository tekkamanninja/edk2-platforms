/** @file

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Base.h>
#include <IndustryStandard/Atapi.h>
#include <IndustryStandard/Nvme.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/SortLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DiskInfoLib.h>
#include <Protocol/DiskInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/NvmExpressPassthru.h>

/**
  Internal function to eliminate the extra spaces in the String to one space.

  @param[in]  String    Pointer to unicode string.

**/
STATIC
VOID
EliminateExtraSpacesInternal (
  IN CHAR16   *String
  )
{
  UINTN  Index;
  UINTN  ActualIndex;

  if (String == NULL) {
    return;
  }

  for (Index = 0, ActualIndex = 0; String[Index] != L'\0'; Index++) {
    if ((String[Index] != L' ') || ((ActualIndex > 0) && (String[ActualIndex - 1] != L' '))) {
      String[ActualIndex++] = String[Index];
    }
  }

  String[ActualIndex] = L'\0';
  return;
}

/**
  Internal function to append disk information context to disk information array.

  @param[in out]  DiskCount     Number of entries in the disk information array.
  @param[in out]  DiskInfo      Pointer to disk information array.
  @param[in]      DiskContext   Pointer to disk information context.

**/
STATIC
VOID
AppendContextInternal (
  IN OUT UINTN              *DiskCount,
  IN OUT DISK_INFO_CONTEXT  **DiskInfo,
  IN     DISK_INFO_CONTEXT  *DiskContext
  )
{
  if ((DiskCount == NULL) || (DiskInfo == NULL) || (DiskContext == NULL)) {
    return;
  }

  if (*DiskCount == 0) {
    *DiskInfo = AllocateZeroPool (sizeof (DISK_INFO_CONTEXT));
  } else {
    *DiskInfo = ReallocatePool (
                            sizeof (DISK_INFO_CONTEXT) * (*DiskCount),
                            sizeof (DISK_INFO_CONTEXT) * (*DiskCount + 1),
                            *DiskInfo
                            );
  }

  if (*DiskInfo) {
    CopyMem (*DiskInfo + (*DiskCount)++, DiskContext, sizeof (DISK_INFO_CONTEXT));
  }

  return;
}

/**
  Sort two disk information context.

  @param[in]  Buffer1       Pointer to first context.
  @param[in]  Buffer2       Pointer to second context.

  @retval 0                 Buffer1 equal to Buffer2.
  @retval <0                Buffer1 is less than Buffer2.
  @retval >0                Buffer1 is greater than Buffer2.

**/
STATIC
INTN
EFIAPI
SortContextInternal (
  IN CONST VOID     *Buffer1,
  IN CONST VOID     *Buffer2
  )
{
  UINT8   DiskType1, DiskType2;

  DiskType1 = ((DISK_INFO_CONTEXT *) Buffer1)->DiskType;
  DiskType2 = ((DISK_INFO_CONTEXT *) Buffer2)->DiskType;

  if (DiskType1 < DiskType2) {
    return -1;
  }

  if (DiskType1 > DiskType2) {
    return 1;
  }

  return 0;
}

/**
  Get disk info context for AHCI drives.

  @param[in]  Handle        Device handle.
  @param[in]  Context       A pointer to disk info context data structure.
                            The caller is responsible to free context.

  @retval EFI_SUCCESS             Get disk info context successfully.
  @retval EFI_INVALID_PARAMETER   One of the input parameters is invalid.
  @retval EFI_OUT_OF_RESOURCES    Allocate buffer failed.
  @retval Others                  Other failures.

**/
STATIC
EFI_STATUS
AhciGetContextInternal (
  IN  EFI_HANDLE          Handle,
  OUT DISK_INFO_CONTEXT   **Context
  )
{
  EFI_STATUS              Status;
  UINTN                   Index;
  UINT32                  BufferSize;
  EFI_DISK_INFO_PROTOCOL  *DiskInfoProtocol;
  ATA_IDENTIFY_DATA       *IdentifyData;
  EFI_DEV_PATH_PTR        DevicePath;
  UINT64                  NumSectors;
  DISK_INFO_CONTEXT       *DiskContext;

  IdentifyData = NULL;
  DiskContext  = NULL;

  if ((Handle == NULL) || (Context == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Context     = NULL;

  Status = gBS->HandleProtocol (Handle, &gEfiDiskInfoProtocolGuid, (VOID **) &DiskInfoProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BufferSize = sizeof (*IdentifyData);
  IdentifyData = AllocateZeroPool (BufferSize);
  if (IdentifyData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DiskInfoProtocol->Identify (DiskInfoProtocol, (VOID *) IdentifyData, &BufferSize);
  if (EFI_ERROR (Status)) {
    goto FreeExit;
  }

  DiskContext = AllocateZeroPool (sizeof (*DiskContext));
  if (DiskContext == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeExit;
  }

  DiskContext->DescString = AllocateZeroPool ((ARRAY_SIZE (IdentifyData->ModelName) + 1) * sizeof (CHAR16));
  if (DiskContext->DescString) {
    for (Index = 0; (Index + 1) < ARRAY_SIZE (IdentifyData->ModelName); Index += 2) {
      DiskContext->DescString[Index]     = (CHAR16) (IdentifyData->ModelName[Index + 1]);
      DiskContext->DescString[Index + 1] = (CHAR16) (IdentifyData->ModelName[Index]);
    }
    EliminateExtraSpacesInternal (DiskContext->DescString);
  }

  DiskContext->SerialNoString = AllocateZeroPool ((ARRAY_SIZE (IdentifyData->SerialNo) + 1) * sizeof (CHAR16));
  if (DiskContext->SerialNoString) {
    for (Index = 0; (Index + 1) < ARRAY_SIZE (IdentifyData->SerialNo); Index += 2) {
      DiskContext->SerialNoString[Index]     = (CHAR16) (IdentifyData->SerialNo[Index + 1]);
      DiskContext->SerialNoString[Index + 1] = (CHAR16) (IdentifyData->SerialNo[Index]);
    }
    EliminateExtraSpacesInternal (DiskContext->SerialNoString);
  }

  DiskContext->FirmwareVerString = AllocateZeroPool ((ARRAY_SIZE (IdentifyData->FirmwareVer) + 1) * sizeof (CHAR16));
  if (DiskContext->FirmwareVerString) {
    for (Index = 0; (Index + 1) < ARRAY_SIZE (IdentifyData->FirmwareVer); Index += 2) {
      DiskContext->FirmwareVerString[Index]     = (CHAR16) (IdentifyData->FirmwareVer[Index + 1]);
      DiskContext->FirmwareVerString[Index + 1] = (CHAR16) (IdentifyData->FirmwareVer[Index]);
    }
    EliminateExtraSpacesInternal (DiskContext->FirmwareVerString);
  }

  NumSectors = 0;
  if ((!(IdentifyData->config & BIT15)) || (IdentifyData->config == 0x848A)) {
    if (IdentifyData->command_set_supported_83 & BIT10) {
      NumSectors = *(UINT64 *) &IdentifyData->maximum_lba_for_48bit_addressing;
    } else {
      NumSectors = (UINT64) *(UINT32 *) &IdentifyData->user_addressable_sectors_lo;
    }
  }

  DiskContext->CapacityInGB = (UINTN) DivU64x64Remainder (MultU64x32 (NumSectors, 512), 1000000000, NULL);

  Status = gBS->HandleProtocol (Handle, &gEfiDevicePathProtocolGuid, (VOID **) &DevicePath.DevPath);
  if (!EFI_ERROR (Status)) {
    DiskContext->DevicePath = DuplicateDevicePath (DevicePath.DevPath);
  }

  DiskContext->DeviceHandle = Handle;
  DiskContext->DiskType     = (IdentifyData->nominal_media_rotation_rate == 0x1) ? DiskSsd : DiskHdd;

  *Context = DiskContext;
  Status   = EFI_SUCCESS;

FreeExit:
  FreePool (IdentifyData);

  return Status;
}

/**
  Get disk info context for NVME drives.

  @param[in]  Handle        Device handle.
  @param[in]  Context       A pointer to disk info context data structure.
                            The caller is responsible to free context.

  @retval EFI_SUCCESS             Get disk info context successfully.
  @retval EFI_INVALID_PARAMETER   One of the input parameters is invalid.
  @retval EFI_OUT_OF_RESOURCES    Allocate buffer failed.
  @retval Others                  Other failures.

**/
STATIC
EFI_STATUS
NvmeGetContextInternal (
  IN  EFI_HANDLE          Handle,
  OUT DISK_INFO_CONTEXT   **Context
  )
{
  EFI_STATUS                                Status;
  UINTN                                     Index;
  UINTN                                     HandleIndex;
  UINTN                                     HandleCount;
  EFI_HANDLE                                *HandleBuffer;
  EFI_BLOCK_IO_PROTOCOL                     *BlockIoProtocol;
  EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL        *NvmePassthruProtocol;
  EFI_NVM_EXPRESS_PASS_THRU_COMMAND_PACKET  CommandPacket;
  EFI_NVM_EXPRESS_COMMAND                   Command;
  EFI_NVM_EXPRESS_COMPLETION                Completion;
  NVME_ADMIN_CONTROLLER_DATA                ControllerData;
  EFI_DEV_PATH_PTR                          DevicePath;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY       *OpenInfos;
  UINTN                                     OpenInfoCount;
  UINTN                                     OpenInfoIndex;
  BOOLEAN                                   FoundDev;
  DISK_INFO_CONTEXT                         *DiskContext;

  HandleCount  = 0;
  HandleBuffer = NULL;
  FoundDev     = FALSE;
  DiskContext  = NULL;

  if ((Handle == NULL) || (Context == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Context = NULL;

  Status = gBS->LocateHandleBuffer (
                          ByProtocol,
                          &gEfiNvmExpressPassThruProtocolGuid,
                          NULL,
                          &HandleCount,
                          &HandleBuffer
                          );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Status = gBS->OpenProtocolInformation (
                          HandleBuffer[HandleIndex],
                          &gEfiNvmExpressPassThruProtocolGuid,
                          &OpenInfos,
                          &OpenInfoCount
                          );
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
      if (((OpenInfos[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) != 0) &&
          (OpenInfos[OpenInfoIndex].ControllerHandle == Handle)) {
        Status = gBS->OpenProtocol (
                          HandleBuffer[HandleIndex],
                          &gEfiNvmExpressPassThruProtocolGuid,
                          (VOID **) &NvmePassthruProtocol,
                          NULL,
                          NULL,
                          EFI_OPEN_PROTOCOL_GET_PROTOCOL
                          );
        if (!EFI_ERROR (Status)) {
          FoundDev = TRUE;
          break;
        }
      }
    }

    if (FoundDev) {
      break;
    }
  }

  if (!FoundDev) {
    Status = EFI_NOT_FOUND;
    goto FreeExit;
  }

  ZeroMem (&CommandPacket, sizeof (CommandPacket));
  ZeroMem (&Command, sizeof (Command));
  ZeroMem (&Completion, sizeof (Completion));

  Command.Cdw0.Opcode          = NVME_ADMIN_IDENTIFY_CMD;
  Command.Nsid                 = 0;
  CommandPacket.NvmeCmd        = &Command;
  CommandPacket.NvmeCompletion = &Completion;
  CommandPacket.TransferBuffer = &ControllerData;
  CommandPacket.TransferLength = sizeof (ControllerData);
  CommandPacket.CommandTimeout = EFI_TIMER_PERIOD_SECONDS (5);
  CommandPacket.QueueType      = NVME_ADMIN_QUEUE;
  Command.Cdw10                = 1;
  Command.Flags                = CDW10_VALID;

  Status = NvmePassthruProtocol->PassThru (NvmePassthruProtocol, 0, &CommandPacket, NULL);
  if (EFI_ERROR (Status)) {
    goto FreeExit;
  }

  DiskContext = AllocateZeroPool (sizeof (*DiskContext));
  if (DiskContext == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeExit;
  }

  DiskContext->DescString = AllocateZeroPool ((ARRAY_SIZE (ControllerData.Mn) + 1) * sizeof (CHAR16));
  if (DiskContext->DescString) {
    for (Index = 0; Index < ARRAY_SIZE (ControllerData.Mn); Index++) {
      DiskContext->DescString[Index] = (CHAR16) (ControllerData.Mn[Index]);
    }
    EliminateExtraSpacesInternal (DiskContext->DescString);
  }

  DiskContext->SerialNoString = AllocateZeroPool ((ARRAY_SIZE (ControllerData.Sn) + 1) * sizeof (CHAR16));
  if (DiskContext->SerialNoString) {
    for (Index = 0; Index < ARRAY_SIZE (ControllerData.Sn); Index++) {
      DiskContext->SerialNoString[Index] = (CHAR16) (ControllerData.Sn[Index]);
    }
    EliminateExtraSpacesInternal (DiskContext->SerialNoString);
  }

  DiskContext->FirmwareVerString = AllocateZeroPool ((ARRAY_SIZE (ControllerData.Fr) + 1) * sizeof (CHAR16));
  if (DiskContext->FirmwareVerString) {
    for (Index = 0; Index < ARRAY_SIZE (ControllerData.Fr); Index++) {
      DiskContext->FirmwareVerString[Index] = (CHAR16) (ControllerData.Fr[Index]);
    }
    EliminateExtraSpacesInternal (DiskContext->FirmwareVerString);
  }

  Status = gBS->HandleProtocol (Handle, &gEfiBlockIoProtocolGuid, (VOID **) &BlockIoProtocol);
  if (!EFI_ERROR (Status)) {
    DiskContext->CapacityInGB = \
      DivU64x64Remainder (
        MultU64x32 (BlockIoProtocol->Media->LastBlock + 1, BlockIoProtocol->Media->BlockSize),
        1000000000,
        NULL
        );
  }

  Status = gBS->HandleProtocol (Handle, &gEfiDevicePathProtocolGuid, (VOID **) &DevicePath.DevPath);
  if (!EFI_ERROR (Status)) {
    DiskContext->DevicePath = DuplicateDevicePath (DevicePath.DevPath);
  }

  DiskContext->DeviceHandle = Handle;
  DiskContext->DiskType     = DiskNvme;

  *Context = DiskContext;
  Status   = EFI_SUCCESS;

FreeExit:
  FreePool (HandleBuffer);

  return Status;
}


/**
  Get disk type unicode string.

  @param[in]  Type    Disk Type.

  @retval !NULL       Disk type unicode string.
  @retval NULL        Disk type is invalid.

**/
CHAR16 *
EFIAPI
DiskInfoLibGetTypeString (
  IN UINT8                Type
  )
{
  switch (Type) {
    case DiskSsd:
      return L"SSD";
    case DiskHdd:
      return L"HDD";
    case DiskNvme:
      return L"NVME";
    default:
      break;
  }

  return NULL;
}

/**
  Print context of disk informations array.

  @param[in]  DiskCount   Number of entries in the disk information array.
  @param[in]  DiskInfo    Pointer to disk information array.

**/
VOID
EFIAPI
DiskInfoLibPrintContext (
  IN UINTN              DiskCount,
  IN DISK_INFO_CONTEXT  *DiskInfo
  )
{
  UINTN                 Index;
  DISK_INFO_CONTEXT     *Context;

  if ((DiskCount == 0) || (DiskInfo == NULL)) {
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: %a Start.\n", gEfiCallerBaseName, __func__));

  for (Index = 0; Index < DiskCount; Index++) {
    Context = &DiskInfo[Index];

    DEBUG ((
      DEBUG_INFO,
      "[0x%p]: %s %s %s %s %dGB %s.\n",
      Context->DeviceHandle,
      DiskInfoLibGetTypeString (Context->DiskType),
      Context->DescString,
      Context->SerialNoString,
      Context->FirmwareVerString,
      Context->CapacityInGB,
      ConvertDevicePathToText (Context->DevicePath, FALSE, FALSE)
      ));
  }

  DEBUG ((DEBUG_INFO, "%a: %a End.\n", gEfiCallerBaseName, __func__));

  return;
}

/**
  Free context of disk informations array.

  @param[in]  DiskCount   Number of entries in the disk information array.
  @param[in]  DiskInfo    Pointer to disk information array.

**/
VOID
EFIAPI
DiskInfoLibFreeContext (
  IN UINTN              DiskCount,
  IN DISK_INFO_CONTEXT  *DiskInfo
  )
{
  UINTN                 Index;
  DISK_INFO_CONTEXT     *Context;

  if ((DiskCount == 0) || (DiskInfo == NULL)) {
    return;
  }

  for (Index = 0; Index < DiskCount; Index++) {
    Context = &DiskInfo[Index];

    if (Context->DevicePath) {
      FreePool (Context->DevicePath);
    }

    if (Context->DescString) {
      FreePool (Context->DescString);
    }

    if (Context->SerialNoString) {
      FreePool (Context->SerialNoString);
    }

    if (Context->FirmwareVerString) {
      FreePool (Context->FirmwareVerString);
    }
  }

  FreePool (DiskInfo);

  return;
}

/**
  Get context of disk information array.

  @param[out] DiskCount   Number of entries in the disk information array.
  @param[out] DiskInfo    Pointer to disk information array.
                          It's caller's responsibility to free the buffer.

  @retval EFI_SUCCESS     Get disk information array successfully.
  @retval Others          The return value of LocateHandleBuffer().

**/
EFI_STATUS
EFIAPI
DiskInfoLibGetContext (
  OUT UINTN               *DiskCount,
  OUT DISK_INFO_CONTEXT   **DiskInfo
  )
{
  EFI_STATUS              Status;
  UINTN                   HandleIndex;
  UINTN                   HandleCount;
  EFI_HANDLE              *HandleBuffer;
  EFI_DISK_INFO_PROTOCOL  *DiskInfoProtocol;
  DISK_INFO_CONTEXT       *Context;

  HandleCount  = 0;
  HandleBuffer = NULL;
  Context      = NULL;

  if ((DiskCount == NULL) || (DiskInfo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *DiskCount = 0;
  *DiskInfo  = NULL;

  Status = gBS->LocateHandleBuffer (
                          ByProtocol,
                          &gEfiDiskInfoProtocolGuid,
                          NULL,
                          &HandleCount,
                          &HandleBuffer
                          );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Status = gBS->HandleProtocol (
                          HandleBuffer[HandleIndex],
                          &gEfiDiskInfoProtocolGuid,
                          (VOID **) &DiskInfoProtocol
                          );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = EFI_UNSUPPORTED;

    if (CompareGuid (&DiskInfoProtocol->Interface, &gEfiDiskInfoAhciInterfaceGuid)) {
      Status = AhciGetContextInternal (HandleBuffer[HandleIndex], &Context);
    }

    if (CompareGuid (&DiskInfoProtocol->Interface, &gEfiDiskInfoNvmeInterfaceGuid)) {
      Status = NvmeGetContextInternal (HandleBuffer[HandleIndex], &Context);
    }

    if (!EFI_ERROR (Status)) {
      AppendContextInternal (DiskCount, DiskInfo, Context);
      if (Context) {
        FreePool (Context);
        Context = NULL;
      }
    }
  }

  if ((*DiskInfo != NULL) && (*DiskCount > 1)) {
    PerformQuickSort (*DiskInfo, *DiskCount, sizeof (DISK_INFO_CONTEXT), SortContextInternal);
  }

  FreePool (HandleBuffer);

  return EFI_SUCCESS;
}
