/** @file
  RISC-V SEC phase module.

  Copyright (c) 2019, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SecMain.h"
#include <Library/SerialPortLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugPrintErrorLevelLib.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi.h>
#include <sbi/sbi_init.h>
#include <sbi/SbiFirmwareContext.h>

int HartsIn = 0;

EFI_PEI_TEMPORARY_RAM_SUPPORT_PPI mTemporaryRamSupportPpi = {
  TemporaryRamMigration
};

EFI_PEI_TEMPORARY_RAM_DONE_PPI mTemporaryRamDonePpi = {
  TemporaryRamDone
};

EFI_PEI_PPI_DESCRIPTOR mPrivateDispatchTable[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI,
    &gEfiTemporaryRamSupportPpiGuid,
    &mTemporaryRamSupportPpi
  },
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiTemporaryRamDonePpiGuid,
    &mTemporaryRamDonePpi
  },
};

/**
  Locates a section within a series of sections
  with the specified section type.

  The Instance parameter indicates which instance of the section
  type to return. (0 is first instance, 1 is second...)

  @param[in]   Sections        The sections to search
  @param[in]   SizeOfSections  Total size of all sections
  @param[in]   SectionType     The section type to locate
  @param[in]   Instance        The section instance number
  @param[out]  FoundSection    The FFS section if found

  @retval EFI_SUCCESS           The file and section was found
  @retval EFI_NOT_FOUND         The file and section was not found
  @retval EFI_VOLUME_CORRUPTED  The firmware volume was corrupted

**/
EFI_STATUS
FindFfsSectionInstance (
  IN  VOID                             *Sections,
  IN  UINTN                            SizeOfSections,
  IN  EFI_SECTION_TYPE                 SectionType,
  IN  UINTN                            Instance,
  OUT EFI_COMMON_SECTION_HEADER        **FoundSection
  )
{
  EFI_PHYSICAL_ADDRESS        CurrentAddress;
  UINT32                      Size;
  EFI_PHYSICAL_ADDRESS        EndOfSections;
  EFI_COMMON_SECTION_HEADER   *Section;
  EFI_PHYSICAL_ADDRESS        EndOfSection;

  //
  // Loop through the FFS file sections within the PEI Core FFS file
  //
  EndOfSection = (EFI_PHYSICAL_ADDRESS)(UINTN) Sections;
  EndOfSections = EndOfSection + SizeOfSections;
  for (;;) {
    if (EndOfSection == EndOfSections) {
      break;
    }
    CurrentAddress = (EndOfSection + 3) & ~(3ULL);
    if (CurrentAddress >= EndOfSections) {
      return EFI_VOLUME_CORRUPTED;
    }

    Section = (EFI_COMMON_SECTION_HEADER*)(UINTN) CurrentAddress;

    Size = SECTION_SIZE (Section);
    if (Size < sizeof (*Section)) {
      return EFI_VOLUME_CORRUPTED;
    }

    EndOfSection = CurrentAddress + Size;
    if (EndOfSection > EndOfSections) {
      return EFI_VOLUME_CORRUPTED;
    }

    //
    // Look for the requested section type
    //
    if (Section->Type == SectionType) {
      if (Instance == 0) {
        *FoundSection = Section;
        return EFI_SUCCESS;
      } else {
        Instance--;
      }
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Locates a section within a series of sections
  with the specified section type.

  @param[in]   Sections        The sections to search
  @param[in]   SizeOfSections  Total size of all sections
  @param[in]   SectionType     The section type to locate
  @param[out]  FoundSection    The FFS section if found

  @retval EFI_SUCCESS           The file and section was found
  @retval EFI_NOT_FOUND         The file and section was not found
  @retval EFI_VOLUME_CORRUPTED  The firmware volume was corrupted

**/
EFI_STATUS
FindFfsSectionInSections (
  IN  VOID                             *Sections,
  IN  UINTN                            SizeOfSections,
  IN  EFI_SECTION_TYPE                 SectionType,
  OUT EFI_COMMON_SECTION_HEADER        **FoundSection
  )
{
  return FindFfsSectionInstance (
           Sections,
           SizeOfSections,
           SectionType,
           0,
           FoundSection
           );
}

/**
  Locates a FFS file with the specified file type and a section
  within that file with the specified section type.

  @param[in]   Fv            The firmware volume to search
  @param[in]   FileType      The file type to locate
  @param[in]   SectionType   The section type to locate
  @param[out]  FoundSection  The FFS section if found

  @retval EFI_SUCCESS           The file and section was found
  @retval EFI_NOT_FOUND         The file and section was not found
  @retval EFI_VOLUME_CORRUPTED  The firmware volume was corrupted

**/
EFI_STATUS
FindFfsFileAndSection (
  IN  EFI_FIRMWARE_VOLUME_HEADER       *Fv,
  IN  EFI_FV_FILETYPE                  FileType,
  IN  EFI_SECTION_TYPE                 SectionType,
  OUT EFI_COMMON_SECTION_HEADER        **FoundSection
  )
{
  EFI_STATUS                  Status;
  EFI_PHYSICAL_ADDRESS        CurrentAddress;
  EFI_PHYSICAL_ADDRESS        EndOfFirmwareVolume;
  EFI_FFS_FILE_HEADER         *File;
  UINT32                      Size;
  EFI_PHYSICAL_ADDRESS        EndOfFile;

  if (Fv->Signature != EFI_FVH_SIGNATURE) {
    DEBUG ((DEBUG_ERROR, "FV at %p does not have FV header signature\n", Fv));
    return EFI_VOLUME_CORRUPTED;
  }

  CurrentAddress = (EFI_PHYSICAL_ADDRESS)(UINTN) Fv;
  EndOfFirmwareVolume = CurrentAddress + Fv->FvLength;

  //
  // Loop through the FFS files in the Boot Firmware Volume
  //
  for (EndOfFile = CurrentAddress + Fv->HeaderLength; ; ) {

    CurrentAddress = (EndOfFile + 7) & ~(7ULL);
    if (CurrentAddress > EndOfFirmwareVolume) {
      return EFI_VOLUME_CORRUPTED;
    }

    File = (EFI_FFS_FILE_HEADER*)(UINTN) CurrentAddress;
    Size = *(UINT32*) File->Size & 0xffffff;
    if (Size < (sizeof (*File) + sizeof (EFI_COMMON_SECTION_HEADER))) {
      return EFI_VOLUME_CORRUPTED;
    }

    EndOfFile = CurrentAddress + Size;
    if (EndOfFile > EndOfFirmwareVolume) {
      return EFI_VOLUME_CORRUPTED;
    }

    //
    // Look for the request file type
    //
    if (File->Type != FileType) {
      continue;
    }

    Status = FindFfsSectionInSections (
               (VOID*) (File + 1),
               (UINTN) EndOfFile - (UINTN) (File + 1),
               SectionType,
               FoundSection
               );
    if (!EFI_ERROR (Status) || (Status == EFI_VOLUME_CORRUPTED)) {
      return Status;
    }
  }
}

/**
  Locates the PEI Core entry point address

  @param[in]  Fv                 The firmware volume to search
  @param[out] PeiCoreEntryPoint  The entry point of the PEI Core image

  @retval EFI_SUCCESS           The file and section was found
  @retval EFI_NOT_FOUND         The file and section was not found
  @retval EFI_VOLUME_CORRUPTED  The firmware volume was corrupted

**/
EFI_STATUS
FindPeiCoreImageBaseInFv (
  IN  EFI_FIRMWARE_VOLUME_HEADER       *Fv,
  OUT  EFI_PHYSICAL_ADDRESS             *PeiCoreImageBase
  )
{
  EFI_STATUS                  Status;
  EFI_COMMON_SECTION_HEADER   *Section;

  Status = FindFfsFileAndSection (
             Fv,
             EFI_FV_FILETYPE_PEI_CORE,
             EFI_SECTION_PE32,
             &Section
             );
  if (EFI_ERROR (Status)) {
    Status = FindFfsFileAndSection (
               Fv,
               EFI_FV_FILETYPE_PEI_CORE,
               EFI_SECTION_TE,
               &Section
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Unable to find PEI Core image\n"));
      return Status;
    }
  }
  DEBUG ((DEBUG_ERROR, "PeiCoreImageBase found\n"));
  *PeiCoreImageBase = (EFI_PHYSICAL_ADDRESS)(UINTN)(Section + 1);
  return EFI_SUCCESS;
}

/**
  Locates the PEI Core entry point address

  @param[in,out]  Fv                 The firmware volume to search
  @param[out]     PeiCoreEntryPoint  The entry point of the PEI Core image

  @retval EFI_SUCCESS           The file and section was found
  @retval EFI_NOT_FOUND         The file and section was not found
  @retval EFI_VOLUME_CORRUPTED  The firmware volume was corrupted

**/
VOID
FindPeiCoreImageBase (
  IN OUT  EFI_FIRMWARE_VOLUME_HEADER       **BootFv,
     OUT  EFI_PHYSICAL_ADDRESS             *PeiCoreImageBase
  )
{
  *PeiCoreImageBase = 0;

  DEBUG ((DEBUG_INFO, "FindPeiCoreImageBaseInFv\n"));
  FindPeiCoreImageBaseInFv (*BootFv, PeiCoreImageBase);
}

/*
  Find and return Pei Core entry point.

  It also find SEC and PEI Core file debug inforamtion. It will report them if
  remote debug is enabled.

**/
VOID
FindAndReportEntryPoints (
  IN  EFI_FIRMWARE_VOLUME_HEADER       **BootFirmwareVolumePtr,
  OUT EFI_PEI_CORE_ENTRY_POINT         *PeiCoreEntryPoint
  )
{
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             PeiCoreImageBase;

  DEBUG ((DEBUG_INFO, "FindAndReportEntryPoints\n"));

  FindPeiCoreImageBase (BootFirmwareVolumePtr, &PeiCoreImageBase);
  //
  // Find PEI Core entry point
  //
  Status = PeCoffLoaderGetEntryPoint ((VOID *) (UINTN) PeiCoreImageBase, (VOID**) PeiCoreEntryPoint);
  if (EFI_ERROR(Status)) {
    *PeiCoreEntryPoint = 0;
  }
  DEBUG ((DEBUG_INFO, "PeCoffLoaderGetEntryPoint success: %x\n", *PeiCoreEntryPoint));

  return;
}
/*
  Print out the content of firmware context.

**/
VOID
DebutPrintFirmwareContext (
    EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT *FirmwareContext
    )
{
  DEBUG ((DEBUG_INFO, "[OpenSBI]: OpenSBI Firmware Context at 0x%x\n", FirmwareContext));
  DEBUG ((DEBUG_INFO, "           PEI Service at 0x%x\n\n", FirmwareContext->PeiServiceTable));
}

EFI_STATUS
EFIAPI
TemporaryRamMigration (
  IN CONST EFI_PEI_SERVICES   **PeiServices,
  IN EFI_PHYSICAL_ADDRESS     TemporaryMemoryBase,
  IN EFI_PHYSICAL_ADDRESS     PermanentMemoryBase,
  IN UINTN                    CopySize
  )
{
  VOID      *OldHeap;
  VOID      *NewHeap;
  VOID      *OldStack;
  VOID      *NewStack;
  struct sbi_platform *ThisSbiPlatform;

  DEBUG ((DEBUG_INFO,
    "TemporaryRamMigration(0x%Lx, 0x%Lx, 0x%Lx)\n",
    TemporaryMemoryBase,
    PermanentMemoryBase,
    (UINT64)CopySize
    ));

  OldHeap = (VOID*)(UINTN)TemporaryMemoryBase;
  NewHeap = (VOID*)((UINTN)PermanentMemoryBase + (CopySize >> 1));

  OldStack = (VOID*)((UINTN)TemporaryMemoryBase + (CopySize >> 1));
  NewStack = (VOID*)(UINTN)PermanentMemoryBase;

  CopyMem (NewHeap, OldHeap, CopySize >> 1);   // Migrate Heap
  CopyMem (NewStack, OldStack, CopySize >> 1); // Migrate Stack

  //
  // Reset firmware context pointer
  //
  ThisSbiPlatform = (struct sbi_platform *)sbi_platform_ptr(sbi_scratch_thishart_ptr());
  ThisSbiPlatform->firmware_context += (unsigned long)((UINTN)NewStack - (UINTN)OldStack);
  //
  // Relocate PEI Service **
  //
  ((EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT *)ThisSbiPlatform->firmware_context)->PeiServiceTable += (unsigned long)((UINTN)NewStack - (UINTN)OldStack);
  DEBUG ((DEBUG_INFO, "[OpenSBI]: OpenSBI Firmware Context is relocated to 0x%x\n", ThisSbiPlatform->firmware_context));
  DebutPrintFirmwareContext ((EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT *)ThisSbiPlatform->firmware_context);

  register uintptr_t a0 asm ("a0") = (uintptr_t)((UINTN)NewStack - (UINTN)OldStack);
  asm volatile ("add sp, sp, a0"::"r"(a0):);
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI TemporaryRamDone (VOID)
{
  DEBUG ((DEBUG_INFO, "2nd time PEI core, temporary ram done.\n"));
  return EFI_SUCCESS;
}

#if 1
#define GPIO_CTRL_ADDR  0x54002000
#define GPIO_OUTPUT_VAL 0x0C
static volatile UINT32 * const gpio = (void *)GPIO_CTRL_ADDR;
#define REG32(p, i)   ((p)[(i)>>2])
#endif

static VOID EFIAPI PeiCore(VOID)
{
  EFI_SEC_PEI_HAND_OFF        SecCoreData;
  EFI_PEI_CORE_ENTRY_POINT    PeiCoreEntryPoint;
  EFI_FIRMWARE_VOLUME_HEADER *BootFv = (EFI_FIRMWARE_VOLUME_HEADER *)FixedPcdGet32(PcdRiscVPeiFvBase);
  EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT FirmwareContext;
  struct sbi_platform *ThisSbiPlatform;
  UINT32 HartId;

  REG32(gpio, GPIO_OUTPUT_VAL) = 0x88;
  FindAndReportEntryPoints (&BootFv, &PeiCoreEntryPoint);

  SecCoreData.DataSize               = sizeof(EFI_SEC_PEI_HAND_OFF);
  SecCoreData.BootFirmwareVolumeBase = BootFv;
  SecCoreData.BootFirmwareVolumeSize = (UINTN) BootFv->FvLength;
  SecCoreData.TemporaryRamBase       = (VOID*)(UINT64) FixedPcdGet32(PcdTemporaryRamBase);
  SecCoreData.TemporaryRamSize       = (UINTN)  FixedPcdGet32(PcdTemporaryRamSize);
  SecCoreData.PeiTemporaryRamBase    = SecCoreData.TemporaryRamBase;
  SecCoreData.PeiTemporaryRamSize    = SecCoreData.TemporaryRamSize >> 1;
  SecCoreData.StackBase              = (UINT8 *)SecCoreData.TemporaryRamBase + (SecCoreData.TemporaryRamSize >> 1);
  SecCoreData.StackSize              = SecCoreData.TemporaryRamSize >> 1;

  //
  // Print out scratch address of each hart
  //
  DEBUG ((DEBUG_INFO, "[OpenSBI]: OpenSBI scratch address for each hart:\n"));
  for (HartId = 0; HartId < FixedPcdGet32 (PcdHartCount); HartId ++) {
    DEBUG ((DEBUG_INFO, "          Hart %d: 0x%x\n", HartId, sbi_hart_id_to_scratch(sbi_scratch_thishart_ptr(), HartId)));
  }

  //
  // Set up OpepSBI firmware context poitner on boot hart OpenSbi scratch. Firmware context residents in stack and will be
  // switched to memory when temporary ram migration.
  //
  ZeroMem ((VOID *)&FirmwareContext, sizeof (EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT));
  ThisSbiPlatform = (struct sbi_platform *)sbi_platform_ptr(sbi_scratch_thishart_ptr());
  if (ThisSbiPlatform->opensbi_version > OPENSBI_VERSION) {
      DEBUG ((DEBUG_ERROR, "[OpenSBI]: OpenSBI platform table version 0x%x is newer than OpenSBI version 0x%x.\n"
                           "There maybe be some backward compatable issues.\n",
             ThisSbiPlatform->opensbi_version,
             OPENSBI_VERSION
             ));
      ASSERT(FALSE);
  }
  DEBUG ((DEBUG_INFO, "[OpenSBI]: OpenSBI platform table at address: 0x%x\nFirmware Context is located at 0x%x\n",
             ThisSbiPlatform,
             &FirmwareContext
             ));
  ThisSbiPlatform->firmware_context = (unsigned long)&FirmwareContext;
  //
  // Set firmware context Hart-specific pointer
  //
  for (HartId = 0; HartId < FixedPcdGet32 (PcdHartCount); HartId ++) {
    FirmwareContext.HartSpecific [HartId] = \
      (EFI_RISCV_FIRMWARE_CONTEXT_HART_SPECIFIC *)((UINT8 *)sbi_hart_id_to_scratch(sbi_scratch_thishart_ptr(), HartId) - FIRMWARE_CONTEXT_HART_SPECIFIC_SIZE);
    DEBUG ((DEBUG_INFO, "[OpenSBI]: OpenSBI Hart %d Firmware Context Hart-specific at address: 0x%x\n",
             HartId,
             FirmwareContext.HartSpecific [HartId]
             ));
  }

  //
  // Transfer the control to the PEI core
  //
  (*PeiCoreEntryPoint) (&SecCoreData, (EFI_PEI_PPI_DESCRIPTOR *)&mPrivateDispatchTable);
}
/**
  This function initilizes hart specific information and SBI.
  For the boot hart, it boots system through PEI core and initial SBI in the DXE IPL.
  For others, it goes to initial SBI and halt.

  the lay out of memory region for each hart is as below delineates,

                                               _                                        ____
  |----Scratch ends                             |                                           |
  |                                             | sizeof (sbi_scratch)                      |
  |                                            _|                                           |
  |----Scratch buffer start s                  <----- *scratch                              |
  |----Firmware Context Hart-specific ends     _                                            |
  |                                             |                                           |
  |                                             | FIRMWARE_CONTEXT_HART_SPECIFIC_SIZE       |
  |                                             |                                           |  PcdOpenSbiStackSize
  |                                            _|                                           |
  |----Firmware Context Hart-specific starts   <----- **HartFirmwareContext                 |
  |----Hart stack top                          _                                            |
  |                                             |                                           |
  |                                             |                                           |
  |                                             |  Stack                                    |
  |                                             |                                           |
  |                                            _|                                       ____|
  |----Hart stack bottom

**/
VOID EFIAPI SecCoreStartUpWithStack(unsigned long hartid, struct sbi_scratch *scratch)
{
  EFI_RISCV_FIRMWARE_CONTEXT_HART_SPECIFIC *HartFirmwareContext;

  //
  // Setup EFI_RISCV_FIRMWARE_CONTEXT_HART_SPECIFIC for each hart.
  //
  HartFirmwareContext = (EFI_RISCV_FIRMWARE_CONTEXT_HART_SPECIFIC *)((UINT8 *)scratch - FIRMWARE_CONTEXT_HART_SPECIFIC_SIZE);
  HartFirmwareContext->IsaExtensionSupported = RiscVReadMisa ();
  HartFirmwareContext->MachineVendorId.Value64_L = RiscVReadMVendorId ();
  HartFirmwareContext->MachineVendorId.Value64_H = 0;
  HartFirmwareContext->MachineArchId.Value64_L = RiscVReadMArchId ();
  HartFirmwareContext->MachineArchId.Value64_H = 0;
  HartFirmwareContext->MachineImplId.Value64_L = RiscVReadMImplId ();
  HartFirmwareContext->MachineImplId.Value64_H = 0;

#if 0
  while (HartsIn != hartid);
  DEBUG ((DEBUG_INFO, "[OpenSBI]: Initial Firmware Context Hart-specific for HART ID:%d\n", hartid));
  DEBUG ((DEBUG_INFO, "           Scratch at address: 0x%x\n", scratch));
  DEBUG ((DEBUG_INFO, "           Firmware Context Hart-specific at address: 0x%x\n", HartFirmwareContext));
  DEBUG ((DEBUG_INFO, "           stack pointer at address: 0x%x\n", stack_point));
  DEBUG ((DEBUG_INFO, "                    MISA: 0x%x\n", HartFirmwareContext->IsaExtensionSupported));
  DEBUG ((DEBUG_INFO, "                    MVENDORID: 0x%x\n", HartFirmwareContext->MachineVendorId.Value64_L));
  DEBUG ((DEBUG_INFO, "                    MARCHID: 0x%x\n", HartFirmwareContext->MachineArchId.Value64_L));
  DEBUG ((DEBUG_INFO, "                    MIMPID: 0x%x\n\n", HartFirmwareContext->MachineImplId.Value64_L));
  HartsIn ++;
  for (;;);
#endif

  if (hartid == FixedPcdGet32(PcdBootHartId)) {
    PeiCore();
  }
  sbi_init(scratch);
}
