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
  // PCIe host bridge 1
  Device (PCI1) {
    Name (_HID, "PNP0A08")    // PCI Express Root Bridge
    Name (_CID, "PNP0A03")    // Compatible PCI Root Bridge
    Name (_UID, 0)      // Unique ID
    Name (_SEG, 1)      // PCI Segment Group Number
    Name (_BBN, 0x00)   // PCI Base Bus Number
    Name (_CCA, 0)      // Cache Coherency Attribute

    Name (_CRS, ResourceTemplate () {
      WordBusNumber (   // Bus numbers assigned to this root
        ResourceProducer,
        MinFixed,
        MaxFixed,
        PosDecode,
        0x0,          // AddressGranularity, should be 2^n - 1
        0x00,         // AddressMinimum
        0xff,         // AddressMaximum
        0x00,         // AddressTranslation
        0x100         // RangeLength
      )

      // RISC-V BRS Spec v0.0.2 AML_020 requires that the IO space window
      // SHOULD NOT (NOT RECOMMENDED) be defined.
#if 1
      QWordIO (   // IO space
        ResourceProducer,
        MinFixed,
        MaxFixed,
        PosDecode,
        EntireRange,
        0x0,            // AddressGranularity, should be 2^n - 1
        0x9F100000,     // AddressMinimum
        0x9F10FFFF,     // AddressMaximum
        0,              // AddressTranslation
        0x10000         // RangeLength
      )
#endif

      QWordMemory (   // non-prefetchable memory space
        ResourceProducer,
        PosDecode,
        MinFixed,
        MaxFixed,
        NonCacheable,
        ReadWrite,
        0x0,               // AddressGranularity, should be 2^n - 1
        0x90000000,        // AddressMinimum
        0x9EFFFFFF,        // AddressMaximum
        0,                 // AddressTranslation
        0xF000000          // RangeLength
      )
    })

    PCI_OSC ()

    // Reserved memory for ECAM region in MCFG
    Device (RES1) {
      Name (_HID, "PNP0C02")  // PNP Motherboard Resource
      Name (_CRS, ResourceTemplate () {
        QWordMemory (
          ResourceProducer,
          PosDecode,
          MinFixed,
          MaxFixed,
          NonCacheable,
          ReadWrite,
          0x0,            // AddressGranularity, should be 2^n - 1
          0x9F000000,     // AddressMinimum
          0x9F0FFFFF,     // AddressMaximum
          0x00000000,     // AddressTranslation
          0x100000        // RangeLength
        )
      })
    }
  } // End of PCI1

  // PCIe host bridge 0 config space accessed over DBI interfaces.
  Device (DBI1) {
    Name (_HID, "AMZN0001")
    Name (_CID, "PNP0C02")
    Name (_UID, 1)  // should be equal to the corresponding PCI segment group number
    Name (_CRS, ResourceTemplate () {
      QWordMemory (
        ResourceProducer,
        PosDecode,
        MinFixed,
        MaxFixed,
        NonCacheable,
        ReadWrite,
        0x0,            // AddressGranularity, should be 2^n - 1
        0xCA400000,     // AddressMinimum
        0xCA400FFF,     // AddressMaximum
        0x00000000,     // AddressTranslation
        0x1000          // RangeLength
      )
    })
  } // End of DBIB
}
