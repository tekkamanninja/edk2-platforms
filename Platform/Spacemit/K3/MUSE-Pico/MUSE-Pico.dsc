## @file
#  RISC-V EFI on SpacemiT K3 MUSE-Pico platform
#
#  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = MUSE-Pico
  PLATFORM_GUID                  = c7e8c815-09a3-4ad7-90a9-e10f262b261c
  PLATFORM_VERSION               = 0.0.1
  DSC_SPECIFICATION              = 0x0001001c
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = RISCV64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = ALL
  FLASH_DEFINITION               = Platform/Spacemit/K3/MUSE-Pico/MUSE-Pico.fdf

  DEFINE DEBUG_ON_SERIAL_PORT        = FALSE
  DEFINE PERFORMANCE_ENABLE          = FALSE
  DEFINE EMU_VARIABLE_NV_MODE_ENABLE = FALSE
  DEFINE CAPSULE_ENABLE              = TRUE
  DEFINE NETWORK_PXE_BOOT_ENABLE     = TRUE
  DEFINE NETWORK_HTTP_BOOT_ENABLE    = TRUE
  DEFINE ACPIVIEW_ENABLE             = FALSE
  DEFINE ACPI_ENABLE                 = FALSE

  POSTBUILD                      = python3 Platform/Spacemit/PostBuild.py --post Platform/Spacemit/K3/MUSE-Pico/PostBuild.cfg

[SkuIds]
  0|DEFAULT
  1|COM260
  2|FML13V05

!include MdePkg/MdeLibs.dsc.inc
!include Silicon/Spacemit/Spacemit.dsc.inc
!include Features/Ext4Pkg/Ext4.dsc.inc

[LibraryClasses.common.SEC]
  # Serial via SBI (without poll)
  SerialPortLib|MdePkg/Library/BaseSerialPortLibRiscVSbiLib/BaseSerialPortLibRiscVSbiLib.inf

  SpacemitSecHelperLib|Silicon/Spacemit/Library/SpacemitSecHelperLib/SpacemitSecHelperLib.inf
  SpacemitSecLib|Silicon/Spacemit/K3/Library/SpacemitSecLib/SpacemitSecLib.inf

[LibraryClasses.common]
  PlatformBootManagerLib|Silicon/Spacemit/Library/PlatformBootManagerLib/PlatformBootManagerLib.inf
  BoardPinctrlMapLib|Platform/Spacemit/K3/MUSE-Pico/Library/BoardPinctrlMapLib/BoardPinctrlMapLib.inf

  NetLib|NetworkPkg/Library/DxeNetLib/DxeNetLib.inf
  ResetSystemLib|Silicon/Spacemit/K3/Library/ResetSystemLib/ResetSystemLib.inf

  RiscVSbiMpxyLib|Silicon/Spacemit/Library/RiscVSbiMpxyLib/RiscVSbiMpxyLib.inf
  RiscVSbiMpxyRpmiLib|Silicon/Spacemit/Library/RiscVSbiMpxyRpmiLib/RiscVSbiMpxyRpmiLib.inf

  # Lib for NonDiscoverableDevice
  NonDiscoverableDeviceRegistrationLib|MdeModulePkg/Library/NonDiscoverableDeviceRegistrationLib/NonDiscoverableDeviceRegistrationLib.inf

  # PCIe support
  PciHostBridgeLib|Silicon/Spacemit/Library/PciHostBridgeLib/PciHostBridgeLib.inf
  PciSegmentLib|Silicon/Spacemit/Library/DesignWarePciSegmentLib/DesignWarePciSegmentLib.inf
  DesignWarePcieControllerLib|Silicon/Spacemit/Library/DesignWarePcieControllerLib/DesignWarePcieControllerLib.inf

  # Serial via SBI (with poll)
  SerialPortLib|MdePkg/Library/BaseSerialPortLibRiscVSbiLib/BaseSerialPortLibRiscVSbiLibRam.inf

  # Lib for USB
  UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf

  # lib for GMAC
  DmaLib|EmbeddedPkg/Library/NonCoherentDmaLib/NonCoherentDmaLib.inf

[PcdsDynamicDefault.common]
  # The seconds that the firmware will wait before initiating the original default boot selection.
  # 0: immediately
  # 0xFFFF: firmware will wait for user input before booting
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0

  # Console output rows and columns. 0 means max.
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutRow|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutColumn|0
  # Video horizontal and vertical resolution. 0 means highest.
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoHorizontalResolution|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdVideoVerticalResolution|0

  # EDK firmware configuration
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVendor|L"SPACEMIT"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"$(PLATFORM_VERSION)"

[PcdsDynamicExDefault.common.DEFAULT]
!if $(CAPSULE_ENABLE)
  gEfiSignedCapsulePkgTokenSpaceGuid.PcdEdkiiSystemFirmwareImageDescriptor|{0x0}|VOID*|0x100
  gEfiSignedCapsulePkgTokenSpaceGuid.PcdEdkiiSystemFirmwareFileGuid|{0x4C, 0x3C, 0x21, 0xB6, 0x43, 0x56, 0xE9, 0x45, 0x8C, 0x57, 0xF6, 0x9A, 0xCC, 0xD2, 0xA4, 0x61}
  gEfiMdeModulePkgTokenSpaceGuid.PcdSystemFmpCapsuleImageTypeIdGuid|{0x3D, 0x1C, 0xEF, 0x6B, 0x2B, 0x13, 0xF0, 0x45, 0x84, 0xF8, 0x5D, 0xB2, 0x33, 0x0A, 0x46, 0xF5}
!endif

[PcdsDynamicDefault.common.DEFAULT]
  # pico-itx / deb1: activate GMAC0 only (bit 0)
  gSpacemitK3TokenSpaceGuid.PcdGmacUseMask|0x01
  # pico-itx / deb1: enable controllers 0,3,4 (PortA + PortD + USB2)
  gSpacemitK3TokenSpaceGuid.PcdUsbHostEnableMask|0x19
  # pico-itx / deb1: limit controllers 0,1,2,4 to HS (bit set = HS-only)
  gSpacemitK3TokenSpaceGuid.PcdUsbHostHsOnlyMask|0x17
  # deb1: enable PCIe Port A + Port B (runtime split by GPIO detection)
  gSpacemitK3TokenSpaceGuid.PcdPcieHostEnableMask|0x03
  # pico-itx / deb1: set pin to low Voltage(1.8v)
  gSpacemitK3TokenSpaceGuid.PcdPcieHostPinState|"lowvoltage"

  # LCD configuration: eDP@pingroup0 or DP@pingroup1
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrderCount|2
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[0].Mode|DpuModeEdp
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[0].DpuId|DPU0
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[0].PinGroup|0
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[1].Mode|DpuModeDp
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[1].DpuId|DPU1
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[1].PinGroup|1

[PcdsDynamicDefault.common.COM260]
  # com260: activate GMAC1 only (bit 1)
  gSpacemitK3TokenSpaceGuid.PcdGmacUseMask|0x02
  # com260: enable controllers 0,1 (PortA + PortB)
  gSpacemitK3TokenSpaceGuid.PcdUsbHostEnableMask|0x03
  # com260: limit controller 0 to HS (bit 0 set = HS-only)
  gSpacemitK3TokenSpaceGuid.PcdUsbHostHsOnlyMask|0x01
  # com260: enable PCIe Port A + Port D (bit 0,3)
  gSpacemitK3TokenSpaceGuid.PcdPcieHostEnableMask|0x09
  # com260: set pin to default Voltage(3.3v)
  gSpacemitK3TokenSpaceGuid.PcdPcieHostPinState|"default"

  # LCD configuration: DP@pingroup2, algin data size with UINT32
  # EDK2 does NOT support field-access syntax overlay; therefore, use a flat structure.
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority|{ 1, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0 }

  # enable SDCRAD support
  gSpacemitTokenSpaceGuid.PcdSdCardIsEnabled|TRUE
  gSpacemitTokenSpaceGuid.PcdSdCardDetectGpioPin|4
  gSpacemitTokenSpaceGuid.PcdSdCardDetectActive|TRUE

  gSpacemitK3TokenSpaceGuid.PcdCtf2301Enable|TRUE

[PcdsDynamicDefault.common.FML13V05]
  # FML13V05 (DeepComputing laptop) hardware configuration
  # no wired ethernet port on this board
  gSpacemitK3TokenSpaceGuid.PcdGmacUseMask|0x00
  # 4x Type-C (PortA DRD + PortB + PortC + PortD) + USB2 internal hub (BT/FG/CAM)
  gSpacemitK3TokenSpaceGuid.PcdUsbHostEnableMask|0x1F
  # USB2 host (controller 4) is HS-only hardware
  gSpacemitK3TokenSpaceGuid.PcdUsbHostHsOnlyMask|0x10
  # enable PCIe Port A (x4, WiFi) + Port E (pcie4_rc, phy5)
  gSpacemitK3TokenSpaceGuid.PcdPcieHostEnableMask|0x11

  # LCD configuration: eDP internal panel + DP1 via Type-C PortD (ANX7447)
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrderCount|2
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[0].Mode|DpuModeEdp
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[0].DpuId|DPU0
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[0].PinGroup|0
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[1].Mode|DpuModeDp
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[1].DpuId|DPU1
  gSpacemitTokenSpaceGuid.PcdDisplayConnectorsPriority.DisplayOrder[1].PinGroup|1

  # enable SD card support (DTS: cd-gpios = <&gpio 88 0>)
  gSpacemitTokenSpaceGuid.PcdSdCardIsEnabled|TRUE
  gSpacemitTokenSpaceGuid.PcdSdCardDetectGpioPin|88
  gSpacemitTokenSpaceGuid.PcdSdCardDetectActive|TRUE

[PcdsFeatureFlag.common]
  gSpacemitTokenSpaceGuid.PcdEscEnterBootMenu|FALSE

[PcdsFixedAtBuild.common]
  # Definition of RISC-V Hart
  gUefiRiscVPlatformPkgTokenSpaceGuid.PcdHartCount|16
  gUefiRiscVPlatformPkgTokenSpaceGuid.PcdBootHartId|0

!if $(PERFORMANCE_ENABLE)
  gEfiMdePkgTokenSpaceGuid.PcdPerformanceLibraryPropertyMask|1
!endif

  # This UEFI memory region is used in SEC phase to create HOBs, load DXE, etc.
  #     Base = PcdSecStackBase + PcdSecStackSize - PcdSecUefiMemorySize
  #     Size = PcdSecUefiMemorySize
  # (The top of the UEFI memory region is reserved for the stack.)
  gSpacemitTokenSpaceGuid.PcdSecStackBase|0x123FF0000
  gSpacemitTokenSpaceGuid.PcdSecStackSize|0x10000
  gSpacemitTokenSpaceGuid.PcdSecUefiMemorySize|0x02000000

  # Indicates if to reset system when memory type information changes.
  # This platform doesn't support S4 state with EDK2, so set it FALSE.
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

  # Set PcdBootManagerMenuFile to UiApp (FILE_GUID = 462CAA21-7614-4503-836E-8AB6F4662331)
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x21, 0xaa, 0x2c, 0x46, 0x14, 0x76, 0x03, 0x45, 0x83, 0x6e, 0x8a, 0xb6, 0xf4, 0x66, 0x23, 0x31 }

  # Configurability to override RISC-V CPU Features
  # BIT 0 = Cache Management Operations. This bit is relevant only if
  # previous stage has feature enabled and user wants to disable it.
  # BIT 1 = Supervisor Time Compare (Sstc). This bit is relevant only if
  # previous stage has feature enabled and user wants to disable it.
  # BIT 2 = Page-Based Memory Types (Pbmt). This bit is relevant only if
  # previous stage has feature enabled and user wants to disable it.
  gEfiMdePkgTokenSpaceGuid.PcdRiscVFeatureOverride|0x07

  # Frequency of the core crystal clock in Hz
  gUefiCpuPkgTokenSpaceGuid.PcdCpuCoreCrystalClockFrequency|24000000

  #
  # Control the maximum SATP mode that MMU allowed to use.
  # 0 - Bare mode.
  # 8 - 39bit mode.
  # 9 - 48bit mode.
  # 10 - 57bit mode.
  #
  gUefiCpuPkgTokenSpaceGuid.PcdCpuRiscVMmuMaxSatpMode|8

  # For MMU type >= sv39, the width of physical address is 56-bit.
  gSpacemitTokenSpaceGuid.PcdMemoryAddressWidthMax|56
  gSpacemitTokenSpaceGuid.PcdIoAddressWidthMax|32

  # for QSPI controller in K3, Spi flash is map to address below
  gSpacemitTokenSpaceGuid.PcdSFMemMapBaseAddress|0xB8000000

  # Enable error status code reporting
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0x07

  gSpacemitK3TokenSpaceGuid.PcdSpacemitMPMURegBase|0xd4050000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitAPMURegBase|0xd4282800
  gSpacemitK3TokenSpaceGuid.PcdSpacemitAPBSpareRegBase|0xd4090000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitAPBClockRegBase|0xd4015000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitMFPRRegBase|0xd401e000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitApbcAsfarRegBase|0xd4015050
  gSpacemitK3TokenSpaceGuid.PcdSpacemitCIURegBase|0xd4282c00
  gSpacemitK3TokenSpaceGuid.PcdSpacemitWDTRegBase|0xd4080000

  # Pcds for DISPLAY
  gSpacemitTokenSpaceGuid.PcdSpacemitMipiDpuRegBase|0xc0340000
  gSpacemitTokenSpaceGuid.PcdSpacemitMipiDsiRegBase|0xd421a800
  gSpacemitK3TokenSpaceGuid.PcdSpacemitDp0RegBase|0xcac84000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitDp1RegBase|0xcac88000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitDpu0RegBase|0xc0340000
  gSpacemitK3TokenSpaceGuid.PcdSpacemitDpu1RegBase|0xc0440000

  # GPIO Controller Configuration
  gSpacemitK3TokenSpaceGuid.PcdGpioControllerBase|0xd4019000
  gSpacemitK3TokenSpaceGuid.PcdGpioControllerCount|1
  gSpacemitK3TokenSpaceGuid.PcdGpioPinCount|128

  # Sd/Sdio/Emmc Host Controller Configuration
  gSpacemitTokenSpaceGuid.PcdSdCardBaseAddress|0xd4280000
  gSpacemitTokenSpaceGuid.PcdSdioBaseAddress|0xd4280800
  gSpacemitTokenSpaceGuid.PcdEmmcBaseAddress|0xd4281000

  # UFS Host Controller Configuration
  gSpacemitK3TokenSpaceGuid.PcdUfsHcBase|0xc0e00000
  gSpacemitK3TokenSpaceGuid.PcdUfsAclkRate|491520000
  gSpacemitK3TokenSpaceGuid.PcdUfsPhyMngBase|0x1b00
  gSpacemitK3TokenSpaceGuid.PcdUfsAtopBase|0x1c00
  gSpacemitK3TokenSpaceGuid.PcdUfsRefClkFreq|0

  # eFuse bank
  gSpacemitK3TokenSpaceGuid.PcdSpacemitEfuseBankBase|0xF0702800

  # NETWORK GMAC
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Num|2
  # GMAC0
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].ClkTuningEnable      | TRUE
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].TxClkFromSoc         | FALSE
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].PhyClkFromSoc        | FALSE
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].ClkTuningWay         | 1
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].TxPhase              | 65
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].RxPhase              | 50
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].Base                 | 0xcac80000
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].CtrlReg              | 0xd4282be4
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].DlineReg             | 0xd4282be8
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].ControllerId         | 0
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].PhyAddr              | 1
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].MaxSpeed             | 1000
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].PhyResetGpioPin      | 15
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].BusClkAndRstName     | "GMAC0"
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].TxClkName            | "GMAC0_TX"
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].PhyClkName           | "GMAC0_PHY"
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[0].PhyMode              | "rgmii"
  # GMAC1
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].ClkTuningEnable      | TRUE
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].TxClkFromSoc         | FALSE
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].PhyClkFromSoc        | FALSE
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].ClkTuningWay         | 1
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].TxPhase              | 47
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].RxPhase              | 53
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].Base                 | 0xcac82000
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].CtrlReg              | 0xd4282bec
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].DlineReg             | 0xd4282bf0
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].ControllerId         | 1
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].PhyAddr              | 1
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].MaxSpeed             | 1000
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].PhyResetGpioPin      | 37
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].BusClkAndRstName     | "GMAC1"
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].TxClkName            | "GMAC1_TX"
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].PhyClkName           | "GMAC1_PHY"
  gSpacemitK3TokenSpaceGuid.PcdDwEqosControllerConfigs.Data[1].PhyMode              | "rgmii"

  # Pcds for USB
  ## Fixed controller hardware (SoC-level, same for all K3 boards)
  ## Board-level filtering is done via PcdUsbHostEnableMask / PcdUsbHostHsOnlyMask.
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Num|5
  # Controller[0]: USB3.0 DRD Port A
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].MaxSpeed|3
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].ClockResetName|"USB3_PORTA"
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].ControllerBase|0xcad00000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].UtmiPhyBase|0xcad20000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].PipePhyBase|0xcad30000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].PipePhy1Base|0xcad40000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[0].HsForceDisableU3Phy|TRUE
  # Controller[1]: USB3.0 Host Port B
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].MaxSpeed|3
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].ClockResetName|"USB3_PORTB"
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].ControllerBase|0x81400000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].UtmiPhyBase|0x81500000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].PipePhyBase|0x81F00000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].IsCombo|TRUE
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[1].PhySelBit|2
  # Controller[2]: USB3.0 Host Port C
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].MaxSpeed|3
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].ClockResetName|"USB3_PORTC"
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].ControllerBase|0x81700000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].UtmiPhyBase|0x81800000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].PipePhyBase|0x82000000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].IsCombo|TRUE
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[2].PhySelBit|1
  # Controller[3]: USB3.0 Host Port D
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].MaxSpeed|3
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].ClockResetName|"USB3_PORTD"
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].ControllerBase|0x81a00000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].UtmiPhyBase|0x81b00000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].PipePhyBase|0x82100000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].IsCombo|TRUE
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[3].PhySelBit|0
  # Controller[4]: USB2.0 Host
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[4].MaxSpeed|2
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[4].ClockResetName|"USB2"
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[4].ControllerBase|0xc0a00000
  gSpacemitK3TokenSpaceGuid.PcdUsbHostControllers.Controller[4].UtmiPhyBase|0xc0a20000

  gSpacemitK3TokenSpaceGuid.PcdUsbHostPortaSwitchBase|0xd4282910

  # QSPI controller Configuration
  gSpacemitTokenSpaceGuid.PcdSTQspiRegBase|0xd420c000
  gSpacemitTokenSpaceGuid.PcdSTQspiMaxFrequency|25000000
  gSpacemitTokenSpaceGuid.PcdSTQspiControllerId|0

  # I2C controller configuration
  # i2c1/i2c2 enabled, eeprom@0x50 on i2c2
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Num|3
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[0].ControllerId|1
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[0].BaseAddress|0xd4011000
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[0].ClockRate|100000
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[0].Enable|TRUE
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[1].ControllerId|2
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[1].BaseAddress|0xd4012000
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[1].ClockRate|100000
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[1].Enable|TRUE
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[2].ControllerId|6
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[2].BaseAddress|0xd4018800
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[2].ClockRate|100000
  gSpacemitTokenSpaceGuid.PcdI2cControllerConfigs.Data[2].Enable|TRUE

  # I2C slave configuration
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Num|3
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Data[0].BusNumber|1
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Data[0].SlaveAddress|0x25
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Data[1].BusNumber|2
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Data[1].SlaveAddress|0x50
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Data[2].BusNumber|6
  gSpacemitTokenSpaceGuid.PcdI2cSlaveConfig.Data[2].SlaveAddress|0x4c

  # EEPROM configuration (AT24C02 on i2c2@0x50)
  gSpacemitTokenSpaceGuid.PcdEepromConfigs.Num|1
  gSpacemitTokenSpaceGuid.PcdEepromConfigs.Data[0].BusNumber|2
  gSpacemitTokenSpaceGuid.PcdEepromConfigs.Data[0].SlaveAddress|0x50
  gSpacemitTokenSpaceGuid.PcdEepromConfigs.Data[0].AddressWidth|1
  gSpacemitTokenSpaceGuid.PcdEepromConfigs.Data[0].PageSize|8

  # FAN configuration (CTF2301 on i2c6@0x4c)
  gSpacemitK3TokenSpaceGuid.PcdCtf2301Configs.Num|1
  gSpacemitK3TokenSpaceGuid.PcdCtf2301Configs.Data[0].BusNumber|6
  gSpacemitK3TokenSpaceGuid.PcdCtf2301Configs.Data[0].SlaveAddress|0x4C

  # USB Pin&GPIO Configs
  gSpacemitK3TokenSpaceGuid.PcdUsbHostVbusConfigs.Num|0

  # eDP GPIO configuration
  gSpacemitTokenSpaceGuid.PcdDpGpioPowerPin|101
  gSpacemitTokenSpaceGuid.PcdDpGpioEnablePin|118
  gSpacemitTokenSpaceGuid.PcdDpGpioBlPin|106

  #
  # PCIe Configuration
  #
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Num|5
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayNum|5

  # PCIe 0 (Port A)
  # Controller registers
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].Reg.DbiBase|0x80000000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].Reg.DbiSize|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].Reg.Dbi2Base|0x80100000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].Reg.Dbi2Size|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].Reg.AtuBase|0x80300000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].Reg.AtuSize|0x4000
  # Controller capabilities
  # deb1: port A is wired as x4; if device-detect is active, firmware will split A/B into x2/x2.
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].NumLanes|4
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].ControllerMode|0
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[0].CfgShiftModeEnabled|FALSE
  # Root Bridge resources
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Segment|0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].ConfigBase|0x1100000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].ConfigSize|0x10000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].BusBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].BusLimit|0xFF
  # IO Space: Disabled (RISC-V uses MMIO only)
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Io.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Io.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Io.CpuBase|0x0
  # MMIO Space (32-bit non-prefetchable)
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Mem.PciBase|0x00110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Mem.PciSize|0x7FEF0000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Mem.CpuBase|0x1100110000
  # MMIO64 Space: Disabled
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Mem64.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Mem64.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].Mem64.CpuBase|0x0
  # Prefetchable MMIO64 Space: 4GB (iATU single window limit)
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].PMem64.PciBase|0x1800000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].PMem64.PciSize|0x100000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[0].PMem64.CpuBase|0x1800000000

  # PCIe 1 (Port B)
  # Controller registers
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].Reg.DbiBase|0x80400000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].Reg.DbiSize|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].Reg.Dbi2Base|0x80500000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].Reg.Dbi2Size|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].Reg.AtuBase|0x80700000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].Reg.AtuSize|0x4000
  # Controller capabilities
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].NumLanes|2
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].ControllerMode|0
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[1].CfgShiftModeEnabled|FALSE
  # Root Bridge resources
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Segment|1
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].ConfigBase|0x1180000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].ConfigSize|0x10000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].BusBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].BusLimit|0xFF
  # IO Space: Disabled (RISC-V uses MMIO only)
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Io.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Io.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Io.CpuBase|0x0
  # MMIO Space (32-bit non-prefetchable):
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Mem.PciBase|0x80110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Mem.PciSize|0x7FEF0000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Mem.CpuBase|0x1180110000
  # MMIO64 Space: Disabled
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Mem64.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Mem64.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].Mem64.CpuBase|0x0
  # Prefetchable MMIO64 Space: 4GB (iATU single window limit)
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].PMem64.PciBase|0x1600000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].PMem64.PciSize|0x100000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[1].PMem64.CpuBase|0x1600000000

  # PCIe 2 (Port C)
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].Reg.DbiBase|0x80800000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].Reg.DbiSize|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].Reg.Dbi2Base|0x80900000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].Reg.Dbi2Size|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].Reg.AtuBase|0x80B00000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].Reg.AtuSize|0x4000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].NumLanes|2
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].ControllerMode|0
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[2].CfgShiftModeEnabled|FALSE
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Segment|2
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].ConfigBase|0x1200000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].ConfigSize|0x10000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].BusBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].BusLimit|0xFF
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Io.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Io.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Io.CpuBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Mem.PciBase|0x00110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Mem.PciSize|0x7FEF0000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Mem.CpuBase|0x1200110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Mem64.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Mem64.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].Mem64.CpuBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].PMem64.PciBase|0x1500000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].PMem64.PciSize|0x100000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[2].PMem64.CpuBase|0x1500000000

  # PCIe 3 (Port D)
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].Reg.DbiBase|0x80C00000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].Reg.DbiSize|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].Reg.Dbi2Base|0x80D00000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].Reg.Dbi2Size|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].Reg.AtuBase|0x80F00000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].Reg.AtuSize|0x4000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].NumLanes|1
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].ControllerMode|0
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[3].CfgShiftModeEnabled|FALSE
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Segment|3
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].ConfigBase|0x1280000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].ConfigSize|0x10000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].BusBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].BusLimit|0xFF
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Io.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Io.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Io.CpuBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Mem.PciBase|0x80110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Mem.PciSize|0x3FEF0000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Mem.CpuBase|0x1280110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Mem64.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Mem64.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].Mem64.CpuBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].PMem64.PciBase|0x1400000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].PMem64.PciSize|0x100000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[3].PMem64.CpuBase|0x1400000000

  # PCIe 4 (Port E)
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].Reg.DbiBase|0x81000000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].Reg.DbiSize|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].Reg.Dbi2Base|0x81100000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].Reg.Dbi2Size|0x1000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].Reg.AtuBase|0x81300000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].Reg.AtuSize|0x4000
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].NumLanes|1
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].ControllerMode|0
  gSpacemitTokenSpaceGuid.PcdDwPcieControllerConfigTable.Data[4].CfgShiftModeEnabled|FALSE
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Segment|4
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].ConfigBase|0x12C0000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].ConfigSize|0x10000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].BusBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].BusLimit|0xFF
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Io.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Io.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Io.CpuBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Mem.PciBase|0xC0110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Mem.PciSize|0x3FEF0000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Mem.CpuBase|0x12C0110000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Mem64.PciBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Mem64.PciSize|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].Mem64.CpuBase|0x0
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].PMem64.PciBase|0x1300000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].PMem64.PciSize|0x100000000
  gSpacemitTokenSpaceGuid.PcdBoardPciRootBridgeResourceConfigTable.ArrayData[4].PMem64.CpuBase|0x1300000000

  #
  # K3 PCIe PHY resources
  #
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Num|6
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[0].PhyId|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[0].NumLanes|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[0].PhyBase|0x81D00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[1].PhyId|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[1].NumLanes|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[1].PhyBase|0x81E00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[2].PhyId|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[2].NumLanes|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[2].PhyBase|0x81F00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[3].PhyId|3
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[3].NumLanes|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[3].PhyBase|0x82000000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[4].PhyId|4
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[4].NumLanes|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[4].PhyBase|0x82100000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[5].PhyId|5
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[5].NumLanes|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePhyConfigs.Phy[5].PhyBase|0x82200000

  #
  # K3 PCIe per-port wrapper/PHY wiring
  #
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Num|5
  # Port A
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].PortId|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].AppBase|0xD42829F0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].PhyAhbBase|0x82900000
  # deb1: spacemit,device-detect = <&gpio 89 0> (active-high)
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].DeviceDetectGpio|89
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].DeviceDetectActiveLevel|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].NumPhys|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].PhyIndex[0]|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[0].PhyIndex[1]|1
  # Port B
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].PortId|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].AppBase|0xD42829D0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].PhyAhbBase|0x82C00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].DeviceDetectGpio|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].DeviceDetectActiveLevel|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].NumPhys|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[1].PhyIndex[0]|1
  # Port C
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].PortId|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].AppBase|0xD42829C8
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].PhyAhbBase|0x82D00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].DeviceDetectGpio|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].DeviceDetectActiveLevel|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].NumPhys|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].PhyIndex[0]|2
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[2].PhyIndex[1]|3
  # Port D
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].PortId|3
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].AppBase|0xD42829E0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].PhyAhbBase|0x82A00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].DeviceDetectGpio|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].DeviceDetectActiveLevel|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].NumPhys|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[3].PhyIndex[0]|4
  # Port E
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].PortId|4
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].AppBase|0xD42829E8
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].PhyAhbBase|0x82B00000
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].DeviceDetectGpio|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].DeviceDetectActiveLevel|0
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].NumPhys|1
  gSpacemitK3TokenSpaceGuid.PcdK3PciePortConfigs.Port[4].PhyIndex[0]|5

  # SDCARD configuration
  gSpacemitTokenSpaceGuid.PcdSdCardClockRate|208000000
  # Tx Delaycode configuration
  gSpacemitTokenSpaceGuid.PcdSdCardTxDelayCode|31

  # EMMC configuration
  gSpacemitTokenSpaceGuid.PcdEmmcClockRate|208000000

!if $(ACPI_ENABLE) == TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiExposedTableVersions|0x20
  # definition of X100
  gSpacemitK3TokenSpaceGuid.PcdX100HartCount|0x8

!endif # ACPI_ENABLE

[Components]
  # SKU selection (must run before any SKU-sensitive driver)
  Silicon/Spacemit/K3/Drivers/SkuSelectDxe/SkuSelectDxe.inf

  # RPMI Voltage
  Silicon/Spacemit/Drivers/RiscvSbiMpxyRpmiVoltageDxe/RiscvSbiMpxyRpmiVoltageDxe.inf

  # system hardware module
  Silicon/Spacemit/K3/Drivers/ClockDxe/ClockDxe.inf
  Silicon/Spacemit/K3/Drivers/PinCtrlDxe/PinCtrlDxe.inf

  #
  # Firmware Volume Block service
  #
  Silicon/Spacemit/Drivers/FlashFvbDxe/FlashFvbDxe.inf

  # GPIO support
  #
  Silicon/Spacemit/K3/Drivers/GPIO/GpioDxe.inf

  # I2C support
  MdeModulePkg/Bus/I2c/I2cDxe/I2cDxe.inf
  Silicon/Spacemit/Drivers/I2CMaster/I2CMaster/I2cMaster.inf

  # EEPROM support
  Silicon/Spacemit/Drivers/I2CMaster/Eeprom/EepromDxe.inf

  # TLV EEPROM support
  Silicon/Spacemit/Drivers/I2CMaster/Tlv_Eeprom/TlvEeprom.inf

  # CTF2301 FAN (I2C)
  Silicon/Spacemit/K3/Drivers/Ctf2301Dxe/Ctf2301Dxe.inf

  # platform info
  Silicon/Spacemit/K3/Drivers/PlatformInfoDxe/PlatformInfoDxe.inf

  # eFuse read protocol
  Silicon/Spacemit/K3/Drivers/EfuseDxe/EfuseDxe.inf

  #
  # Spinor flash support
  #
  Silicon/Spacemit/Drivers/Spi/STQspiDxe/QspiDxe.inf
  Silicon/Spacemit/Drivers/Spi/SpiNorFlashDxe/SpiNorFlashDxe.inf

  #
  # USB Support
  #
  MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf
  Silicon/Spacemit/Override/MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
  MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf
  Silicon/Spacemit/K3/Drivers/UsbHcdInitDxe/UsbHcd.inf

  #
  # SD/MMC support
  #
  MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
  MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
  Silicon/Spacemit/Override/MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/SdMmcPciHcDxe.inf
  Silicon/Spacemit/Drivers/SdhciDxe/SdhciDxe.inf

  #
  # GOP support
  #
  Silicon/Spacemit/Drivers/LcdGraphicsOutputDxe/LcdGraphicsOutputDxe.inf
  Silicon/Spacemit/K3/Drivers/DpuDxe/DpuDxe.inf
  Silicon/Spacemit/K3/Drivers/LcdDpDxe/LcdDpDxe.inf

  # boot logo
  Silicon/Spacemit/Drivers/LogoDxe/LogoDxe.inf

  # PCIe support
  #
  Silicon/Spacemit/Drivers/PciCpuIo2Dxe/PciCpuIo2Dxe.inf
  Silicon/Spacemit/Override/MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciHostBridgeDxe.inf {
    <LibraryClasses>
      NULL|Silicon/Spacemit/K3/Library/K3PcieHostBridgeLib/K3PcieHostBridgeLib.inf
  }
  Silicon/Spacemit/Override/MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf

  #
  # NVMe support (for PCIe NVMe storage)
  #
  MdeModulePkg/Bus/Pci/NvmExpressDxe/NvmExpressDxe.inf

  #
  # UFS support
  #
  Silicon/Spacemit/K3/Drivers/UfsHcDxe/SpacemitK3UfsHcDxe.inf
  MdeModulePkg/Bus/Ufs/UfsPassThruDxe/UfsPassThruDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiBusDxe/ScsiBusDxe.inf
  MdeModulePkg/Bus/Scsi/ScsiDiskDxe/ScsiDiskDxe.inf

  # Network support
!include NetworkPkg/Network.dsc.inc
  Silicon/Spacemit/K3/Drivers/DwEqosDxe/DwEqosDxe.inf
  ShellPkg/DynamicCommand/TftpDynamicCommand/TftpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  ShellPkg/DynamicCommand/HttpDynamicCommand/HttpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  OvmfPkg/LinuxInitrdDynamicShellCommand/LinuxInitrdDynamicShellCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
  ShellPkg/Application/Shell/Shell.inf {
    <LibraryClasses>
      ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
      NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellDriver1CommandsLib/UefiShellDriver1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellDebug1CommandsLib/UefiShellDebug1CommandsLib.inf
!if $(ACPIVIEW_ENABLE) == TRUE
      NULL|ShellPkg/Library/UefiShellAcpiViewCommandLib/UefiShellAcpiViewCommandLib.inf
!endif
      NULL|ShellPkg/Library/UefiShellInstall1CommandsLib/UefiShellInstall1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellNetwork1CommandsLib/UefiShellNetwork1CommandsLib.inf
      NULL|Silicon/Spacemit/Applications/I2cTool/I2cCmd.inf
      NULL|Silicon/Spacemit/Applications/EepromTool/EepromCmd.inf
      NULL|Silicon/Spacemit/Applications/TlvTool/TlvCmd.inf
#!if $(NETWORK_IP6_ENABLE) == TRUE
      NULL|ShellPkg/Library/UefiShellNetwork2CommandsLib/UefiShellNetwork2CommandsLib.inf
#!endif
      HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
      PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
      BcfgCommandLib|ShellPkg/Library/UefiShellBcfgCommandLib/UefiShellBcfgCommandLib.inf

    <PcdsFixedAtBuild>
      gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0xFF
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
      gEfiMdePkgTokenSpaceGuid.PcdUefiLibMaxPrintBufferSize|8000
  }

  #
  # Performance Application
  #
!if $(PERFORMANCE_ENABLE)
  ShellPkg/DynamicCommand/DpDynamicCommand/DpDynamicCommand.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
!endif

  # UiApp
  MdeModulePkg/Application/UiApp/UiApp.inf {
    <LibraryClasses>
      NULL|Silicon/Spacemit/Library/PlatformUiLib/PlatformManagerUiLib.inf
      NULL|MdeModulePkg/Library/DeviceManagerUiLib/DeviceManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootManagerUiLib/BootManagerUiLib.inf
      NULL|MdeModulePkg/Library/BootMaintenanceManagerUiLib/BootMaintenanceManagerUiLib.inf
  }

  #
  # ACPI support
  #
!if $(ACPI_ENABLE) == TRUE
  #
  # ACPI
  #
  # Produce gEfiAcpiTableProtocolGuid and gEfiAcpiSdtProtocolGuid
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  # Find ACPI tables and intall to ACPI-memory
  MdeModulePkg/Universal/Acpi/AcpiPlatformDxe/AcpiPlatformDxe.inf
  # ACPI tables
  Platform/Spacemit/K3/AcpiTables/K3AcpiTables.inf
!else # ACPI_ENABLE
  Silicon/RISC-V/ProcessorPkg/Universal/FdtDxe/FdtDxe.inf {
    <LibraryClasses>
      RiscVCpuLib|Silicon/RISC-V/ProcessorPkg/Library/RiscVCpuLib/RiscVCpuLib.inf
      #
      # TODO:
      #   Remove this EmbeddedPkg/Library/FdtLib/FdtLib.inf declaration after
      #   edk2 repository is updated to a version newer than edk2-stable202505,
      #   since this EmbeddedPkg FdtLib has been replaced with MdePkg BaseFdtLib
      #   in https://github.com/tianocore/edk2/pull/10968.
      #
      FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf
  }
!endif # ACPI_ENABLE

  # Device tree for K3
  Platform/Spacemit/K3/DeviceTree/K3DeviceTree.inf

  # FDT fixup protocol
  Silicon/Spacemit/Drivers/FdtFixupDxe/FdtFixupDxe.inf {
    <LibraryClasses>
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
  # Firmware update
  #
!if $(CAPSULE_ENABLE)
  MdeModulePkg/Application/CapsuleApp/CapsuleApp.inf
  Platform/Spacemit/K3/MUSE-Pico/Feature/Capsule/SystemFirmwareDescriptor/SystemFirmwareDescriptor.inf
  SignedCapsulePkg/Universal/SystemFirmwareUpdate/SystemFirmwareReportDxe.inf{
    <LibraryClasses>
      # Add a dependency on gSpacemitSystemFirmwareDescriptorReadyProtocolGuid
      NULL|Silicon/Spacemit/Library/SystemFirmwareDescriptorReadyLib/SystemFirmwareDescriptorReadyLib.inf
  }
  MdeModulePkg/Universal/EsrtDxe/EsrtDxe.inf
  SignedCapsulePkg/Universal/SystemFirmwareUpdate/SystemFirmwareUpdateDxe.inf
!endif

  # RNG support
  Silicon/Spacemit/Drivers/PseudoRngDxe/PseudoRngDxe.inf
