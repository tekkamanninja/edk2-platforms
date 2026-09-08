/** @file
 *  Spacemit K3 silicon UFS clock driver implementation.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <ClockDxe.h>

#define UFS_ACLK_SEL_SHIFT 2
#define UFS_ACLK_SEL_WIDTH 3
#define UFS_ACLK_DIV_SHIFT 5
#define UFS_ACLK_DIV_WIDTH 3
#define UFS_ACLK_FC_BIT BIT8

#define UFS_ACLK_SEL_PLL1_D5_491P52 0
#define UFS_ACLK_SEL_PLL1_D6_409P6 1

#define UFS_ACLK_SEL_MASK                                                      \
  (((1U << UFS_ACLK_SEL_WIDTH) - 1) << UFS_ACLK_SEL_SHIFT)
#define UFS_ACLK_DIV_MASK                                                      \
  (((1U << UFS_ACLK_DIV_WIDTH) - 1) << UFS_ACLK_DIV_SHIFT)
#define UFS_ACLK_MAX_DIV (1U << UFS_ACLK_DIV_WIDTH)
#define UFS_ACLK_FC_TIMEOUT_US (10 * 1000)
#define UFS_ACLK_MAX_DIFF (~0ULL)

STATIC CONST UINT64 UfsAclkParentRateHz[] = {
    491520000ULL,
    409600000ULL,
};

STATIC
EFI_STATUS
SelectUfsAclkSourceAndDiv(IN UINT64 RequestRate, OUT UINT32 *ClockSel,
                          OUT UINT32 *ClockDiv, OUT UINT64 *ActualRate) {
  UINT32 Sel;
  UINT32 Div;
  UINT64 CandidateRate;
  UINT64 MinDiff;
  UINT32 BestSel;
  UINT32 BestDiv;
  UINT64 BestRate;
  UINT64 MinRate;
  UINT32 MinRateSel;
  UINT32 MinRateDiv;

  if ((RequestRate == 0) || (ClockSel == NULL) || (ClockDiv == NULL) ||
      (ActualRate == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  MinDiff = UFS_ACLK_MAX_DIFF;
  BestSel = UFS_ACLK_SEL_PLL1_D6_409P6;
  BestDiv = 1;
  BestRate = UfsAclkParentRateHz[BestSel];
  MinRate = UFS_ACLK_MAX_DIFF;
  MinRateSel = BestSel;
  MinRateDiv = UFS_ACLK_MAX_DIV;

  for (Sel = 0; Sel < ARRAY_SIZE(UfsAclkParentRateHz); Sel++) {
    if (UfsAclkParentRateHz[Sel] == 0) {
      continue;
    }

    for (Div = 1; Div <= UFS_ACLK_MAX_DIV; Div++) {
      CandidateRate = UfsAclkParentRateHz[Sel] / Div;
      if (CandidateRate < MinRate) {
        MinRate = CandidateRate;
        MinRateSel = Sel;
        MinRateDiv = Div;
      }

      if (CandidateRate > RequestRate) {
        continue;
      }

      if ((RequestRate - CandidateRate) < MinDiff) {
        MinDiff = RequestRate - CandidateRate;
        BestSel = Sel;
        BestDiv = Div;
        BestRate = CandidateRate;

        if (MinDiff == 0) {
          goto Found;
        }
      }
    }
  }

  if ((MinDiff == UFS_ACLK_MAX_DIFF) && (MinRate != UFS_ACLK_MAX_DIFF)) {
    BestSel = MinRateSel;
    BestDiv = MinRateDiv;
    BestRate = MinRate;
  }

Found:
  *ClockSel = BestSel;
  *ClockDiv = BestDiv;
  *ActualRate = BestRate;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetUfsClockRate(IN UINT64 ClockRate) {
  EFI_STATUS Status;
  UINT32 ClockSel;
  UINT32 ClockDiv;
  UINT64 ActualRate;

  Status =
      SelectUfsAclkSourceAndDiv(ClockRate, &ClockSel, &ClockDiv, &ActualRate);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  DEBUG((DEBUG_INFO,
         "UFS ACLK request=%lluHz source=%lluHz div=%u real=%lluHz\n",
         ClockRate, UfsAclkParentRateHz[ClockSel], ClockDiv, ActualRate));

  MmioAndThenOr32(K3_UFS_CLK_RES_CTRL, ~(UFS_ACLK_SEL_MASK | UFS_ACLK_DIV_MASK),
                  (ClockSel << UFS_ACLK_SEL_SHIFT) |
                      ((ClockDiv - 1) << UFS_ACLK_DIV_SHIFT));

  MmioOr32(K3_UFS_CLK_RES_CTRL, UFS_ACLK_FC_BIT);

  Status = PollRegStatus(K3_UFS_CLK_RES_CTRL, UFS_ACLK_FC_BIT, 0,
                         UFS_ACLK_FC_TIMEOUT_US);
  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "Set UFS clock rate timeout!\n"));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetUfsCurrentClockRate(OUT UINT64 *ClockRate) {
  UINT32 RegVal;
  UINT32 ClockSel;
  UINT32 ClockDiv;

  if (ClockRate == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  RegVal = MmioRead32(K3_UFS_CLK_RES_CTRL);
  ClockSel = (RegVal & UFS_ACLK_SEL_MASK) >> UFS_ACLK_SEL_SHIFT;
  ClockDiv = ((RegVal & UFS_ACLK_DIV_MASK) >> UFS_ACLK_DIV_SHIFT) + 1;

  if ((ClockSel >= ARRAY_SIZE(UfsAclkParentRateHz)) ||
      (UfsAclkParentRateHz[ClockSel] == 0) || (ClockDiv == 0)) {
    return EFI_DEVICE_ERROR;
  }

  *ClockRate = UfsAclkParentRateHz[ClockSel] / ClockDiv;

  DEBUG((DEBUG_INFO, "UFS ACLK source=%lluHz div=%u real=%lluHz\n",
         UfsAclkParentRateHz[ClockSel], ClockDiv, *ClockRate));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetUfsMaxClockRate(OUT UINT64 *ClockRate) {
  if (ClockRate == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ClockRate = UfsAclkParentRateHz[UFS_ACLK_SEL_PLL1_D5_491P52];
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetUfsMinClockRate(OUT UINT64 *ClockRate) {
  if (ClockRate == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ClockRate =
      UfsAclkParentRateHz[UFS_ACLK_SEL_PLL1_D6_409P6] / UFS_ACLK_MAX_DIV;
  return EFI_SUCCESS;
}

CLOCK_RATE_OPERATIONS UfsClockRateOps = {GetUfsCurrentClockRate,
                                         GetUfsMaxClockRate, GetUfsMinClockRate,
                                         SetUfsClockRate};
