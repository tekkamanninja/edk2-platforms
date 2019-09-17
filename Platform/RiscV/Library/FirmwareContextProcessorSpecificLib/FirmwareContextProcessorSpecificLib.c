/**@file
  Common library to build upfirmware context processor-specific information

  Copyright (c) 2019, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

//
// The package level header files this module uses
//
#include <PiPei.h>

//
// The Library classes this module consumes
//
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseMemoryLib.h>

#include <RiscV.h>
#include <SmbiosProcessorSpecificData.h>
#include <ProcessorSpecificDataHob.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi.h>
#include <sbi/SbiFirmwareContext.h>

/**
  Build up common firmware context processor-specific information

  @param  FirmwareContextHartSpecific  Pointer to EFI_RISCV_FIRMWARE_CONTEXT_HART_SPECIFIC
  @param  ParentProcessorGuid          Pointer to GUID of Processor which contains this core
  @param  ParentProcessorUid           Unique ID of pysical processor which owns this core.
  @param  CoreGuid                     Pointer to GUID of core
  @param  HartId                       Hart ID of this core.
  @param  IsBootHart                   This is boot hart or not
  @param  ProcessorSpecDataHob         Pointer to RISC_V_PROCESSOR_SPECIFIC_DATA_HOB

  @return EFI_STATUS

**/
EFI_STATUS
EFIAPI
CommonFirmwareContextHartSpecificInfo (
  EFI_RISCV_FIRMWARE_CONTEXT_HART_SPECIFIC *FirmwareContextHartSpecific,
  EFI_GUID  *ParentProcessorGuid,
  UINTN     ParentProcessorUid,
  EFI_GUID  *CoreGuid,
  UINTN     HartId,
  BOOLEAN   IsBootHart,
  RISC_V_PROCESSOR_SPECIFIC_DATA_HOB *ProcessorSpecDataHob
  )
{
  //
  // Build up RISC_V_PROCESSOR_SPECIFIC_DATA_HOB.
  //
  CopyGuid (&ProcessorSpecDataHob->ParentPrcessorGuid, ParentProcessorGuid);
  ProcessorSpecDataHob->ParentProcessorUid = ParentProcessorUid;
  CopyGuid (&ProcessorSpecDataHob->CoreGuid, CoreGuid);
  ProcessorSpecDataHob->Context = NULL;
  ProcessorSpecDataHob->ProcessorSpecificData.Revision         = SMBIOS_RISC_V_PROCESSOR_SPECIFIC_DATA_REVISION;
  ProcessorSpecDataHob->ProcessorSpecificData.Length           = sizeof (SMBIOS_RISC_V_PROCESSOR_SPECIFIC_DATA);
  ProcessorSpecDataHob->ProcessorSpecificData.HartId.Value64_L = (UINT64)HartId;
  ProcessorSpecDataHob->ProcessorSpecificData.HartId.Value64_H = 0;
  ProcessorSpecDataHob->ProcessorSpecificData.BootHartId       = (UINT8)IsBootHart;
  ProcessorSpecDataHob->ProcessorSpecificData.InstSetSupported = FirmwareContextHartSpecific->IsaExtensionSupported;
  ProcessorSpecDataHob->ProcessorSpecificData.PrivilegeModeSupported   = SMBIOS_RISC_V_PSD_MACHINE_MODE_SUPPORTED;
  if ((ProcessorSpecDataHob->ProcessorSpecificData.InstSetSupported & RISC_V_ISA_SUPERVISOR_MODE_IMPLEMENTED) != 0) {
    ProcessorSpecDataHob->ProcessorSpecificData.PrivilegeModeSupported |= SMBIOS_RISC_V_PSD_SUPERVISOR_MODE_SUPPORTED;
  }
  if ((ProcessorSpecDataHob->ProcessorSpecificData.InstSetSupported & RISC_V_ISA_USER_MODE_IMPLEMENTED) != 0) {
    ProcessorSpecDataHob->ProcessorSpecificData.PrivilegeModeSupported |= SMBIOS_RISC_V_PSD_USER_MODE_SUPPORTED;
  }
  ProcessorSpecDataHob->ProcessorSpecificData.MachineVendorId.Value64_L = FirmwareContextHartSpecific->MachineVendorId.Value64_L;
  ProcessorSpecDataHob->ProcessorSpecificData.MachineVendorId.Value64_H = FirmwareContextHartSpecific->MachineVendorId.Value64_H;
  ProcessorSpecDataHob->ProcessorSpecificData.MachineArchId.Value64_L = FirmwareContextHartSpecific->MachineArchId.Value64_L;
  ProcessorSpecDataHob->ProcessorSpecificData.MachineArchId.Value64_H = FirmwareContextHartSpecific->MachineArchId.Value64_H;
  ProcessorSpecDataHob->ProcessorSpecificData.MachineImplId.Value64_L = FirmwareContextHartSpecific->MachineImplId.Value64_L;
  ProcessorSpecDataHob->ProcessorSpecificData.MachineImplId.Value64_H = FirmwareContextHartSpecific->MachineImplId.Value64_H;
  return EFI_SUCCESS;
}
