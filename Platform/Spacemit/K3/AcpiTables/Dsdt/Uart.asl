/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

Scope (_SB)
{
  // spacemit UART
  Device (COM0) {
    Name (_HID, "RSCV0003")
    Name (_UID, 0)

    Name (_DEP, Package () { \_SB.IC00 })   // Depends on the APLIC

    Name (_CRS, ResourceTemplate () {
      QWordMemory (
        ResourceProducer,
        PosDecode,
        MinFixed,
        MaxFixed,
        NonCacheable,
        ReadWrite,
        0x0,                  // AddressGranularity, should be 2^n - 1
        0xD4017000,           // AddressMinimum
        0xD40170FF,           // AddressMaximum
        0x00000000000,        // AddressTranslation
        0x100                 // RangeLength
        )
      Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 42 }
    })

    Name (_DSD, Package () {
      ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
      Package () {
        Package (2) { "clock-frequency", 14750000 },
        Package (2) { "reg-shift", 2 },
        Package (2) { "reg-io-width", 4 }
      }
    })
  }
}
