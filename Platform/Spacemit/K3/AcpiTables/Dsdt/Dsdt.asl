/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "K3PlatformAcpi.h"

DefinitionBlock ("DsdtTable.aml", "DSDT", 2, "SPMT  ", "K3", EFI_ACPI_OEM_REVISION) {
  include ("Cpu.asl")
  include ("Uart.asl")
  include ("Aplic.asl")
  include ("Pci.asl")
  include ("Usb.asl")
}
