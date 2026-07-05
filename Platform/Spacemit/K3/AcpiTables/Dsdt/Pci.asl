/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#define PCI_OSC()                                                             \
  Name (SUPP, 0)  /* Object to save PCI _OSC Support Field value */           \
  Name (CTRL, 0)  /* Object to save PCI _OSC Control Field value */           \
  Method (_OSC, 4, NotSerialized) {                                           \
    /* DWORD1 in Capabilities Buffer: status and error information */         \
    CreateDWordField (Arg3, 0, CDW1)                                          \
                                                                              \
    If ((Arg0 == ToUUID ("33DB4D5B-1FF7-401C-9657-7441C03DD766"))) {          \
      /* DWORD2 in Capabilities Buffer: Support Field */                      \
      CreateDWordField (Arg3, 4, CDW2)                                        \
      /* DWORD3 in Capabilities Buffer: Control Field */                      \
      CreateDWordField (Arg3, 8, CDW3)                                        \
                                                                              \
      /* Save Support Field and Control Field values */                       \
      SUPP = CDW2                                                             \
      CTRL = CDW3                                                             \
                                                                              \
      /* Allow OS control for 5 features: */                                  \
      /*  PCIe Hot Plug, SHPC Hot Plug, PME, AER, PCIe Capability */          \
      CTRL &= 0x1f                                                            \
                                                                              \
      /* Unknown revision */                                                  \
      if ((Arg1 != 1)) {                                                      \
        CDW1 |= 0x08                                                          \
      }                                                                       \
      /* Some capabilities bits set by OS were cleared by firmware */         \
      if ((CDW3 != CTRL)) {                                                   \
        CDW1 |= 0x10                                                          \
      }                                                                       \
      /* Update DWORD3 in Capabilities Buffer */                              \
      CDW3 = CTRL                                                             \
                                                                              \
      Return (Arg3)                                                           \
                                                                              \
    } Else {  /* Unrecognized UUID */                                         \
      CDW1 |= 0x04                                                            \
      Return (Arg3)                                                           \
    }                                                                         \
  } /* End _OSC */

Scope (_SB)
{
  Device (PCI0) {
    Name (_HID, "PNP0A08")    // PCI Express Root Bridge
    Name (_CID, "PNP0A03")    // Compatible PCI Root Bridge
    Name (_UID, 0)            // Unique ID
    Name (_SEG, 0)            // PCI Segment Group Number
    Name (_BBN, 0x00)         // PCI Base Bus Number
    Name (_CCA, 0)            // Cache Coherency Attribute

    Name (_CRS, ResourceTemplate () {
      WordBusNumber (
        ResourceProducer, MinFixed, MaxFixed, PosDecode,
        0x0,    // AddressGranularity
        0x00,   // AddressMinimum
        0xff,   // AddressMaximum
        0x00,   // AddressTranslation
        0x100   // RangeLength
      )

      // RISC-V BRS Spec v0.0.2 AML_020 requires that the IO space window
      // SHOULD NOT (NOT RECOMMENDED) be defined.

      QWordMemory (    // non-prefetchable memory
        ResourceProducer, PosDecode, MinFixed, MaxFixed,
        NonCacheable, ReadWrite,
        0x00000000,           // AddressGranularity, should be 2^n - 1
        0x0010000000,         // PCI AddressMinimum
        0x007FFFFFFF,         // PCI AddressMaximum
        0x1100000000,         // AddressTranslation
        0x0070000000          // RangeLength
      )

      QWordMemory (    // 64-bit prefetchable memory:
        ResourceProducer, PosDecode, MinFixed, MaxFixed,
        Prefetchable, ReadWrite,
        0x0,                   // AddressGranularity, should be 2^n - 1
        0x0000001800000000,    // AddressMinimum
        0x00000018FFFFFFFF,    // AddressMaximum
        0x0000000000000000,    // AddressTranslation
        0x0000000100000000     // RangeLength
      )
    })

    PCI_OSC ()

    // Reserve the ECAM region declared in MCFG.
    Device (RES0) {
      Name (_HID, "PNP0C02")
      Name (_CRS, ResourceTemplate () {
        QWordMemory (
          ResourceProducer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0,                   // AddressGranularity, should be 2^n - 1
          0x0000001100000000,    // AddressMinimum
          0x000000110FFFFFFF,    // AddressMaximum
          0x0000000000000000,    // AddressTranslation
          0x0000000010000000     // RangeLength
        )
      })
    }
  } // End of PCI0

  // PCIe host bridge 0 config space accessed over DBI interfaces.
  Device (DBI0) {
    Name (_HID, "AMZN0001")
    Name (_CID, "PNP0C02")
    Name (_UID, 0)
    Name (_CRS, ResourceTemplate () {
      QWordMemory (
        ResourceProducer,
        PosDecode,
        MinFixed,
        MaxFixed,
        NonCacheable,
        ReadWrite,
        0x0,           // AddressGranularity, should be 2^n - 1
        0x80000000,    // AddressMinimum
        0x80000FFF,    // AddressMaximum
        0x00000000,    // AddressTranslation
        0x1000         // RangeLength
      )
    })
  } // End DBI0
}
