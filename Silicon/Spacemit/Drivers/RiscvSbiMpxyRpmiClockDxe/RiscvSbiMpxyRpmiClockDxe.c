/** @file
 *  A clock driver based on RISC-V RPMI CLOCK Service Group
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <WordPart.h>
#include "RiscvSbiMpxyRpmiClockDxe.h"

#define RPMI_CLKRATE_U64(Hi, Lo)    ((UINT64)(Hi) << 32 | (UINT32)(Lo))
#define LINEAR_RATE_WORDS_NUM       6
#define DISCRETE_RATE_WORDS_NUM     2

typedef struct {
  UINT64  Discrete;
} CLK_DISCRETE_RATE;

typedef struct {
  UINT64  Min;
  UINT64  Max;
  UINT64  Step;
} CLK_LINEAR_RATE;

typedef enum {
  RATE_ROUND_DOWN = 0,
  RATE_ROUND_UP,
  RATE_AUTO,
  RATE_RESV
} RATE_ROUND_MODE;

/**
  Get number of clocks provided by RPMI CLK_GET_NUM_CLOCKS (0x02).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  NumClocks           Number of clocks provided by RPMI services.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkGetNumClocks (
  IN  MPXY_RPMI_CHANNEL  *Chan,
  OUT UINT32             *NumClocks
)
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_CLK_GET_NUM_CLOCKS_RESP    Resp;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (NumClocks == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_GET_NUM_CLOCKS;
  Msg.TxBuf = NULL;
  Msg.TxLen = 0;
  Msg.RxBuf = (VOID *) &Resp;
  Msg.RxBufLen = sizeof (Resp);
  Msg.RxLen = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n", __func__, Status));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLK_GET_NUM_CLOCKS service failed (error code: %d)\n",
            __func__, Resp.Status));
    return EFI_DEVICE_ERROR;
  }

  *NumClocks = Resp.NumClocks;

  return EFI_SUCCESS;
}

/**
  Get attributes of the specified clock by RPMI CLK_GET_ATTRIBUTES (0x03).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  ClockId             The clock ID.
  @param  RpmiClk             The RPMI_CLOCK_DEVICE instance of the specified ClockId.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkGetAttributes (
  IN  MPXY_RPMI_CHANNEL         *Chan,
  IN  UINT32                    ClockId,
  OUT RPMI_CLOCK_DEVICE         *RpmiClk
)
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_CLK_GET_ATTRIBUTES_REQ     Req;
  RPMI_CLK_GET_ATTRIBUTES_RESP    Resp;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (RpmiClk == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Req.ClockId = ClockId;

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_GET_ATTRIBUTES;
  Msg.TxBuf = (VOID *) &Req;
  Msg.TxLen = sizeof (Req);
  Msg.RxBuf = (VOID *) &Resp;
  Msg.RxBufLen = sizeof (Resp);
  Msg.RxLen = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n",
      __func__, Status
    ));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLK_GET_NUM_CLOCKS service failed (error code: %d)\n",
            __func__, Resp.Status));
    return EFI_DEVICE_ERROR;
  }

  RpmiClk->ClockId = ClockId;
  RpmiClk->Type = Resp.Flags & RPMI_CLK_GET_ARTTRIBUTES_FORMAT_MASK;
  RpmiClk->NumRates = Resp.NumRates;
  RpmiClk->TransitionLatency = Resp.TransitionLatency;
  AsciiStrCpyS (RpmiClk->ClockName, RPMI_CLK_NAME_LEN, (CONST CHAR8 *)Resp.ClockName);

  return EFI_SUCCESS;
}

/**
  Get supported rates of the specified clock by RPMI CLK_GET_SUPPORTED_RATES (0x04).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  RpmiClk             The RPMI_CLOCK_DEVICE instance of the specified ClockId.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkGetSupportedRates (
  IN  MPXY_RPMI_CHANNEL       *Chan,
  IN OUT RPMI_CLOCK_DEVICE         *RpmiClk
)
{
  EFI_STATUS                            Status;
  MPXY_RPMI_MESSAGE                     Msg;
  UINT32                                MsgLenMax;
  UINT32                                Remaining;
  UINT32                                Returned;
  UINT32                                RateIndex;
  UINT32                                WordIndex;
  UINT64                                LastRate;
  UINT64                                NextRate;
  RPMI_CLK_GET_SUPPORTED_RATES_REQ      Req;
  RPMI_CLK_GET_SUPPORTED_RATES_RESP     *PtrResp;
  CLK_DISCRETE_RATE                     *DiscreteRates = NULL;
  CLK_LINEAR_RATE                       *LinearRates = NULL;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (RpmiClk == NULL || RpmiClk->Rates == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  MsgLenMax = MpxyRpmiMessageLengthMax (Chan);
  PtrResp = (RPMI_CLK_GET_SUPPORTED_RATES_RESP *) AllocateZeroPool (MsgLenMax);
  if (PtrResp == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate memory for RPMI CLK_GET_SUPPORTED_RATES service response data\n",
      __func__
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  if (RpmiClk->Type == RPMI_CLOCK_FORMAT_DISCRETE) {
    DiscreteRates = (CLK_DISCRETE_RATE *)RpmiClk->Rates;
  } else if (RpmiClk->Type == RPMI_CLOCK_FORMAT_LINEAR) {
    LinearRates = (CLK_LINEAR_RATE *)RpmiClk->Rates;
  } else {
    Status = EFI_UNSUPPORTED;
    goto FreePtrResp;
  }

  LastRate = 0;
  NextRate = 0;

  Req.ClockRateIndex = 0;
  Req.ClockId = RpmiClk->ClockId;

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_GET_SUPPORTED_RATES;
  Msg.TxBuf = (VOID *) &Req;
  Msg.TxLen = sizeof (Req);
  Msg.RxBuf = (VOID *) PtrResp;
  Msg.RxBufLen = MsgLenMax;
  Msg.RxLen = NULL;

  do {
    RateIndex = 0;
    Status = MpxyRpmiSendMessage (Chan, &Msg);
    if (Status != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n", __func__, Status));
      goto FreePtrResp;
    }

    if (PtrResp->Status != RPMI_SUCCESS) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: RPMI CLK_GET_SUPPORTED_RATES service failed (error code: %d)\n",
        __func__, PtrResp->Status
        ));
      Status = EFI_DEVICE_ERROR;
      goto FreePtrResp;
    }

    Remaining = PtrResp->Remaining;
    Returned = PtrResp->Returned;

    if (Returned + Req.ClockRateIndex > RpmiClk->NumRates) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Clock ID %u: The slots to store clock discrete supported rates is not enough"
        "(expected: %u, current: %u)\n",
        __func__,
        Req.ClockId,
        Returned + Req.ClockRateIndex,
        RpmiClk->NumRates
        ));
      Status = EFI_DEVICE_ERROR;;
      goto FreePtrResp;
    }

    WordIndex = 0;
    if (RpmiClk->Type == RPMI_CLOCK_FORMAT_DISCRETE) {
      for (; RateIndex < Returned; RateIndex++, WordIndex += DISCRETE_RATE_WORDS_NUM) {
        NextRate = RPMI_CLKRATE_U64(
          PtrResp->ClockRate[WordIndex + 1], PtrResp->ClockRate[WordIndex]
        );

        //
        //  Check if clock rate at a lower index is less than that at a higher index
        //
        if (LastRate != 0 && NextRate <= LastRate) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Invalid Supported Rate %llu (must be higher than %llu\n)",
            __func__, NextRate, LastRate));
            Status = EFI_DEVICE_ERROR;
            goto FreePtrResp;
        }

        DiscreteRates[Req.ClockRateIndex++].Discrete = NextRate;
        LastRate = NextRate;

        DEBUG ((DEBUG_INFO, "Rates %d = %llu\n", Req.ClockRateIndex - 1, NextRate));
      }
    } else if (RpmiClk->Type == RPMI_CLOCK_FORMAT_LINEAR) {
      for (; RateIndex < Returned; RateIndex++, WordIndex += LINEAR_RATE_WORDS_NUM) {
        NextRate = RPMI_CLKRATE_U64(
          PtrResp->ClockRate[WordIndex + 1], PtrResp->ClockRate[WordIndex]
        );

        //
        //  Check if clock range at a lower index is less than that at a higher index
        //
        if (LastRate != 0 && NextRate <= LastRate) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Invalid Supported Rate range %llu (must be higher than %llu)\n",
            __func__, NextRate, LastRate));
            Status = EFI_DEVICE_ERROR;
            goto FreePtrResp;
        }
        LinearRates[Req.ClockRateIndex].Min = NextRate;
        LastRate = NextRate;
        DEBUG ((DEBUG_INFO, "Rates %d min = %llu\n", Req.ClockRateIndex, NextRate));

        //
        //  Check if min value of a clock range is less than max value of that
        //
        NextRate = RPMI_CLKRATE_U64(
          PtrResp->ClockRate[WordIndex + 3], PtrResp->ClockRate[WordIndex + 2]
        );
        if (NextRate <= LastRate) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Invalid Supported Rate range %llu (must be higher than %llu)\n",
            __func__, NextRate, LastRate));
            Status = EFI_DEVICE_ERROR;
            goto FreePtrResp;
        }

        LinearRates[Req.ClockRateIndex].Max = NextRate;
        LastRate = NextRate;
        DEBUG ((DEBUG_INFO, "Rates %d max = %llu\n", Req.ClockRateIndex, NextRate));

        LinearRates[Req.ClockRateIndex].Step = RPMI_CLKRATE_U64(
          PtrResp->ClockRate[WordIndex + 5], PtrResp->ClockRate[WordIndex + 4]
        );

        DEBUG ((DEBUG_INFO, "Rates %d step = %llu\n",
          Req.ClockRateIndex, LinearRates[Req.ClockRateIndex].Step
        ));

        Req.ClockRateIndex++;
      }
    }
  } while (Remaining > 0);

  Status = EFI_SUCCESS;

FreePtrResp:
  FreePool (PtrResp);
  return Status;
}

/**
  Set clock config by RPMI CLK_SET_CONFIG (0x05).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  ClockId             The clock ID.
  @param  ClockConfig         CONFIG value to set.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkSetConfig (
  IN  MPXY_RPMI_CHANNEL     *Chan,
  IN  UINT32                ClockId,
  IN  UINT32                ClockConfig
)
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_CLK_SET_CONFIG_REQ         Req;
  RPMI_CLK_SET_CONFIG_RESP        Resp;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  Req.ClockId = ClockId;
  Req.ClockConfig = ClockConfig & RPMI_CLK_CONFIG_MASK;

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_SET_CONFIG;
  Msg.TxBuf = (VOID *) &Req;
  Msg.TxLen = sizeof (Req);
  Msg.RxBuf = (VOID *) &Resp;
  Msg.RxBufLen = sizeof (Resp);
  Msg.RxLen = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n",
      __func__, Status
    ));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a: RPMI CLK_SET_CONFIG service failed (error code: %d)\n",
      __func__, Resp.Status
    ));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Get clock config by RPMI CLK_GET_CONFIG (0x06).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  ClockId             The clock ID.
  @param  ClockConfig         CONFIG value of the clock.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkGetConfig (
  IN  MPXY_RPMI_CHANNEL     *Chan,
  IN  UINT32                ClockId,
  OUT UINT32                *ClockConfig
)
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_CLK_GET_CONFIG_REQ         Req;
  RPMI_CLK_GET_CONFIG_RESP        Resp;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (ClockConfig == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Req.ClockId = ClockId;

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_GET_CONFIG;
  Msg.TxBuf = (VOID *) &Req;
  Msg.TxLen = sizeof (Req);
  Msg.RxBuf = (VOID *) &Resp;
  Msg.RxBufLen = sizeof (Resp);
  Msg.RxLen = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n",
      __func__, Status
    ));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a: RPMI CLK_GET_NUM_CLOCKS service failed (error code: %d)\n",
      __func__, Resp.Status
    ));
    return EFI_DEVICE_ERROR;
  }

  *ClockConfig = Resp.Config & RPMI_CLK_CONFIG_MASK;

  return EFI_SUCCESS;
}

/**
  Set clock rate by RPMI CLK_SET_RATE (0x07).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  ClockId             The clock ID.
  @param  Flags               Clock rate rounding mode
  @param  ClockRate           Clock rate in Hertz
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkSetRate (
  IN  MPXY_RPMI_CHANNEL   *Chan,
  IN  UINT32              ClockId,
  IN  RATE_ROUND_MODE     Flags,
  IN  UINT64              ClockRate
)
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_CLK_SET_RATE_REQ           Req;
  RPMI_CLK_SET_RATE_RESP          Resp;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  Req.ClockId = ClockId;
  Req.Flags = Flags;
  Req.ClockRateLow = LOWER_32_BITS(ClockRate);
  Req.ClockRateHigh = UPPER_32_BITS(ClockRate);

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_SET_RATE;
  Msg.TxBuf = (VOID *) &Req;
  Msg.TxLen = sizeof (Req);
  Msg.RxBuf = (VOID *) &Resp;
  Msg.RxBufLen = sizeof (Resp);
  Msg.RxLen = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n",
      __func__, Status
    ));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a: RPMI CLK_GET_NUM_CLOCKS service failed (error code: %d)\n",
      __func__, Resp.Status));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Get clock rate by RPMI CLK_GET_RATE (0x08).

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  ClockId             The clock ID.
  @param  ClockRate           Clock rate in Hertz.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClkGetRate (
  IN  MPXY_RPMI_CHANNEL     *Chan,
  IN  UINT32                ClockId,
  OUT UINT64                *ClockRate
)
{
  EFI_STATUS                      Status;
  MPXY_RPMI_MESSAGE               Msg;
  RPMI_CLK_GET_RATE_REQ           Req;
  RPMI_CLK_GET_RATE_RESP          Resp;

  if (Chan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI CLOCK channel not inited\n", __func__));
    return EFI_NOT_READY;
  }

  if (ClockRate == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Req.ClockId = ClockId;

  Msg.Type = RpmiMsgTypeNormalRequest;
  Msg.ServiceId = RPMI_CLOCK_SRV_GET_RATE;
  Msg.TxBuf = (VOID *) &Req;
  Msg.TxLen = sizeof (Req);
  Msg.RxBuf = (VOID *) &Resp;
  Msg.RxBufLen = sizeof (Resp);
  Msg.RxLen = NULL;

  Status = MpxyRpmiSendMessage (Chan, &Msg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to send MPXY RPMI message\n",
      __func__, Status
    ));
    return Status;
  }

  if (Resp.Status != RPMI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a: RPMI CLK_GET_NUM_CLOCKS service failed (error code: %d)\n",
      __func__, Resp.Status
    ));
    return EFI_DEVICE_ERROR;
  }

  *ClockRate = RPMI_CLKRATE_U64(Resp.ClockRateHigh, Resp.ClockRateLow);

  return EFI_SUCCESS;
}

/**
  Get Clock ID by Clock Name.

  @param  ClockName           Clock Name.
  @param  ClockCtrlInstance   The pointer to the RPMI_CLOCKCTRL_INSTANCE.
  @param  ClockId             Id of the specified Clock Name.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
GetClockIdByName (
  IN  CONST CHAR8             *ClockName,
  IN  OUT RPMI_CLOCKCTRL_INSTANCE  *ClockCtrlInstance,
  OUT UINT32                  *ClockId
)
{
  UINT32      Index;
  CHAR8       *CurClockName;

  if (ClockCtrlInstance == NULL
      || ClockCtrlInstance->ClockDevices == NULL
      || ClockCtrlInstance->NumDevices == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid ClockCtrl Instance \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (ClockId == NULL
      || ClockName == NULL
      || AsciiStrSize (ClockName) > RPMI_CLK_NAME_LEN) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < ClockCtrlInstance->NumDevices; Index++) {
    CurClockName = ClockCtrlInstance->ClockDevices[Index].ClockName;
    if (CurClockName != NULL && AsciiStrCmp (ClockName, CurClockName) == 0) {
      *ClockId = ClockCtrlInstance->ClockDevices[Index].ClockId;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Initialize a clock device identified by ClockId 

  @param  Chan                RPMI Clock service groups MPXY channel.
  @param  RpmiClk             The RPMI_CLOCK_DEVICE instance of the specified ClockId.
  @param  ClockId             The clock ID.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClockEnumerate (
  IN MPXY_RPMI_CHANNEL             *Chan,
  IN OUT RPMI_CLOCK_DEVICE         *RpmiClk,
  IN UINT32                        ClockId
)
{
  EFI_STATUS    Status;
  UINT64        MinRate;
  UINT64        MaxRate;

  if (Chan == NULL || RpmiClk == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = RpmiClkGetAttributes (Chan, ClockId, RpmiClk);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: Clock id %lu: Failed to Clock Attributes \n",
            __func__, Status, ClockId));
    return Status;
  }

  if (RpmiClk->NumRates == 0) {
    return EFI_SUCCESS;
  }

  if (RpmiClk->Type == RPMI_CLOCK_FORMAT_DISCRETE) {
    RpmiClk->Rates =
      AllocateZeroPool (
          sizeof(CLK_DISCRETE_RATE) * RpmiClk->NumRates
        );
  } else if (RpmiClk->Type == RPMI_CLOCK_FORMAT_LINEAR) {
    RpmiClk->Rates =
      AllocateZeroPool (
          sizeof(CLK_LINEAR_RATE) * RpmiClk->NumRates
        );
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Clock id %lu: Invalid Clock rate type %u \n",
      __func__, ClockId, RpmiClk->Type));
    Status = EFI_INVALID_PARAMETER;
    goto ErrOut;
  }

  if (RpmiClk->Rates == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate memory for RPMI Clock Rates\n",
      __func__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }

  Status = RpmiClkGetSupportedRates (Chan, RpmiClk);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Clock id %lu: Failed to get supported rates \n",
      __func__, Status, ClockId
    ));
    goto FailFreeClockRates;
  }

  if (RpmiClk->Type == RPMI_CLOCK_FORMAT_DISCRETE) {
    MinRate = ((CLK_DISCRETE_RATE *)RpmiClk->Rates)[0].Discrete;
    MaxRate = ((CLK_DISCRETE_RATE *)RpmiClk->Rates)[RpmiClk->NumRates - 1].Discrete;
  } else if (RpmiClk->Type == RPMI_CLOCK_FORMAT_LINEAR) {
    MinRate = ((CLK_LINEAR_RATE *)RpmiClk->Rates)[0].Min;
    MaxRate = ((CLK_LINEAR_RATE *)RpmiClk->Rates)[RpmiClk->NumRates - 1].Max;
  }

  RpmiClk->MinRate = MinRate;
  RpmiClk->MaxRate = MaxRate;

  DEBUG ((DEBUG_INFO,
    "Initialize Clock: "
    "Clock ID = %d, name = %a, NumRates = %d, MaxRate = %llu, MinRate = %llu\n",
    ClockId, RpmiClk->ClockName,
    RpmiClk->NumRates, RpmiClk->MaxRate, RpmiClk->MinRate
  ));

  return EFI_SUCCESS;

FailFreeClockRates:
  FreePool(RpmiClk->Rates);
  RpmiClk->Rates = NULL;
  RpmiClk->NumRates = 0;
ErrOut:
  return Status;
}

/**
  DeInitialize clock devices managed by ClockCtrlInstance.

  @param  ClockCtrlInstance   The pointer to the RPMI_CLOCKCTRL_INSTANCE.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
VOID
RpmiClocksDeinit (
  IN OUT RPMI_CLOCKCTRL_INSTANCE     *ClockCtrlInstance
)
{
  UINT32                ClockId;
  RPMI_CLOCK_DEVICE     *PtrClk;

  if (ClockCtrlInstance == NULL || ClockCtrlInstance->ClockDevices == NULL) {
    return;
  }

  for (ClockId = 0; ClockId < ClockCtrlInstance->NumDevices; ClockId++) {
    PtrClk = &ClockCtrlInstance->ClockDevices[ClockId];
    if (PtrClk->Rates == NULL) {
      continue;
    }
    FreePool(PtrClk->Rates);
    PtrClk->Rates = NULL;
  }

  FreePool (ClockCtrlInstance->ClockDevices);
  ClockCtrlInstance->ClockDevices = NULL;
  ClockCtrlInstance->NumDevices = 0;
}

/**
  Initialize the all the clock devices managed by ClockCtrlInstance

  @param  ClockCtrlInstance   The pointer to the RPMI_CLOCKCTRL_INSTANCE.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClocksInit (
  IN OUT RPMI_CLOCKCTRL_INSTANCE     *ClockCtrlInstance
)
{
  EFI_STATUS                    Status;
  UINT32                        NumDevices;
  UINT32                        ClockId;
  MPXY_RPMI_CHANNEL             *ClkChan;
  RPMI_CLOCK_DEVICE             *ClockDevices;
  RPMI_CLOCK_DEVICE             *PtrClk;

  if (ClockCtrlInstance == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get the RPMI_CLOCKCTRL_INSTANCE\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (ClockCtrlInstance->MpxyRpmiChan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: RPMI Clock Service group channel is not ready \n", __func__));
    return EFI_NOT_READY;
  }
  ClkChan = ClockCtrlInstance->MpxyRpmiChan;

  Status = RpmiClkGetNumClocks (ClkChan, &NumDevices);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to get RPMI clock number \n",
      __func__, Status
    ));
    goto ErrOut;
  }
  ClockCtrlInstance->NumDevices = NumDevices;

  DEBUG ((DEBUG_INFO, "Get %d Clocks \n", NumDevices));

  ClockDevices =
    (RPMI_CLOCK_DEVICE *) AllocateZeroPool (sizeof (RPMI_CLOCK_DEVICE) * NumDevices);
  if (ClockDevices == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for clock devices\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }

  ClockCtrlInstance->ClockDevices = ClockDevices;
  for (ClockId = 0; ClockId < NumDevices; ClockId++) {
    PtrClk = &ClockDevices[ClockId];
    Status = RpmiClockEnumerate (ClkChan, PtrClk, ClockId);
    if (Status != EFI_SUCCESS) {
      DEBUG ((
        DEBUG_ERROR,
        "%a [Status = 0x%x]: Clock ID (%u): Failed to initialize\n",
        __func__, Status, ClockId
      ));
      goto FailFreeClockDevices;
    }
  }

  return EFI_SUCCESS;

FailFreeClockDevices:
  RpmiClocksDeinit (ClockCtrlInstance);
ErrOut:
  ClockCtrlInstance->ClockDevices = NULL;
  ClockCtrlInstance->NumDevices = 0;
  return Status;
}

/**
  Initialize the RPMI clock serive group over MPXY channel

  @param  ClockCtrlInstance   The pointer to the RPMI_CLOCKCTRL_INSTANCE.
  @retval EFI_SUCCESS         Succeed.
  @retval Other               Return error status.

**/
STATIC
EFI_STATUS
RpmiClockChannelInit (
  IN OUT RPMI_CLOCKCTRL_INSTANCE *ClockCtrlInstance
)
{
  EFI_STATUS                          Status;
  UINT32                              MpxyChanCount;
  UINT32                              *MpxyChanIds;
  UINT32                              Index;
  SBI_MPXY_RPMI_CHANNEL_ATTRIBUTES    MpxyRpmiAttrs;
  UINT32                              ClkChanId;
  BOOLEAN                             ClkChanFound;
  MPXY_RPMI_CHANNEL                   *ClkMpxyRpmiChan;

  if (ClockCtrlInstance == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get the RPMI_CLOCKCTRL_INSTANCE\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (ClockCtrlInstance->MpxyRpmiChan != NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SbiMpxyGetChannelCount (&MpxyChanCount);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to get MPXY channel count\n",
      __func__, Status
    ));
    goto ErrOut;
  }

  MpxyChanIds = AllocateZeroPool (sizeof (UINT32) * MpxyChanCount);
  if (MpxyChanIds == NULL) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to allocate memory for MPXY channel IDs\n",
      __func__, Status
    ));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }

  Status = SbiMpxyGetChannelIds (0, MpxyChanCount, MpxyChanIds);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to get all MPXY channel IDs\n",
      __func__, Status
    ));
    Status = EFI_UNSUPPORTED;
    goto FreeMpxyChanIds;
  }

  //
  // Read the RPMI attributes of each MPXY channel, and find the one whose
  // service group ID matches the RPMI Clock service group.
  //
  ClkChanFound = FALSE;
  for (Index = 0; Index < MpxyChanCount; Index++) {
    Status = SbiMpxyReadChannelAttrs (MpxyChanIds[Index],
                                      SbiMpxyChanAttrMsgProtAttrStart,
                                      sizeof (MpxyRpmiAttrs) / sizeof (UINT32),
                                      (UINT32 *) &MpxyRpmiAttrs);
    if (Status != EFI_SUCCESS) {
      continue;
    }

    if (MpxyRpmiAttrs.ServicegroupId == RPMI_SRVGRP_CLOCK) {
      ClkChanId = MpxyChanIds[Index];
      ClkChanFound = TRUE;
      break;
    }
  }
  if (!ClkChanFound) {
    DEBUG ((DEBUG_ERROR, "%a: MPXY channel for RPMI CLOCK service group not found\n", __func__));
    Status = EFI_UNSUPPORTED;
    goto FreeMpxyChanIds;
  }

  ClkMpxyRpmiChan = MpxyRpmiOpenChannel (ClkChanId);
  if (ClkMpxyRpmiChan == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to open MPXY RPMI channel (channel ID: 0x%x)\n",
            __func__, ClkChanId));
    Status = EFI_UNSUPPORTED;
    goto FreeMpxyChanIds;
  }

  ClockCtrlInstance->MpxyRpmiChan = ClkMpxyRpmiChan;

  DEBUG ((DEBUG_INFO, "Initialize RPMI Clock Channel, Channel ID = 0x%x\n", ClkMpxyRpmiChan->ChannelId));

  //
  // MpxyChanIds is a temporary buffer to store all the MPXY channel IDs, and
  // it always should be released at the end.
  //
  FreePool (MpxyChanIds);

  return EFI_SUCCESS;

FreeMpxyChanIds:
  FreePool (MpxyChanIds);
ErrOut:
  return Status;
}

/**
  Deinit the RPMI Clock Channel

  @param  ClockCtrlInstance   The pointer to the RPMI_CLOCKCTRL_INSTANCE

  @retval VOID

**/
STATIC
VOID
RpmiClockChannelDeinit (
  IN OUT RPMI_CLOCKCTRL_INSTANCE *ClockCtrlInstance
)
{
  if (ClockCtrlInstance == NULL || ClockCtrlInstance->MpxyRpmiChan == NULL) {
    return;
  }

  MpxyRpmiCloseChannel (ClockCtrlInstance->MpxyRpmiChan);
  ClockCtrlInstance->MpxyRpmiChan = NULL;
}

/**
  Get the state of the clock specified by ClockName.

  @param[in]  This        A pointer to the SILICON_CLOCKCTRL_PROTOCOL instance.
  @param[in]  ClockName   A pointer to Null-terminated ASCII string to specify
                          the clock name.
  @param[out] ClockState  A pointer to the clock state.

  @retval EFI_SUCCESS             Succeed.
  @retval EFI_INVALID_PARAMETER   One of the input parameters is invalid.
  @retval Other                   Other failures.

**/
STATIC
EFI_STATUS
EFIAPI
ClkGetState (
  IN  SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                 *ClockName,
  OUT UINT32                      *ClockState
)
{
  EFI_STATUS          Status;
  UINT32              ClockId;
  UINT32              RpmiConfig;
  RPMI_CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  if (ClockState == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  ClockCtrlInstance = RPMI_CLOCKCTRL_INSTANCE_FROM_THIS (This);

  Status = GetClockIdByName (ClockName, ClockCtrlInstance, &ClockId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: GetClockIdByName failed \n", __func__, Status));
    return Status;
  }

  Status = RpmiClkGetConfig (ClockCtrlInstance ->MpxyRpmiChan, ClockId, &RpmiConfig);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: RpmiClkGetRate failed \n", __func__, Status));
    return Status;
  }

  *ClockState = RpmiConfig == RPMI_CLOCK_CONFIG_ENABLE ? CLOCK_ENABLED : CLOCK_DISABLED;

  return EFI_SUCCESS;
}

/**
  Set the state of the clock specified by ClockName.

  @param[in]  This        A pointer to the SILICON_CLOCKCTRL_PROTOCOL instance.
  @param[in]  ClockName   A pointer to Null-terminated ASCII string to specify
                          the clock name.
  @param[in]  ClockState  Clock state to be set.

  @retval EFI_SUCCESS         Succeed.
  @retval Other               Other failures.

**/
STATIC
EFI_STATUS
EFIAPI
ClkSetState (
  IN  SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                 *ClockName,
  IN  UINT32                      ClockState
)
{
  EFI_STATUS              Status;
  UINT32                  ClockId;
  RPMI_CLOCKCTRL_INSTANCE      *ClockCtrlInstance;
  UINT32                  RpmiConfig;

  ClockCtrlInstance = RPMI_CLOCKCTRL_INSTANCE_FROM_THIS (This);

  Status = GetClockIdByName (ClockName, ClockCtrlInstance, &ClockId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: GetClockIdByName failed \n", __func__, Status));
    return Status;
  }

  RpmiConfig = ClockState == CLOCK_DISABLED ?
                  RPMI_CLOCK_CONFIG_DISABLE : RPMI_CLOCK_CONFIG_ENABLE;

  Status = RpmiClkSetConfig (ClockCtrlInstance ->MpxyRpmiChan, ClockId, RpmiConfig);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: RpmiClkSetConfig failed \n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Get the current rate of the clock specified by ClockName.

  @param[in]  This        A pointer to the SILICON_CLOCKCTRL_PROTOCOL instance.
  @param[in]  ClockName   A pointer to Null-terminated ASCII string to specify
                          the clock name.
  @param[out] ClockRate   A pointer to the current clock rate in Hz.

  @retval EFI_SUCCESS             Succeed.
  @retval EFI_INVALID_PARAMETER   One of the input parameters is invalid.
  @retval Other                   Other failures.

**/
STATIC
EFI_STATUS
EFIAPI
ClkGetRate (
  IN  SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                 *ClockName,
  OUT UINT64                      *ClockRate
)
{
  EFI_STATUS          Status;
  UINT32              ClockId;
  RPMI_CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  if (ClockRate == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameters \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  ClockCtrlInstance = RPMI_CLOCKCTRL_INSTANCE_FROM_THIS (This);

  Status = GetClockIdByName (ClockName, ClockCtrlInstance, &ClockId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: GetClockIdByName failed \n", __func__, Status));
    return Status;
  }

  Status = RpmiClkGetRate (ClockCtrlInstance ->MpxyRpmiChan, ClockId, ClockRate);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: RpmiClkGetRate failed \n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Set the rate of the clock specified by ClockName.

  @param[in]  This        A pointer to the SILICON_CLOCKCTRL_PROTOCOL instance.
  @param[in]  ClockName   A pointer to Null-terminated ASCII string to specify
                          the clock name.
  @param[in]  ClockRate   The clock rate to set in Hz.

  @retval EFI_SUCCESS         Succeed.
  @retval Other               Other failures.

**/
STATIC
EFI_STATUS
EFIAPI
ClkSetRate (
  IN  SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                 *ClockName,
  IN  UINT64                      ClockRate
)
{
  EFI_STATUS                    Status;
  UINT32                        ClockId;
  RPMI_CLOCKCTRL_INSTANCE       *ClockCtrlInstance;
  UINT32                        Flags;

  ClockCtrlInstance = RPMI_CLOCKCTRL_INSTANCE_FROM_THIS (This);

  Status = GetClockIdByName (ClockName, ClockCtrlInstance, &ClockId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: GetClockIdByName failed \n", __func__, Status));
    return Status;
  }

  // Use Round Down Mode by default
  Flags = RATE_ROUND_DOWN;
  Status = RpmiClkSetRate (ClockCtrlInstance ->MpxyRpmiChan, ClockId, Flags, ClockRate);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: RpmiClkGetRate failed \n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Get the maximum supported rate of the clock specified by ClockName.

  @param[in]  This        A pointer to the SILICON_CLOCKCTRL_PROTOCOL instance.
  @param[in]  ClockName   A pointer to Null-terminated ASCII string to specify
                          the clock name.
  @param  ClockRate       A pointer to the maximum clock rate in Hz.

  @retval EFI_SUCCESS         Succeed.
  @retval Other               Other failures.

**/
STATIC
EFI_STATUS
EFIAPI
ClkGetMaxRate (
  IN  SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                 *ClockName,
  OUT UINT64                      *ClockRate
  )
{
  EFI_STATUS          Status;
  UINT32              ClockId;
  RPMI_CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  ClockCtrlInstance = RPMI_CLOCKCTRL_INSTANCE_FROM_THIS (This);

  Status = GetClockIdByName (ClockName, ClockCtrlInstance, &ClockId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: GetClockIdByName failed \n", __func__, Status));
    return Status;
  }

  *ClockRate = ClockCtrlInstance->ClockDevices[ClockId].MaxRate;

  return EFI_SUCCESS;
}

/**
  Get the minimum supported rate of the clock specified by ClockName.

  @param[in]  This        A pointer to the SILICON_CLOCKCTRL_PROTOCOL instance.
  @param[in]  ClockName   A pointer to Null-terminated ASCII string to specify
                          the clock name.
  @param[out] ClockRate   A pointer to the minimum clock rate in Hz.

  @retval EFI_SUCCESS         Succeed.
  @retval Other               Other failures.

**/
STATIC
EFI_STATUS
EFIAPI
ClkGetMinRate (
  IN  SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                 *ClockName,
  OUT UINT64                      *ClockRate
)
{
  EFI_STATUS          Status;
  UINT32              ClockId;
  RPMI_CLOCKCTRL_INSTANCE  *ClockCtrlInstance;

  ClockCtrlInstance = RPMI_CLOCKCTRL_INSTANCE_FROM_THIS (This);

  Status = GetClockIdByName (ClockName, ClockCtrlInstance, &ClockId);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a [Status = 0x%x]: GetClockIdByName failed \n", __func__, Status));
    return Status;
  }

  *ClockRate = ClockCtrlInstance->ClockDevices[ClockId].MinRate;

  return EFI_SUCCESS;
}

/**
  Register RISC-V RPMI clock conctrl Protocol

  @param  ImageHandle   Pointer of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
EFIAPI
RiscvSbiMpxyRpmiClockDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
)
{
  EFI_STATUS                Status;
  RPMI_CLOCKCTRL_INSTANCE   *ClockCtrlInstance;

  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gSpacemitSiliconClockCtrlProtocolGuid);

  ClockCtrlInstance = AllocateZeroPool (sizeof (RPMI_CLOCKCTRL_INSTANCE));
  if (ClockCtrlInstance == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for ClockCtrl Protocol \n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto ErrOut;
  }

  ClockCtrlInstance->Signature = RPMI_CLOCK_SIGNATURE;
  ClockCtrlInstance->ClockCtrlProtocol.GetClockState   = ClkGetState;
  ClockCtrlInstance->ClockCtrlProtocol.SetClockState   = ClkSetState;
  ClockCtrlInstance->ClockCtrlProtocol.GetClockRate    = ClkGetRate;
  ClockCtrlInstance->ClockCtrlProtocol.GetMaxClockRate = ClkGetMaxRate;
  ClockCtrlInstance->ClockCtrlProtocol.GetMinClockRate = ClkGetMinRate;
  ClockCtrlInstance->ClockCtrlProtocol.SetClockRate    = ClkSetRate;

  Status = RpmiClockChannelInit (ClockCtrlInstance);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to initialize RPMI Clock Channel \n",
      __func__, Status
    ));
    goto FailFreeClkCtrlInstance;
  }

  Status = RpmiClocksInit (ClockCtrlInstance);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to initialize RPMI Clock devices \n",
      __func__, Status
    ));
    goto FailFreeRpmiClkChannel;
  }

  Status = gBS->InstallProtocolInterface (
                                          &ClockCtrlInstance->Handle,
                                          &gSpacemitSiliconClockCtrlProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &ClockCtrlInstance->ClockCtrlProtocol
                                          );
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR,
      "%a [Status = 0x%x]: Failed to install ClockCtrl Protocol \n",
      __func__, Status
    ));
    goto FailFreeRpmiClocks;
  }

  return EFI_SUCCESS;

FailFreeRpmiClocks:
  RpmiClocksDeinit(ClockCtrlInstance);
FailFreeRpmiClkChannel:
  RpmiClockChannelDeinit (ClockCtrlInstance);
FailFreeClkCtrlInstance:
  FreePool (ClockCtrlInstance);
ErrOut:
  return Status;
}
