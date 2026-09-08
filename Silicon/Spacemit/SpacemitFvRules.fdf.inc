#
#  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

[Rule.Common.SEC]
  FILE SEC = $(NAMED_GUID) RELOCS_STRIPPED FIXED {
    PE32     PE32    Align = Auto     $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING ="$(MODULE_NAME)" Optional
    VERSION  STRING ="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.DXE_CORE]
  FILE DXE_CORE = $(NAMED_GUID) {
    PE32     PE32    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.DXE_DRIVER]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX    DXE_DEPEX Optional      $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32     PE32    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.DXE_RUNTIME_DRIVER]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX    DXE_DEPEX Optional      $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32     PE32    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_DRIVER]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX    DXE_DEPEX Optional      $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32     PE32   $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_DRIVER.BINARY]
  FILE DRIVER = $(NAMED_GUID) {
    DXE_DEPEX DXE_DEPEX Optional      |.depex
    PE32      PE32                    |.efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_APPLICATION]
  FILE APPLICATION = $(NAMED_GUID) {
    PE32     PE32    $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI       STRING="$(MODULE_NAME)" Optional
    VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_APPLICATION.BINARY]
  FILE APPLICATION = $(NAMED_GUID) {
    PE32      PE32                    |.efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.UEFI_APPLICATION.UI]
  FILE APPLICATION = $(NAMED_GUID) {
    PE32      PE32   $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI        STRING="Enter Setup"
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }

[Rule.Common.USER_DEFINED.ACPITABLE]
  FILE FREEFORM = $(NAMED_GUID) {
    RAW ACPI  Optional     |.acpi
    RAW ASL   Optional     |.aml
    UI        STRING="$(MODULE_NAME)" Optional
  }

[Rule.Common.DXE_DRIVER.FMP_IMAGE_DESC]
  FILE DRIVER = $(NAMED_GUID) {
     RAW BIN                  |.acpi
     DXE_DEPEX DXE_DEPEX Optional        $(INF_OUTPUT)/$(MODULE_NAME).depex
     PE32      PE32                      $(INF_OUTPUT)/$(MODULE_NAME).efi
     UI       STRING="$(MODULE_NAME)" Optional
     VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }