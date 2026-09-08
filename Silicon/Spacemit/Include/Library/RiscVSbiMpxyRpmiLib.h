/** @file
  RISC-V RPMI interfaces integrated with SBI MPXY extension.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __RISCV_SBI_MPXY_RPMI_LIB_H__
#define __RISCV_SBI_MPXY_RPMI_LIB_H__

#include <Base.h>
#include <IndustryStandard/Rpmi.h>
#include <Library/RiscVSbiMpxyLib.h>

//
// RPMI specific SBI MPXY channel attribute IDs
//
typedef enum {
  SbiMpxyChanAttrRpmiServicegroupId         = 0x80000000,
  SbiMpxyChanAttrRpmiServicegroupVersion    = 0x80000001,
  SbiMpxyChanAttrRpmiImplementationId       = 0x80000002,
  SbiMpxyChanAttrRpmiImplementationVersion  = 0x80000003,
} SBI_MPXY_RPMI_CHANNEL_ATTRIBUTE_ID;

#pragma pack(1)
//
// RPMI specific SBI MPXY channel attributes
//
typedef struct {
  UINT32  ServicegroupId;
  UINT32  ServicegroupVersion;
  UINT32  ImplementationId;
  UINT32  ImplementationVersion;
} SBI_MPXY_RPMI_CHANNEL_ATTRIBUTES;
#pragma pack()

typedef struct {
  LIST_ENTRY                          Link;

  UINT32                              ChannelId;
  SBI_MPXY_CHANNEL_ATTRIBUTES         StdAttrs;
  SBI_MPXY_RPMI_CHANNEL_ATTRIBUTES    RpmiAttrs;

  INTN                                RefCount;
} MPXY_RPMI_CHANNEL;

typedef struct {
  RPMI_MESSAGE_TYPE         Type;
  UINT32                    ServiceId;
  VOID                      *TxBuf;
  UINTN                     TxLen;
  VOID                      *RxBuf;
  UINTN                     RxBufLen;
  UINTN                     *RxLen;
} MPXY_RPMI_MESSAGE;

/**
  Open a MPXY RPMI channel.

  @param  ChannelId     The ID of the channel to be opened.

  @retval A MPXY_RPMI_CHANNEL instance corresponding to the ID on success,
          otherwise NULL on failure.

**/
MPXY_RPMI_CHANNEL *
EFIAPI
MpxyRpmiOpenChannel (
  IN  UINT32    ChannelId
  );

/**
  Close a MPXY RPMI channel.

  @param  Chan          The MPXY_RPMI_CHANNEL instance to be closed.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
MpxyRpmiCloseChannel (
  IN  MPXY_RPMI_CHANNEL   *Chan
  );

/**
  Get the maximum message length of a MPXY RPMI channel.

  @param  Chan      MPXY RPMI channel.

  @retval The maximum message length in bytes.

**/
UINT32
EFIAPI
MpxyRpmiMessageLengthMax (
  IN  CONST MPXY_RPMI_CHANNEL   *Chan
  );

/**
  Send message over a MPXY RPMI channel.

  @param  Chan          MPXY RPMI channel.
  @param  Msg           MPXY RPMI message to be sent.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
MpxyRpmiSendMessage (
  IN  CONST MPXY_RPMI_CHANNEL   *Chan,
  IN  OUT   MPXY_RPMI_MESSAGE   *Msg
  );

#endif /* ifndef __RISCV_SBI_MPXY_RPMI_LIB_H__ */
