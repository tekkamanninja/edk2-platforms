/** @file
  A temporary header file to define contents that added in ACPI 6.6

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __ACPI_6_6_TEMP_H
#define __ACPI_6_6_TEMP_H

#include <IndustryStandard/Acpi.h>

// TODO: Remove this file after ACPI 6.6 released

// Ensure proper structure formats
#pragma pack(1)

//
// =============================================================================
// Definitions for RHCT
//
#define EFI_ACPI_6_6_RISCV_HART_CAPABILITIES_TABLE_SIGNATURE  SIGNATURE_32('R', 'H', 'C', 'T')
#define EFI_ACPI_6_6_RISCV_HART_CAPABILITIES_TABLE_REVISION   0x01

// RHCT header
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
  UINT32                         Flags;
  UINT64                         TimeBaseFreq;
  UINT32                         NumNodes;
  UINT32                         OffsetToNodeArray;
} EFI_ACPI_6_6_RISCV_HART_CAPABILITIES_TABLE_HEADER;

#define EFI_ACPI_6_6_RHCT_OFFSET_TO_NODE_ARRAY  (sizeof (EFI_ACPI_6_6_RISCV_HART_CAPABILITIES_TABLE_HEADER))

// RHCT ISA string node structure
#define EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE(NAME) \
  EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE_ ## NAME

typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT16    Revision;
  UINT16    IsaStringLength;
} EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE_HEADER;

#define EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE_TYPEDEF(NAME, IsaStringLength)        \
  typedef struct {                                                                        \
    EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE_HEADER  Header;                           \
    char                                                IsaString[IsaStringLength];       \
    UINT8                                               Padding[(IsaStringLength) % 2];   \
  } EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE(NAME)

#define EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE_INIT(NAME, IsaString)     \
  {                                                                           \
    {                                                                         \
      0x0,                                                                    \
      sizeof (EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE(NAME)),             \
      1,                                                                      \
      OFFSET_OF (EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE(NAME), Padding)  \
        - sizeof (EFI_ACPI_6_6_RHCT_ISA_STRING_NODE_STRUCTURE_HEADER),        \
    },                                                                        \
    IsaString,                                                                \
  }

// RHCT CMO node structure
typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT16    Revision;
  UINT8     Reserved;
  UINT8     CbomBlockSize;
  UINT8     CbopBlockSize;
  UINT8     CbozBlockSize;
} EFI_ACPI_6_6_RHCT_CMO_NODE_STRUCTURE;

#define EFI_ACPI_6_6_RHCT_CMO_NODE_STRUCTURE_INIT(                  \
    CbomBlockSize, CbopBlockSize, CbozBlockSize)                    \
  {                                                                 \
    0x1, sizeof (EFI_ACPI_6_6_RHCT_CMO_NODE_STRUCTURE), 1, 0,       \
    CbomBlockSize, CbopBlockSize, CbozBlockSize                     \
  }

// RHCT MMU node structure
typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT16    Revision;
  UINT8     Reserved;
  UINT8     MmuType;
} EFI_ACPI_6_6_RHCT_MMU_NODE_STRUCTURE;

#define EFI_ACPI_6_6_RHCT_MMU_NODE_STRUCTURE_INIT(MmuType)              \
  {                                                                     \
    0x2, sizeof (EFI_ACPI_6_6_RHCT_MMU_NODE_STRUCTURE), 1, 0, MmuType   \
  }

// RHCT hart info node structure
#define EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE(NAME) \
  EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE_ ## NAME

typedef struct {
  UINT16    Type;
  UINT16    Length;
  UINT16    Revision;
  UINT16    NumOffsets;
  UINT32    AcpiProcessorUid;
} EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE_HEADER;

#define EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE_TYPEDEF(NAME, NumOffsets)  \
  typedef struct {                                                            \
    EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE_HEADER   Header;               \
    UINT32                                              Offsets[NumOffsets];  \
  } EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE(NAME)

#define EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE_HEADER_INIT(NAME, AcpiProcessorUid)  \
  {                                                                                     \
    0xFFFF,                                                                             \
    sizeof (EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE(NAME)),                          \
    1,                                                                                  \
    (sizeof (EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE(NAME))                          \
      - sizeof (EFI_ACPI_6_6_RHCT_HART_INFO_NODE_STRUCTURE_HEADER))                     \
      / sizeof (UINT32),                                                                \
    AcpiProcessorUid                                                                    \
  }

//
// =============================================================================
// Definitions for MADT
//

#define EFI_ACPI_6_6_RINTC_STRUCTURE_INIT(                                  \
    Flags, HartId, AcpiProcessorUid, ExtIntcId, ImsicAddress, ImsicSize)    \
  {                                                                         \
    0x18, sizeof (EFI_ACPI_6_6_RINTC_STRUCTURE), 1, EFI_ACPI_RESERVED_BYTE, \
    Flags, HartId, AcpiProcessorUid, ExtIntcId, ImsicAddress, ImsicSize     \
  }

#define EFI_ACPI_6_6_IMSIC_STRUCTURE_INIT(NumSupervisorIntIds, NumGuestIntIds, \
    GuestIndexBits, HartIndexBits, GroupIndexBits, GroupIndexShift)            \
  {                                                                            \
    0x19, sizeof (EFI_ACPI_6_6_IMSIC_STRUCTURE), 1, EFI_ACPI_RESERVED_BYTE,    \
    0x00000000, NumSupervisorIntIds, NumGuestIntIds, GuestIndexBits,           \
    HartIndexBits, GroupIndexBits, GroupIndexShift                             \
  }

#define EFI_ACPI_6_6_APLIC_STRUCTURE_INIT(AplicId, HardwareId, NumIdcs,     \
    NumExtIntSources, GsiBase, AplicAddress, AplicSize)                     \
  {                                                                         \
    0x1A, sizeof (EFI_ACPI_6_6_APLIC_STRUCTURE), 1, AplicId, 0x00000000,    \
    HardwareId, NumIdcs, NumExtIntSources, GsiBase, AplicAddress, AplicSize \
  }

#define EFI_ACPI_6_6_PLIC_STRUCTURE_INIT(PlicId, HardwareId, NumExtIntSources, \
    MaxPriority, PlicSize, PlicAddress, GsivBase)                              \
  {                                                                            \
    0x1B, sizeof (EFI_ACPI_6_6_PLIC_STRUCTURE), 1, PlicId, HardwareId,         \
    NumExtIntSources, MaxPriority, 0x00000000, PlicSize, PlicAddress, GsivBase \
  }

//
// =============================================================================
// Definitions for RIMT
//
#define EFI_ACPI_6_6_RISCV_IO_MAPPING_TABLE_SIGNATURE  SIGNATURE_32('R', 'I', 'M', 'T')
#define EFI_ACPI_6_6_RISCV_IO_MAPPING_TABLE_REVISION   0x01

// RIMT header
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER    Header;
  UINT32                         NumNodes;
  UINT32                         OffsetToNodeArray;
  UINT32                         Reserved;
} EFI_ACPI_6_6_RISCV_IO_MAPPING_TABLE_HEADER;

#define EFI_ACPI_6_6_RIMT_OFFSET_TO_NODE_ARRAY  (sizeof (EFI_ACPI_6_6_RISCV_IO_MAPPING_TABLE_HEADER))

// RIMT IOMMU Node structure
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE(NAME) \
  EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_ ## NAME

typedef struct {
  UINT8     Type;
  UINT8     Revision;
  UINT16    Length;
  UINT16    Reserved;
  UINT16    Id;
  UINT64    HardwareId;
  UINT64    BaseAddress;
  UINT32    Flags;
  UINT32    ProximityDomain;
  UINT16    PcieSegmentNumber;
  UINT16    PcieBdf;
  UINT16    NumInterruptWires;
  UINT16    InterruptWireArrayOffset;
} EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_HEADER;

typedef struct {
  UINT32    InterruptNumber;
  UINT32    Flags;
} EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_STRUCTURE;

#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_TYPEDEF(NAME, NumInterruptWires)                 \
  typedef struct {                                                                              \
    EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_HEADER           Header;                             \
    EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_STRUCTURE   InterruptWires[NumInterruptWires];  \
  } EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE(NAME)

#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_HEADER_INIT(NAME,                      \
    Id, HardwareId, BaseAddress, Flags, ProximityDomain, PcieSegmentNumber, PcieBdf)  \
  {                                                                                   \
    0,                                                                                \
    1,                                                                                \
    sizeof (EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE(NAME)),                            \
    0,                                                                                \
    Id,                                                                               \
    HardwareId,                                                                       \
    BaseAddress,                                                                      \
    Flags,                                                                            \
    ProximityDomain,                                                                  \
    PcieSegmentNumber,                                                                \
    PcieBdf,                                                                          \
    (sizeof (EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE(NAME))                            \
      - sizeof (EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_HEADER))                       \
      / sizeof (EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_STRUCTURE),               \
    sizeof (EFI_ACPI_6_6_RIMT_IOMMU_NODE_STRUCTURE_HEADER)                            \
  }

// RIMT IOMMU Node Flags
//  Bit[0]: IOMMU is a PCIe device
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_FLAG_IS_PCIE_DEVICE      (1 << 0)
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_FLAG_IS_PLATFORM_DEVICE  (0 << 0)
//  Bit[1]: Proximity Domain valid
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_FLAG_PROXIMITY_DOMAIN_VALID    (1 << 1)
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_FLAG_PROXIMITY_DOMAIN_INVALID  (0 << 1)

// RIMT IOMMU Node Interrupt Wire Structure Flags
//  Bit[0]: Interrupt Mode
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_FLAG_EDGE_TRIGGERED   (0 << 0)
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_FLAG_LEVEL_TRIGGERED  (1 << 0)
//  Bit[1]: Interrupt Polarity
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_FLAG_ACTIVE_LOW   (0 << 1)
#define EFI_ACPI_6_6_RIMT_IOMMU_NODE_INTERRUPT_WIRE_FLAG_ACTIVE_HIGH  (1 << 1)

// RIMT PCIe Root Complex Node structure
#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE(NAME) \
  EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_ ## NAME

typedef struct {
  UINT8     Type;
  UINT8     Revision;
  UINT16    Length;
  UINT16    Reserved1;
  UINT16    Id;
  UINT32    Flags;
  UINT16    Reserved2;
  UINT16    PcieSegmentNumber;
  UINT16    IdMappingArrayOffset;
  UINT16    NumIdMappings;
} EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_HEADER;

#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_TYPEDEF(NAME, NumIdMappings)       \
  typedef struct {                                                                  \
    EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_HEADER   Header;                       \
    EFI_ACPI_6_6_RIMT_ID_MAPPING_STRUCTURE            IdMappings[NumIdMappings];    \
  } EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE(NAME)

#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_HEADER_INIT(NAME,                \
                                                             Id,                  \
                                                             Flags,               \
                                                             PcieSegmentNumber)   \
  {                                                                               \
    1,                                                                            \
    1,                                                                            \
    sizeof (EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE(NAME)),                      \
    0,                                                                            \
    Id,                                                                           \
    Flags,                                                                        \
    0,                                                                            \
    PcieSegmentNumber,                                                            \
    sizeof (EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_HEADER),                     \
    (sizeof (EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE(NAME))                      \
      - sizeof (EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_STRUCTURE_HEADER))                 \
      / sizeof (EFI_ACPI_6_6_RIMT_ID_MAPPING_STRUCTURE)                           \
  }

// RIMT PCIe Root Complex Node Flags
//  Bit[0]: ATS support
#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_FLAG_ATS_SUPPORT      (0 << 0)
#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_FLAG_ATS_NOT_SUPPORT  (1 << 0)
//  Bit[1]: PRI support
#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_FLAG_PRI_SUPPORT      (0 << 1)
#define EFI_ACPI_6_6_RIMT_PCIE_RC_NODE_FLAG_PRI_NOT_SUPPORT  (1 << 1)

// RIMT Platform Device Node structure
#define EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE(NAME) \
  EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE_ ## NAME

typedef struct {
  UINT8     Type;
  UINT8     Revision;
  UINT16    Length;
  UINT16    Reserved;
  UINT16    Id;
  UINT16    IdMappingArrayOffset;
  UINT16    NumIdMappings;
} EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE_HEADER;

#define EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE_TYPEDEF(NAME,                                  \
                                                                 DeviceObjectNameLength,                \
                                                                 NumIdMappings)                         \
  typedef struct {                                                                                      \
    EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE_HEADER   Header;                                   \
    char                                                      DeviceObjectName[DeviceObjectNameLength]; \
    UINT8                                                     Padding[(DeviceObjectNameLength) % 4];    \
    EFI_ACPI_6_6_RIMT_ID_MAPPING_STRUCTURE                    IdMappings[NumIdMappings];                \
  } EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE(NAME)

#define EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE_HEADER_OBJNAME_INIT(NAME,                \
                                                                             Id,                  \
                                                                             DeviceObjectName)    \
  {                                                                                               \
    {                                                                                             \
      2,                                                                                          \
      1,                                                                                          \
      sizeof (EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE(NAME)),                            \
      0,                                                                                          \
      Id,                                                                                         \
      OFFSET_OF (EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE(NAME), IdMappings),             \
      (sizeof (EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE(NAME))                            \
        - OFFSET_OF (EFI_ACPI_6_6_RIMT_PLATFORM_DEVICE_NODE_STRUCTURE(NAME), IdMappings))         \
        / sizeof (EFI_ACPI_6_6_RIMT_ID_MAPPING_STRUCTURE),                                        \
    },                                                                                            \
    DeviceObjectName                                                                              \
  }

// RIMT ID Mapping Structure
typedef struct {
  UINT32    SrcIdBase;
  UINT32    NumIds;
  UINT32    DestDeviceIdBase;
  UINT32    DestIommuOffset;
  UINT32    Flags;
} EFI_ACPI_6_6_RIMT_ID_MAPPING_STRUCTURE;

// RIMT ID Mapping Structure Flags
//  Bit[0]: ATS Required
#define EFI_ACPI_6_6_RIMT_ID_MAPPING_FLAG_ATS_NOT_REQUIRED  (0 << 0)
#define EFI_ACPI_6_6_RIMT_ID_MAPPING_FLAG_ATS_REQUIRED      (1 << 0)
//  Bit[1]: PRI Required
#define EFI_ACPI_6_6_RIMT_ID_MAPPING_FLAG_PRI_NOT_REQUIRED  (0 << 1)
#define EFI_ACPI_6_6_RIMT_ID_MAPPING_FLAG_PRI_REQUIRED      (1 << 1)

//
// SRAT Structure Type Definitions (ACPI 6.6)
//
#define EFI_ACPI_6_6_RINTC_AFFINITY  0x07  // RISC-V INTC Affinity

//
// RINTC Affinity Structure Flags
//
#define EFI_ACPI_6_6_RINTC_ENABLED  BIT0

#pragma pack()

#endif /* ifndef __ACPI_6_6_TEMP_H */
