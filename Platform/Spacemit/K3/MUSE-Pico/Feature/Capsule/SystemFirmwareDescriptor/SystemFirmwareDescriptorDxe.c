/** @file
  System Firmware descriptor producer.

  Copyright (c) 2025, Spcemit Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/FirmwareVolume2.h>
#include <Guid/EdkiiSystemFmpCapsule.h>

EFI_HANDLE  mHandle;

/**
  Entrypoint for SystemFirmwareDescriptor.

  @param[in]  ImageHandle       The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable       A pointer to the EFI system table.

  @retval EFI_SUCCESS           Get SystemFirmwareImageDescriptor successfully.
**/
EFI_STATUS
EFIAPI
SystemFirmwareDescriptorEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_FIRMWARE_VOLUME2_PROTOCOL           *FvProtocol;
  EDKII_SYSTEM_FIRMWARE_IMAGE_DESCRIPTOR  *Descriptor = NULL;
  UINT32                                  Authentication;
  UINTN                                   Size, SectionSize;
  EFI_STATUS                              Status;

  // Locate FV protocol for your firmware volume
  Status = gBS->LocateProtocol (&gEfiFirmwareVolume2ProtocolGuid, NULL, (VOID **)&FvProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Fail(%r) to locate FV protocol\n", Status));
    return Status;
  }

  //
  // Get System Firmware descriptor.
  //
  Status = FvProtocol->ReadSection (
                                    FvProtocol,
                                    &gEdkiiSystemFirmwareImageDescriptorFileGuid,
                                    EFI_SECTION_RAW,
                                    0,
                                    (VOID **)&Descriptor,
                                    &SectionSize,
                                    &Authentication
                                    );
  if (!EFI_ERROR (Status)) {
    Size = Descriptor->Length;
    DEBUG (
           (DEBUG_INFO, "SystemFirmwareImageDescriptor Signature: 0x%x, size: %d\n",
            Descriptor->Signature, Size)
           );
    PcdSetPtrS (PcdEdkiiSystemFirmwareImageDescriptor, &Size, Descriptor);
  }

  if (!EFI_ERROR (Status)) {
    Status = gBS->InstallProtocolInterface (
                                            &mHandle,
                                            &gSpacemitSystemFirmwareDescriptorReadyProtocolGuid,
                                            EFI_NATIVE_INTERFACE,
                                            NULL
                                            );
    if (EFI_ERROR (Status)) {
      DEBUG (
             (DEBUG_ERROR, "%a: Install FirmwareDescriptor ready protocol failed(%r).\n",
              __func__, Status)
             );
    }
  }

  return Status;
}
