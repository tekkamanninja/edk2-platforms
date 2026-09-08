/** @file
  Configurations for DmaIoMmuLib.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DMA_IOMMU_CONFIG_H__
#define __DMA_IOMMU_CONFIG_H__

#pragma pack(1)

typedef struct {
  UINT64    CpuAddr;
  UINT64    DmaAddr;
  UINT64    Size;
} DMA_IOMMU_MAPPING;

typedef struct {
  UINT16                        Num;
  DMA_IOMMU_MAPPING             Data[0];
} DMA_IOMMU_MAPPINGS;

#pragma pack()

#endif /* ifndef __DMA_IOMMU_CONFIG_H__ */
