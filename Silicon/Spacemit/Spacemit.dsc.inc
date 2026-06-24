#
#  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

[BuildOptions]
  GCC:RELEASE_*_*_CC_FLAGS       = -DMDEPKG_NDEBUG
!ifdef $(SOURCE_DEBUG_ENABLE)
  GCC:*_*_RISCV64_GENFW_FLAGS    = --keepexceptiontable
!endif

[BuildOptions.common.EDKII.DXE_CORE, BuildOptions.common.EDKII.DXE_DRIVER, BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER, BuildOptions.common.EDKII.UEFI_DRIVER, BuildOptions.common.EDKII.UEFI_APPLICATION]
  GCC:  *_*_*_DLINK_FLAGS = -z common-page-size=0x1000
  MSFT: *_*_*_DLINK_FLAGS = /ALIGN:4096

################################################################################
#
# Library Class section - list of all Library Classes needed by this Platform.
#
################################################################################
[LibraryClasses.common]
!if $(DEBUG_ON_SERIAL_PORT) == TRUE
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
!else
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
!endif
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf

  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  BmpSupportLib|MdeModulePkg/Library/BaseBmpSupportLib/BaseBmpSupportLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLibDevicePathProtocol/UefiDevicePathLibDevicePathProtocol.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  ShellCEntryLib|ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
  OemHookStatusCodeLib|MdeModulePkg/Library/OemHookStatusCodeLibNull/OemHookStatusCodeLibNull.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  AcpiLib|EmbeddedPkg/Library/AcpiLib/AcpiLib.inf

  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf
  ImagePropertiesRecordLib|MdeModulePkg/Library/ImagePropertiesRecordLib/ImagePropertiesRecordLib.inf
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
  PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf
  TpmMeasurementLib|MdeModulePkg/Library/TpmMeasurementLibNull/TpmMeasurementLibNull.inf
  #
  # Secure Boot dependencies
  #
!if $(SECURE_BOOT_ENABLE) == TRUE
  AuthVariableLib|SecurityPkg/Library/AuthVariableLib/AuthVariableLib.inf
  SecureBootVariableLib|SecurityPkg/Library/SecureBootVariableLib/SecureBootVariableLib.inf
  SecureBootVariableProvisionLib|SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.inf
  PlatformPKProtectionLib|SecurityPkg/Library/PlatformPKProtectionLibVarPolicy/PlatformPKProtectionLibVarPolicy.inf
  PlatformSecureLib|SecurityPkg/Library/PlatformSecureLibNull/PlatformSecureLibNull.inf
!else
  AuthVariableLib|MdeModulePkg/Library/AuthVariableLibNull/AuthVariableLibNull.inf
!endif
  VarCheckLib|MdeModulePkg/Library/VarCheckLib/VarCheckLib.inf
  VariableFlashInfoLib|MdeModulePkg/Library/BaseVariableFlashInfoLib/BaseVariableFlashInfoLib.inf
  VariablePolicyLib|MdeModulePkg/Library/VariablePolicyLib/VariablePolicyLib.inf
  RealTimeClockLib|EmbeddedPkg//Library/VirtualRealTimeClockLib/VirtualRealTimeClockLib.inf
  TimeBaseLib|EmbeddedPkg/Library/TimeBaseLib/TimeBaseLib.inf
  FileExplorerLib|MdeModulePkg/Library/FileExplorerLib/FileExplorerLib.inf
  CustomizedDisplayLib|MdeModulePkg/Library/CustomizedDisplayLib/CustomizedDisplayLib.inf
  BootLogoLib|MdeModulePkg/Library/BootLogoLib/BootLogoLib.inf
  JedecJep106Lib|MdePkg/Library/JedecJep106Lib/JedecJep106Lib.inf

  # Flattened Device Tree (FDT) access library
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  FdtHelperLib|Silicon/Spacemit/Library/FdtHelperLib/FdtHelperLib.inf

  # RISC-V Architectural Libraries
  RiscVSbiLib|MdePkg/Library/BaseRiscVSbiLib/BaseRiscVSbiLib.inf
  CpuExceptionHandlerLib|UefiCpuPkg/Library/BaseRiscV64CpuExceptionHandlerLib/BaseRiscV64CpuExceptionHandlerLib.inf
  TimerLib|UefiCpuPkg/Library/BaseRiscV64CpuTimerLib/BaseRiscV64CpuTimerLib.inf
  RiscVMmuLib|UefiCpuPkg/Library/BaseRiscVMmuLib/BaseRiscVMmuLib.inf
  RiscVFpuLib|UefiCpuPkg/Library/BaseRiscVFpuLib/BaseRiscVFpuLib.inf

  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  DebugAgentTimerLib|EmbeddedPkg/Library/DebugAgentTimerLibNull/DebugAgentTimerLibNull.inf

  NorFlashInfoLib|EmbeddedPkg/Library/NorFlashInfoLib/NorFlashInfoLib.inf

!if $(CAPSULE_ENABLE)
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibFmp/DxeCapsuleLib.inf
  FmpAuthenticationLib|SecurityPkg/Library/FmpAuthenticationLibPkcs7/FmpAuthenticationLibPkcs7.inf
  BmpSupportLib|MdeModulePkg/Library/BaseBmpSupportLib/BaseBmpSupportLib.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
!else
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
  FmpAuthenticationLib|MdeModulePkg/Library/FmpAuthenticationLibNull/FmpAuthenticationLibNull.inf
!endif

  # Cryptographic libraries
  RngLib|MdePkg/Library/DxeRngLib/DxeRngLib.inf
  IntrinsicLib|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  BaseCryptLib|CryptoPkg/Library/BaseCryptLib/BaseCryptLib.inf
  OpensslLib|CryptoPkg/Library/OpensslLib/OpensslLib.inf
  TlsLib|CryptoPkg/Library/TlsLib/TlsLib.inf

  EdkiiSystemCapsuleLib|SignedCapsulePkg/Library/EdkiiSystemCapsuleLib/EdkiiSystemCapsuleLib.inf
  IniParsingLib|SignedCapsulePkg/Library/IniParsingLib/IniParsingLib.inf
  DisplayUpdateProgressLib|MdeModulePkg/Library/DisplayUpdateProgressLibText/DisplayUpdateProgressLibText.inf

  #
  # Silicon Libraries
  #
  PlatformFlashAccessLib|Silicon/Spacemit/Library/PlatformFlashAccessLib/PlatformFlashAccessLibDxe.inf
  MemoryManagementLib|Silicon/Spacemit/Library/MemoryManagementLibNull/MemoryManagementLibNull.inf
  CustomizedHiiLib|Silicon/Spacemit/Library/CustomizedHiiLib/CustomizedHiiLib.inf
  SmbiosLib|Silicon/Spacemit/Library/SmbiosLib/SmbiosLib.inf
  DiskInfoLib|Silicon/Spacemit/Library/DiskInfoLib/DiskInfoLib.inf
  CpuInfoLib|Silicon/Spacemit/Library/CpuInfoLibNull/CpuInfoLibNull.inf
  CpuInfoCustomizedDisplayLib|Silicon/Spacemit/Library/CpuInfoCustomizedDisplayLibNull/CpuInfoCustomizedDisplayLibNull.inf
  GenericBdsLib|Silicon/Spacemit/Library/GenericBdsLib/GenericBdsLib.inf

[LibraryClasses.common.SEC]
  MemoryAllocationLib|EmbeddedPkg/Library/PrePiMemoryAllocationLib/PrePiMemoryAllocationLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  HobLib|EmbeddedPkg/Library/PrePiHobLib/PrePiHobLib.inf
  ExtractGuidedSectionLib|EmbeddedPkg/Library/PrePiExtractGuidedSectionLib/PrePiExtractGuidedSectionLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/PeiReportStatusCodeLib/PeiReportStatusCodeLib.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf

[LibraryClasses.common.DXE_CORE]
  MemoryAllocationLib|MdeModulePkg/Library/DxeCoreMemoryAllocationLib/DxeCoreMemoryAllocationLib.inf
  HobLib|MdePkg/Library/DxeCoreHobLib/DxeCoreHobLib.inf
  DxeCoreEntryPoint|MdePkg/Library/DxeCoreEntryPoint/DxeCoreEntryPoint.inf
  PerformanceLib|MdeModulePkg/Library/DxeCorePerformanceLib/DxeCorePerformanceLib.inf

[LibraryClasses.common.DXE_DRIVER]
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
  BaseCryptLib|CryptoPkg/Library/BaseCryptLib/BaseCryptLib.inf
  MemoryManagementLib|Silicon/Spacemit/Library/MemoryManagementLib/MemoryManagementLibDxe.inf

[LibraryClasses.common.UEFI_DRIVER]
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  MemoryManagementLib|Silicon/Spacemit/Library/MemoryManagementLib/MemoryManagementLibDxe.inf

[LibraryClasses.common.DXE_RUNTIME_DRIVER]
  TimerLib|UefiCpuPkg/Library/BaseRiscV64CpuTimerLib/BaseRiscV64CpuTimerLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  VariablePolicyLib|MdeModulePkg/Library/VariablePolicyLib/VariablePolicyLibRuntimeDxe.inf
  ReportStatusCodeLib|MdeModulePkg/Library/RuntimeDxeReportStatusCodeLib/RuntimeDxeReportStatusCodeLib.inf
!if $(DEBUG_ON_SERIAL_PORT) == TRUE
  DebugLib|MdePkg/Library/DxeRuntimeDebugLibSerialPort/DxeRuntimeDebugLibSerialPort.inf
!endif
  BaseCryptLib|CryptoPkg/Library/BaseCryptLib/RuntimeCryptLib.inf
  MemoryManagementLib|Silicon/Spacemit/Library/MemoryManagementLib/MemoryManagementLibDxe.inf

!if $(CAPSULE_ENABLE)
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibFmp/DxeRuntimeCapsuleLib.inf
!endif

[LibraryClasses.common.UEFI_APPLICATION]
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  MemoryManagementLib|Silicon/Spacemit/Library/MemoryManagementLib/MemoryManagementLibDxe.inf

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform.
#
################################################################################
[PcdsFeatureFlag.common]
  # // TODO: why?
  # Not export HII data and configuration to be used in OS runtime, otherwise
  # when booting firmware in BDS phase, the warning "[HiiDatabase]: Memory
  # allocation is required after ReadyToBoot, which may change memory map and
  # cause S4 resume issue" will occur.
  gEfiMdeModulePkgTokenSpaceGuid.PcdHiiOsRuntimeSupport|FALSE

  # On/Off to help prevent EFI memory map fragments.
  # Refer to the comments below which mentions this flag.
  gEmbeddedTokenSpaceGuid.PcdPrePiProduceMemoryTypeInformationHob|TRUE

  ## Indicates if TEXT statement is always set to GrayOut statement in HII Form Browser.<BR><BR>
  #   TRUE  - TEXT statement will always be set to GrayOut.<BR>
  #   FALSE - TEXT statement will be set to GrayOut only when GrayOut condition is TRUE.<BR>
  # @Prompt Always GrayOut TEXT statement.
  gEfiMdeModulePkgTokenSpaceGuid.PcdBrowserGrayOutTextStatement|TRUE

  ## Indicates if unselectable menu should be gray out in HII Form Browser.<BR><BR>
  #   TRUE  - The unselectable menu will be set to GrayOut.<BR>
  #   FALSE - The menu will be show as normal menu entry even if it is not selectable.<BR>
  # @Prompt GrayOut read only menu.
  gEfiMdeModulePkgTokenSpaceGuid.PcdBrowerGrayOutReadOnlyMenu|TRUE

  #
  # Activate AcpiSdtProtocol
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdInstallAcpiSdtProtocol|TRUE

[PcdsFixedAtBuild.common]
  # DEBUG_ASSERT_ENABLED       0x01
  # DEBUG_PRINT_ENABLED        0x02
  # DEBUG_CODE_ENABLED         0x04
  # CLEAR_MEMORY_ENABLED       0x08
  # ASSERT_BREAKPOINT_ENABLED  0x10
  # ASSERT_DEADLOOP_ENABLED    0x20
!if $(DEBUG_ON_SERIAL_PORT) != TRUE
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x21
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2f
!endif

  #  DEBUG_INIT      0x00000001  // Initialization
  #  DEBUG_WARN      0x00000002  // Warnings
  #  DEBUG_LOAD      0x00000004  // Load events
  #  DEBUG_FS        0x00000008  // EFI File system
  #  DEBUG_POOL      0x00000010  // Alloc & Free (pool)
  #  DEBUG_PAGE      0x00000020  // Alloc & Free (page)
  #  DEBUG_INFO      0x00000040  // Informational debug messages
  #  DEBUG_DISPATCH  0x00000080  // PEI/DXE/SMM Dispatchers
  #  DEBUG_VARIABLE  0x00000100  // Variable
  #  DEBUG_BM        0x00000400  // Boot Manager
  #  DEBUG_BLKIO     0x00001000  // BlkIo Driver
  #  DEBUG_NET       0x00004000  // SNP Driver
  #  DEBUG_UNDI      0x00010000  // UNDI Driver
  #  DEBUG_LOADFILE  0x00020000  // LoadFile
  #  DEBUG_EVENT     0x00080000  // Event messages
  #  DEBUG_GCD       0x00100000  // Global Coherency Database changes
  #  DEBUG_CACHE     0x00200000  // Memory range cachability changes
  #  DEBUG_VERBOSE   0x00400000  // Detailed debug messages that may
  #                              // significantly impact boot performance
  #  DEBUG_ERROR     0x80000000  // Error
!if $(DEBUG_ON_SERIAL_PORT) == TRUE
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000047
!endif

  #
  # Optional feature to help prevent EFI memory map fragments
  # Turned on and off via: PcdPrePiProduceMemoryTypeInformationHob
  # Values are in EFI Pages (4K). DXE Core will make sure that
  # at least this much of each type of memory can be allocated
  # from a single memory range. This way you only end up with
  # maximum of two fragments for each type in the memory map
  # (the memory used, and the free memory that was prereserved
  # but not used).
  #
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiACPIReclaimMemory|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiACPIMemoryNVS|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiReservedMemoryType|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesData|300
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesCode|150
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesCode|1000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesData|6000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderCode|20
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderData|0

  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000

!if $(SECURE_BOOT_ENABLE) == TRUE
  gEfiSecurityPkgTokenSpaceGuid.PcdUserPhysicalPresence|TRUE
  # override the default values from SecurityPkg to ensure images from all sources are verified in secure boot
  gEfiSecurityPkgTokenSpaceGuid.PcdOptionRomImageVerificationPolicy|0x04
  gEfiSecurityPkgTokenSpaceGuid.PcdFixedMediaImageVerificationPolicy|0x04
  gEfiSecurityPkgTokenSpaceGuid.PcdRemovableMediaImageVerificationPolicy|0x04
!endif

  # Default platform supported RFC 4646 languages: English & French & Chinese Simplified.
  # Default Value of PlatformLangCodes Variable.
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultPlatformLangCodes|"en-US"

  # Default current RFC 4646 language: Chinese Simplified.
  # Default Value of PlatformLang Variable.
  gEfiMdePkgTokenSpaceGuid.PcdUefiVariableDefaultPlatformLang|"en-US"

[PcdsDynamicHii.common.DEFAULT]
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|L"Timeout"|gEfiGlobalVariableGuid|0x0|3

[PcdsDynamicDefault.common.DEFAULT]
  #
  # If we don't have a variable NV (Non-volatile) storage (such as flash),
  # we need to enable this emulated variable NV mode, otherwise the Variable
  # Write Architectural Protocol won't be installed.
  #
  #   A specific DXE module (such as OvmfPkg/VirtNorFlashDxe/VirtNorFlashDxe.inf):
  #       install gEfiFirmwareVolumeBlockProtocolGuid.
  #   FaultTolerantWriteDxe:
  #       needs gEfiFirmwareVolumeBlockProtocolGuid to install gEfiFaultTolerantWriteProtocolGuid.
  #   VariableRuntimeDxe:
  #       if PcdEmuVariableNvModeEnable == FALSE:
  #           needs gEfiFaultTolerantWriteProtocolGuid to install gEfiVariableWriteArchProtocol.
  #       else:
  #           install gEfiVariableWriteArchProtocol directly.
  #
!if $(EMU_VARIABLE_NV_MODE_ENABLE) == TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
!else
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|FALSE
!endif

  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosVersion|0x0306
  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosDocRev|0x0
  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosEntryPointProvideMethod|0x2

  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVendor|L""
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L""
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareRevision|0x00FF00FF

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform.
#
################################################################################
[Components]
  #
  # SEC Phase modules
  #
  Silicon/Spacemit/Sec/SecMain.inf  {
    <LibraryClasses>
      LzmaDecompressLib|MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf
      PrePiLib|EmbeddedPkg/Library/PrePiLib/PrePiLib.inf
      PrePiHobListPointerLib|OvmfPkg/RiscVVirt/Library/PrePiHobListPointerLib/PrePiHobListPointerLib.inf
  }

  #
  # DXE Phase modules
  #
  MdeModulePkg/Core/Dxe/DxeMain.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/DxeCrc32GuidedSectionExtractLib/DxeCrc32GuidedSectionExtractLib.inf
      DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  }

  #
  # DXE Architectural Protocols
  #
  # Security Architectural
!if $(SECURE_BOOT_ENABLE) == TRUE
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf {
    <LibraryClasses>
      NULL|SecurityPkg/Library/DxeImageVerificationLib/DxeImageVerificationLib.inf
  }
  SecurityPkg/EnrollFromDefaultKeysApp/EnrollFromDefaultKeysApp.inf
!else
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
!endif

  # CPU Architectural
  UefiCpuPkg/CpuDxeRiscV64/CpuDxeRiscV64.inf
  # Metronome Architectural
  #   Use MdeModulePkg/Universal/Metronome/Metronome instead of
  #   EmbeddedPkg/MetronomeDxe/MetronomeDxe, because the former precision is
  #   nanosecond while the latter is microsecond.
  MdeModulePkg/Universal/Metronome/Metronome.inf
  # Timer Architectural
  UefiCpuPkg/CpuTimerDxeRiscV64/CpuTimerDxeRiscV64.inf
  # BDS Architectural
  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
  # Watchdog Timer Architectural
  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  # Runtime Architectural
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  # Variable Architectural and Variable Runtime Architectural
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf {
    <LibraryClasses>
!if $(EMU_VARIABLE_NV_MODE_ENABLE) != TRUE
      NULL|EmbeddedPkg/Library/NvVarStoreFormattedLib/NvVarStoreFormattedLib.inf
!endif
      NULL|MdeModulePkg/Library/VarCheckUefiLib/VarCheckUefiLib.inf
      # do not use unaligned CopyMem () on the UEFI varstore NOR flash region
      BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  }
  # Monotonic Counter Architectural
  MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
  # Reset Architectural
  MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  # Real Time Clock Architectural
  EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  # Capsule Architectural
  MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf

  #
  # Device Path driver
  #
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf {
    <LibraryClasses>
      DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
      DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  }

  #
  # PCD DXE driver
  #
  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf  {
    <LibraryClasses>
      DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
      DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  }

  #
  # FDT client driver
  #
  EmbeddedPkg/Drivers/FdtClientDxe/FdtClientDxe.inf {
    <LibraryClasses>
      DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
      DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
      #
      # TODO:
      #   Remove this EmbeddedPkg/Library/FdtLib/FdtLib.inf declaration after
      #   edk2 repository is updated to a version newer than edk2-stable202505,
      #   since this EmbeddedPkg FdtLib has been replaced with MdePkg BaseFdtLib
      #   in https://github.com/tianocore/edk2/pull/10968.
      #
      FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf
  }

  #
  # FTW (Fault Tolerant Write) DXE driver
  #
  MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf

  #
  # HII (Human Interface Infrastructure) support
  #
  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  #
  # Form Browser Protocol
  #
  Silicon/Spacemit/Override/MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
  #
  # Form Display Engine Protocol
  #
  Silicon/Spacemit/Override/MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf

  #
  # Multiple Console IO support
  #
  MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  Silicon/Spacemit/Override/MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  #
  # Status Code Routing
  #
  MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf

  #
  # GPT/MBR/ElTorito partitioning + Unicode collation + UDF filesystem + FAT filesystem
  #
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf

  #
  # SMBIOS Support
  #
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  Silicon/Spacemit/Drivers/SmbiosMiscDxe/SmbiosMiscDxe.inf

  #
  # Secure Boot Support
  #
!if $(SECURE_BOOT_ENABLE) == TRUE
  SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf
  SecurityPkg/VariableAuthenticated/SecureBootDefaultKeysDxe/SecureBootDefaultKeysDxe.inf
!endif
