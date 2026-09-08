/** @file
  RISC-V IOMMU interfaces

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __RISCV_IOMMU_DXE_H__
#define __RISCV_IOMMU_DXE_H__

#include <Base.h>
#include <BitOps.h>

typedef struct {
  UINT16          Id;

  UINT64          RegBase;    // Base address of the IOMMU registers.
  UINT64          RegSize;    // Size of the IOMMU registers.
} RISCV_IOMMU_DEVICE;

//
// Memory-mapped register interface
//

// Device-directory-table pointer (ddtp) (64bits)
#define RISCV_IOMMU_REG_DDTP                      0x0010
#define RISCV_IOMMU_REG_DDTP_IOMMU_MODE_MASK	    0xf
#define RISCV_IOMMU_REG_DDTP_BUSY                 BIT_ULL(4)

//
// ddtp.iommu_mode:
//  0: Off: No inbound memory transactions are allowed.
//  1: Bare: All inbound memory accesses are passed through.
//  2: 1LVL: One-level device-directory-table
//  3: 2LVL: Two-level device-directory-table
//  4: 3LVL: Three-level device-directory-table
//
typedef enum  {
  RISCV_IOMMU_DDTP_IOMMU_MODE_OFF = 0,
  RISCV_IOMMU_DDTP_IOMMU_MODE_BARE = 1,
  RISCV_IOMMU_DDTP_IOMMU_MODE_1LVL = 2,
  RISCV_IOMMU_DDTP_IOMMU_MODE_2LVL = 3,
  RISCV_IOMMU_DDTP_IOMMU_MODE_3LVL = 4,
  RISCV_IOMMU_DDTP_IOMMU_MODE_MAX = RISCV_IOMMU_DDTP_IOMMU_MODE_3LVL
} RISCV_IOMMU_DDTP_IOMMU_MODE;

// ddtp.busy
#define RISCV_IOMMU_DDTP_IS_BUSY(Ddtp)  ((Ddtp) & RISCV_IOMMU_REG_DDTP_BUSY)

#endif /* ifndef __RISCV_IOMMU_DXE_H__ */
