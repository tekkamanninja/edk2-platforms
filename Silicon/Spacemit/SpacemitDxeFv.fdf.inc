#
#  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

INF  MdeModulePkg/Core/Dxe/DxeMain.inf

#
# DXE Architectural Protocols
#
INF  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
INF  UefiCpuPkg/CpuDxeRiscV64/CpuDxeRiscV64.inf
INF  MdeModulePkg/Universal/Metronome/Metronome.inf
INF  UefiCpuPkg/CpuTimerDxeRiscV64/CpuTimerDxeRiscV64.inf
INF  MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
INF  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
INF  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
INF  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
INF  MdeModulePkg/Universal/MonotonicCounterRuntimeDxe/MonotonicCounterRuntimeDxe.inf
INF  MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
INF  EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
INF  MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf

#
# Device Path driver
#
INF  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf

#
# PCD DXE driver
#
INF  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

#
# FDT client driver
#
INF EmbeddedPkg/Drivers/FdtClientDxe/FdtClientDxe.inf

#
# FTW (Fault Tolerant Write) DXE driver
#
INF MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteDxe.inf

#
# HII (Human Interface Infrastructure) support
#
INF  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

#
# Form Browser Protocol
#
INF  Silicon/Spacemit/Override/MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf
#
# Form Display Engine Protocol
#
INF  Silicon/Spacemit/Override/MdeModulePkg/Universal/DisplayEngineDxe/DisplayEngineDxe.inf

#
# Multiple Console IO support
#
INF  MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
INF  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
INF  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
INF  Silicon/Spacemit/Override/MdeModulePkg/Universal/Console/TerminalDxe/TerminalDxe.inf
INF  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

#
# Status Code Routing
#
INF  MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
INF  MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf

#
# GPT/MBR partitioning + Unicode collation + UDF filesystem + FAT filesystem
#
INF  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
INF  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
INF  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
INF  MdeModulePkg/Universal/Disk/UdfDxe/UdfDxe.inf
INF  FatPkg/EnhancedFatDxe/Fat.inf

#
# SMBIOS Support
#
INF  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
INF  Silicon/Spacemit/Drivers/SmbiosMiscDxe/SmbiosMiscDxe.inf

#
# Secure Boot Support
#
!if $(SECURE_BOOT_ENABLE) == TRUE
INF  SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigDxe.inf
INF  SecurityPkg/VariableAuthenticated/SecureBootDefaultKeysDxe/SecureBootDefaultKeysDxe.inf
!endif
