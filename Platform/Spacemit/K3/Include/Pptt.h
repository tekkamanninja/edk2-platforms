/** @file
  Processor Properties Topology Table (PPTT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __K3_ACPI_PPTT_H__
#define __K3_ACPI_PPTT_H__

#pragma pack(1)

typedef struct {
  EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR   Core;
  UINT32                                  PrivateResources[2];
  EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE       L1ICache;
  EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE       L1DCache;
} PPTT_CORE;

typedef struct {
  EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR   Cluster;
  UINT32                                  PrivateResources;
  EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE       L2Cache;
  PPTT_CORE                               Cores[4];
} PPTT_CLUSTER;

typedef struct {
  EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR   Package;
  PPTT_CLUSTER                            Clusters[2];
} PPTT_PACKAGE;

#ifndef NUM_PPTT_PACKAGES
#error "Macro NUM_PPTT_PACKAGES undefined"
#endif

typedef struct {
  EFI_ACPI_6_6_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER   Header;
  PPTT_PACKAGE                                              Packages[NUM_PPTT_PACKAGES];
} EFI_ACPI_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE;

//
// Helper macro to construct the unique Core ID, which should be the same as
// the _UID of Processor Device in ACPI namespace for each Core.
//
#define CORE_ID(PackageIdx, ClusterIdx, CoreIdx)  \
  (64 * (PackageIdx) + 4 * (ClusterIdx) + (CoreIdx))

//
// Helper macro to construct the unique Cluster ID, which should be the same as
// the _UID of Processor Container Device in ACPI namespace for each Cluster.
//
// - Bits[31:24]: Hierarchy level. Starting from 1, top-down incremental.
// - Bits[23:0] : Device index in this hierarchy level.
//
// In this PPTT, the top-down processor hierarchy is Package-Cluster-Core, so
// Clusters are always in the hierarchy level 2.
//
#define CLUSTER_ID(PackageIdx, ClusterIdx)  \
  ((2 << 24) | ((16 * (PackageIdx) + (ClusterIdx)) & 0xffffff))

//
// Helper macro to calculate a unique 32-bit cache ID.
//
// - Bits[31:28]: Cache level. 1 for L1, 2 for L2, etc.
// - Bits[27:0] : Cache index in this level.
//
#define PPTT_CACHE_ID(CacheLevel, Index)  \
  ((((CacheLevel) & 0xf) << 28) | ((Index) & 0xfffffff))

#define PPTT_PROCESSOR_FLAGS_CORE                   \
  {                                                 \
    EFI_ACPI_6_6_PPTT_PACKAGE_NOT_PHYSICAL,         \
    EFI_ACPI_6_6_PPTT_PROCESSOR_ID_VALID,           \
    EFI_ACPI_6_6_PPTT_PROCESSOR_IS_NOT_THREAD,      \
    EFI_ACPI_6_6_PPTT_NODE_IS_LEAF,                 \
    EFI_ACPI_6_6_PPTT_IMPLEMENTATION_IDENTICAL,     \
  }

#define PPTT_PROCESSOR_FLAGS_CLUSTER                \
  {                                                 \
    EFI_ACPI_6_6_PPTT_PACKAGE_NOT_PHYSICAL,         \
    EFI_ACPI_6_6_PPTT_PROCESSOR_ID_VALID,           \
    EFI_ACPI_6_6_PPTT_PROCESSOR_IS_NOT_THREAD,      \
    EFI_ACPI_6_6_PPTT_NODE_IS_NOT_LEAF,             \
    EFI_ACPI_6_6_PPTT_IMPLEMENTATION_IDENTICAL,     \
  }

#define PPTT_PROCESSOR_FLAGS_PACKAGE                \
  {                                                 \
    EFI_ACPI_6_6_PPTT_PACKAGE_PHYSICAL,             \
    EFI_ACPI_6_6_PPTT_PROCESSOR_ID_INVALID,         \
    EFI_ACPI_6_6_PPTT_PROCESSOR_IS_NOT_THREAD,      \
    EFI_ACPI_6_6_PPTT_NODE_IS_NOT_LEAF,             \
    EFI_ACPI_6_6_PPTT_IMPLEMENTATION_IDENTICAL,     \
  }

#define PPTT_CACHE_FLAGS                        \
  {                                             \
    EFI_ACPI_6_6_PPTT_CACHE_SIZE_VALID,         \
    EFI_ACPI_6_6_PPTT_NUMBER_OF_SETS_VALID,     \
    EFI_ACPI_6_6_PPTT_ASSOCIATIVITY_VALID,      \
    EFI_ACPI_6_6_PPTT_ALLOCATION_TYPE_VALID,    \
    EFI_ACPI_6_6_PPTT_CACHE_TYPE_VALID,         \
    EFI_ACPI_6_6_PPTT_WRITE_POLICY_VALID,       \
    EFI_ACPI_6_6_PPTT_LINE_SIZE_VALID,          \
    EFI_ACPI_6_6_PPTT_CACHE_ID_VALID,           \
  }

#define PPTT_CACHE_ATTRS_INST                               \
  {                                                         \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_ALLOCATION_READ,          \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION,   \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,  \
  }

#define PPTT_CACHE_ATTRS_DATA                               \
  {                                                         \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,    \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_CACHE_TYPE_DATA,          \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,  \
  }

#define PPTT_CACHE_ATTRS_UNIFIED                            \
  {                                                         \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_ALLOCATION_READ_WRITE,    \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED,       \
    EFI_ACPI_6_6_CACHE_ATTRIBUTES_WRITE_POLICY_WRITE_BACK,  \
  }

#define EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR_INIT(Length, Flags, Parent, Id, NumRes)   \
  {                                                                                     \
    EFI_ACPI_6_6_PPTT_TYPE_PROCESSOR,     /* Type */                                    \
    Length,                               /* Length */                                  \
    {                                     /* Reserved[2] */                             \
      EFI_ACPI_RESERVED_BYTE,                                                           \
      EFI_ACPI_RESERVED_BYTE,                                                           \
    },                                                                                  \
    Flags,                                /* Flags */                                   \
    Parent,                               /* Parent */                                  \
    Id,                                   /* AcpiProcessorId */                         \
    NumRes                                /* NumberOfPrivateResources */                \
  }

#define EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE_INIT(Flags, NextLevelCache, Size,   \
                                               NumSets, NumWays, Attrs,       \
                                               LineSize, Id)                  \
  {                                                                           \
    EFI_ACPI_6_6_PPTT_TYPE_CACHE,                 /* Type */                  \
    sizeof (EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE),   /* Length */                \
    {                                             /* Reserved[2] */           \
      EFI_ACPI_RESERVED_BYTE,                                                 \
      EFI_ACPI_RESERVED_BYTE                                                  \
    },                                                                        \
    Flags,                                        /* Flags */                 \
    NextLevelCache,                               /* NextLevelOfCache */      \
    Size,                                         /* Size */                  \
    NumSets,                                      /* NumberOfSets */          \
    NumWays,                                      /* Associativity */         \
    Attrs,                                        /* Attributes */            \
    LineSize,                                     /* LineSize */              \
    Id                                            /* CacheId */               \
  }

#define PPTT_CORE_INIT_RAW(PackageIdx, ClusterIdx, CoreIdx, L1ICacheStruct, L1DCacheStruct)   \
  {                                                                                           \
    /* EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR  Cluster */                                      \
    EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR_INIT (                                              \
      OFFSET_OF (PPTT_CORE, L1ICache),                    /* Length */                        \
      PPTT_PROCESSOR_FLAGS_CORE,                          /* Flags */                         \
      OFFSET_OF (                                         /* Parent */                        \
        EFI_ACPI_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE,                                         \
        Packages[PackageIdx].Clusters[ClusterIdx].Cluster                                     \
      ),                                                                                      \
      CORE_ID (PackageIdx, ClusterIdx, CoreIdx),          /* Id */                            \
      2                                                   /* NumRes */                        \
    ),                                                                                        \
                                                                                              \
    /* UINT32 PrivateResources[2] */                                                          \
    {                                                                                         \
      OFFSET_OF (                                                                             \
        EFI_ACPI_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE,                                         \
        Packages[PackageIdx].Clusters[ClusterIdx].Cores[CoreIdx].L1ICache                     \
      ),                                                                                      \
      OFFSET_OF (                                                                             \
        EFI_ACPI_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE,                                         \
        Packages[PackageIdx].Clusters[ClusterIdx].Cores[CoreIdx].L1DCache                     \
      )                                                                                       \
    },                                                                                        \
                                                                                              \
    /* EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE  L1ICache; */                                        \
    L1ICacheStruct,                                                                           \
                                                                                              \
    /* EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE  L1DCache; */                                        \
    L1DCacheStruct                                                                            \
  }

#define PPTT_CORE_INIT(CpuType, PackageIdx, ClusterIdx, CoreIdx)                  \
  PPTT_CORE_INIT_RAW (                                                            \
    PackageIdx,                                                                   \
    ClusterIdx,                                                                   \
    CoreIdx,                                                                      \
    PPTT_L1I_CACHE_INIT_ ## CpuType (CORE_ID (PackageIdx, ClusterIdx, CoreIdx)),  \
    PPTT_L1D_CACHE_INIT_ ## CpuType (CORE_ID (PackageIdx, ClusterIdx, CoreIdx))   \
  )

#define PPTT_CORES_PER_CLUSTER_INIT(CpuType, PackageIdx, ClusterIdx)    \
  {                                                                     \
    PPTT_CORE_INIT (CpuType, PackageIdx, ClusterIdx, 0),                \
    PPTT_CORE_INIT (CpuType, PackageIdx, ClusterIdx, 1),                \
    PPTT_CORE_INIT (CpuType, PackageIdx, ClusterIdx, 2),                \
    PPTT_CORE_INIT (CpuType, PackageIdx, ClusterIdx, 3)                 \
  }

#define PPTT_CLUSTER_INIT_RAW(PackageIdx, ClusterIdx, L2CacheStruct, CoreStructs) \
  {	                                                                              \
    /* EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR  Cluster */                          \
    EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR_INIT (                                  \
      OFFSET_OF (PPTT_CLUSTER, L2Cache),      /* Length */                        \
      PPTT_PROCESSOR_FLAGS_CLUSTER,           /* Flags */                         \
      OFFSET_OF (                             /* Parent */                        \
        EFI_ACPI_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE,                             \
        Packages[PackageIdx]                                                      \
      ),                                                                          \
      CLUSTER_ID (PackageIdx, ClusterIdx),    /* Id */                            \
      1                                       /* NumRes */                        \
    ),                                                                            \
                                                                                  \
    /* UINT32 PrivateResources */                                                 \
    OFFSET_OF (                                                                   \
      EFI_ACPI_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE,                               \
      Packages[PackageIdx].Clusters[ClusterIdx].L2Cache                           \
    ),                                                                            \
                                                                                  \
    /* EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE  L2Cache */                              \
    L2CacheStruct,                                                                \
                                                                                  \
    /* PPTT_CORE Cores[4] */                                                      \
    CoreStructs                                                                   \
  }

#define PPTT_CLUSTER_INIT(CpuType, PackageIdx, ClusterIdx)          \
  PPTT_CLUSTER_INIT_RAW (                                           \
    PackageIdx,                                                     \
    ClusterIdx,                                                     \
    PPTT_L2_CACHE_INIT_ ## CpuType (PackageIdx, ClusterIdx),        \
    PPTT_CORES_PER_CLUSTER_INIT (CpuType, PackageIdx, ClusterIdx)   \
  )

#define PPTT_PACKAGE_PROC_STRUCT_INIT(PackageIdx)                     \
  EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR_INIT (                        \
    sizeof (EFI_ACPI_6_6_PPTT_STRUCTURE_PROCESSOR),   /* Length */    \
    PPTT_PROCESSOR_FLAGS_PACKAGE,                     /* Flags */     \
    0,                                                /* Parent */    \
    PackageIdx,                                       /* Id */        \
    0                                                 /* NumRes */    \
  )                                                                   \

#define PPTT_L1I_CACHE_INIT_X100(CoreId)                    \
  EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE_INIT (                  \
    PPTT_CACHE_FLAGS,                 /* Flags */           \
    0,                                /* NextLevelCache */  \
    SIZE_64KB,                        /* Size */            \
    256,                              /* NumSets */         \
    4,                                /* NumWays */         \
    PPTT_CACHE_ATTRS_INST,            /* Attrs */           \
    64,                               /* LineSize */        \
    PPTT_CACHE_ID (1, (CoreId) * 2)   /* CacheId */         \
  )

#define PPTT_L1D_CACHE_INIT_X100(CoreId)                        \
  EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE_INIT (                      \
    PPTT_CACHE_FLAGS,                     /* Flags */           \
    0,                                    /* NextLevelCache */  \
    SIZE_64KB,                            /* Size */            \
    256,                                  /* NumSets */         \
    4,                                    /* NumWays */         \
    PPTT_CACHE_ATTRS_DATA,                /* Attrs */           \
    64,                                   /* LineSize */        \
    PPTT_CACHE_ID (1, (CoreId) * 2 + 1)   /* CacheId */         \
  )                                                             \

#define PPTT_L2_CACHE_INIT_X100(PackageIdx, ClusterIdx)                         \
  EFI_ACPI_6_6_PPTT_STRUCTURE_CACHE_INIT (                                      \
    PPTT_CACHE_FLAGS,                                     /* Flags */           \
    0,                                                    /* NextLevelCache */  \
    SIZE_4MB,                                             /* Size */            \
    4096,                                                 /* NumSets */         \
    16,                                                   /* NumWays */         \
    PPTT_CACHE_ATTRS_UNIFIED,                             /* Attrs */           \
    64,                                                   /* LineSize */        \
    PPTT_CACHE_ID (2, 16 * (PackageIdx) + (ClusterIdx))   /* CacheId*/          \
  )                                                                             \

#pragma pack()

#endif /* ifndef __K3_ACPI_PPTT_H__ */
