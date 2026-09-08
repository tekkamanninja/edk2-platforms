## @file
#
#  Capsule for RISC-V EFI on SpacemiT K3 MUSE-Pico platform
#
#  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = MUSE-Pico
  PLATFORM_GUID                  = D127A1A1-DECE-44E3-B0E9-9BEF9F97E2BA
  PLATFORM_VERSION               = 0.0.1
  DSC_SPECIFICATION              = 0x0001001c
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = RISCV64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/Spacemit/K3/MUSE-Pico/MUSE-PicoCapsule.fdf
