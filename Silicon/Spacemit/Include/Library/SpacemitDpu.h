/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_DPU_H_
#define _SPACEMIT_DPU_H_

#define OUTFMT_RGB121212  0
#define OUTFMT_RGB101010  1
#define OUTFMT_RGB888     2
#define OUTFMT_RGB666     12
#define OUTFMT_RGB565     13

typedef enum {
  DpuModeEdp = 0,
  DpuModeMipi,
  DpuModeHdmi,
  DpuModeLvds,
  DpuModeDp,
  DpuModeMax
} DPU_MODES;

typedef enum {
  DPU0 = 0,
  DPU1,
  DPU_NUM_MAX
} DPU_ID;

typedef enum {
  DpuFeatureOutput10Bit = (1 << 0),
} DPU_FEATURES;

typedef enum {
  PowerInvalid = 0,
  PowerOff,
  PowerOn,
} POWER_STATE;

#endif
