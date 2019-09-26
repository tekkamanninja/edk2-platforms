/** @file
  SiFive E51 Core library definitions.

  Copyright (c) 2019, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef _SIFIVE_E51_CORE_H_
#define _SIFIVE_E51_CORE_H_

#include <PiPei.h>

#include <SmbiosProcessorSpecificData.h>
#include <ProcessorSpecificDataHob.h>

/**
  Function to build core specific information HOB.

  @param  ParentProcessorGuid    Parent processor od this core. ParentProcessorGuid
                                 could be the same as CoreGuid if one processor has
                                 only one core.
  @param  ParentProcessorUid     Unique ID of pysical processor which owns this core.
  @param  HartId                 Hart ID of this core.
  @param  IsBootHart             TRUE means this is the boot HART.
  @param  GuidHobData            Pointer to receive RISC_V_PROCESSOR_SPECIFIC_DATA_HOB.

  @return EFI_SUCCESS     The PEIM initialized successfully.

**/
EFI_STATUS
EFIAPI
CreateE51CoreProcessorSpecificDataHob (
  IN EFI_GUID  *ParentProcessorGuid,
  IN UINTN     ParentProcessorUid,
  IN UINTN     HartId,
  IN BOOLEAN   IsBootHart,
  OUT RISC_V_PROCESSOR_SPECIFIC_DATA_HOB **GuidHobData
  );

/**
  Function to build processor related SMBIOS information. RISC-V SMBIOS DXE driver collect
  this information and build SMBIOS Type4 and Type7 record.

  @param  ProcessorUid    Unique ID of pysical processor which owns this core.
  @param  SmbiosHobPtr    Pointer to receive RISC_V_PROCESSOR_SMBIOS_DATA_HOB. The pointers
                          maintained in this structure is only valid before memory is discovered.
                          Access to those pointers after memory is installed will cause unexpected issues.

  @return EFI_SUCCESS     The PEIM initialized successfully.

**/
EFI_STATUS
EFIAPI
CreateE51ProcessorSmbiosDataHob (
  IN UINTN     ProcessorUid,
  OUT RISC_V_PROCESSOR_SMBIOS_DATA_HOB **SmbiosHobPtr
  );

#endif
