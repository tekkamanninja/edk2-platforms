/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_DPUDXE_H_
#define _SPACEMIT_DPUDXE_H_

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include "Include/Library/SpacemitDpu.h"

#define K3_CIU_BASE  (FixedPcdGet64(PcdSpacemitCIURegBase))

#define DPU0_REG_BASE  (FixedPcdGet64(PcdSpacemitDpu0RegBase))
#define DPU1_REG_BASE  (FixedPcdGet64(PcdSpacemitDpu1RegBase))

#endif
