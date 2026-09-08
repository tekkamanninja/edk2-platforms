/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

Scope (_SB)
{
  // RISC-V APLIC
  Device (IC00) {
    Name (_HID, "RSCV0002")
    Name (_UID, 0)
    Name (_GSB, 0)  // Global System Interrupt Base for this APLIC starts at 0

    Name (_CRS, ResourceTemplate () {
      QWordMemory (
        ResourceProducer,
        PosDecode,
        MinFixed,
        MaxFixed,
        NonCacheable,
        ReadWrite,
        0x0,               // AddressGranularity, should be 2^n - 1
        0xe0804000,        // AddressMinimum
        0xe0807fff,        // AddressMaximum
        0x00000000000,     // AddressTranslation
        0x4000             // RangeLength
      )
    })
  }
}
