/** @file
  RISC-V SBI MPXY interfaces

  Copyright (c) 2024, Ventana Micro Systems, Inc.
  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __RISCV_SBI_MPXY_LIB_H__
#define __RISCV_SBI_MPXY_LIB_H__

#include <Base.h>

#pragma pack(1)

//
// SBI MPXY channel attribute IDs
//
typedef enum {
  //
  // Standard attributes, defined by the SBI MPXY extension.
  //
  SbiMpxyChanAttrMsgProtId                  = 0x00000000,
  SbiMpxyChanAttrMsgProtVersion             = 0x00000001,
  SbiMpxyChanAttrMsgDataMaxLen              = 0x00000002,
  SbiMpxyChanAttrMsgSendTimeout             = 0x00000003,
  SbiMpxyChanAttrMsgCompletionTimeout       = 0x00000004,
  SbiMpxyChanAttrChannelCapability          = 0x00000005,
  SbiMpxyChanAttrSseEventId                 = 0x00000006,
  SbiMpxyChanAttrMsiControl                 = 0x00000007,
  SbiMpxyChanAttrMsiAddrLow                 = 0x00000008,
  SbiMpxyChanAttrMsiAddrHigh                = 0x00000009,
  SbiMpxyChanAttrMsiData                    = 0x0000000a,
  SbiMpxyChanAttrEventsStateControl         = 0x0000000b,
  SbiMpxyChanAttrStdAttrMax,
  //
  // Message protocol specific attributes,
  // defined by the message protocol specification.
  //
  SbiMpxyChanAttrMsgProtAttrStart           = 0x80000000,
  SbiMpxyChanAttrMsgProtAttrEnd             = 0xffffffff,
} SBI_MPXY_CHANNEL_ATTRIBUTE_ID;

//
// SBI MPXY standard channel attributes
//
typedef struct {
  UINT32  MsgProtId;
  UINT32  MsgProtVersion;
  UINT32  MsgDataMaxLen;
  UINT32  MsgSendTimeout;
  UINT32  MsgCompletionTimeout;
  UINT32  ChannelCapability;
  UINT32  SseEventId;
  UINT32  MsiControl;
  UINT32  MsiAddrLow;
  UINT32  MsiAddrHigh;
  UINT32  MsiData;
  UINT32  EventsStateControl;
} SBI_MPXY_CHANNEL_ATTRIBUTES;

//
// SBI MPXY MSG_PROT_VERSION attribute encoding
//
#define SBI_MPXY_MSG_PROT_VERSION_MAJOR(Ver)        (((Ver) >> 16) & 0xffff)
#define SBI_MPXY_MSG_PROT_VERSION_MINOR(Ver)        ((Ver) & 0xffff)
#define SBI_MPXY_MSG_PROT_VERSION(Major, Minor)     (((Major) << 16) | (Minor))

//
// SBI MPXY CHANNEL_CAPABILITY attribute bits
//
#define SBI_MPXY_CHAN_CAP_MSI                       BIT0
#define SBI_MPXY_CHAN_CAP_SSE                       BIT1
#define SBI_MPXY_CHAN_CAP_EVENTS_STATE              BIT2
#define SBI_MPXY_CHAN_CAP_SEND_MSG_WITH_RESP        BIT3
#define SBI_MPXY_CHAN_CAP_SEND_MSG_WITHOUT_RESP     BIT4
#define SBI_MPXY_CHAN_CAP_GET_NOTIFICATIONS         BIT5

//
// SBI MPXY message protocol IDs
//
typedef enum {
  SbiMpxyMsgProtIdRpmi                      = 0x00000000,
  SbiMpxyMsgProtIdReservedStart             = 0x00000001,
  SbiMpxyMsgProtIdReservedEnd               = 0x7fffffff,
  SbiMpxyMsgProtIdVendorSpecificStart       = 0x80000000,
  SbiMpxyMsgProtIdVendorSpecificEnd         = 0xffffffff,
} SBI_MPXY_MSG_PROT_ID;

//
// SBI MPXY shared memory flags
//
#define SBI_MPXY_SHMEM_FLAG_OVERWRITE           0x0
#define SBI_MPXY_SHMEM_FLAG_OVERWRITE_RETURN    0x1

//
// SBI MPXY channel IDs data in shared memory
//
typedef struct {
  UINT32  Remaining;
  UINT32  Returned;
  UINT32  ChannelIds[0];
} SBI_MPXY_CHANNEL_IDS_DATA;

//
// SBI MPXY notification events data in shared memory
//
typedef struct {
  UINT32  Remaining;
  UINT32  Returned;
  UINT32  Lost;
  UINT32  Reserved;
  UINT8   EventsData[0];
} SBI_MPXY_NOTIFICATION_EVENTS_DATA;

#pragma pack()

/**
  Get SBI MPXY shared memory size.

  @param  ShmemSize     The shared memory size get in bytes.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxyGetShmemSize (
  OUT UINT64  *ShmemSize
  );

/**
  Set SBI MPXY shared memory.

  @param  ShmemPhysAddr   Shared memory physical base address.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxySetShmem (
  IN  UINT64    ShmemPhysAddr,
  IN  UINTN     Flags
  );

/**
  Set SBI MPXY shared memory with or without getting the previous shared memory address.

  @param  ShmemPhysAddr       Shared memory physical base address.
  @param  GetPrevShmemAddr    Whether to get the previous shared memory address.
  @param  PrevShmemPhysAddr   Previous shared memory address. It can be NULL when GetPrevShmemAddr is FALSE.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxySetShmemWithPrevGot (
  IN  UINT64    ShmemPhysAddr,
  IN  BOOLEAN   GetPrevShmemAddr,
  OUT UINT64    *PrevShmemPhysAddr
  );

/**
  Unset (disable) SBI MPXY shared memory.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxyUnsetShmem (
  VOID
  );

/**
  Get the count of SBI MPXY channels.

  @param  ChannelCount  The channel count got.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

 **/
EFI_STATUS
EFIAPI
SbiMpxyGetChannelCount (
  OUT UINT32  *ChannelCount
  );

/**
  Get SBI MPXY channel IDs.

  @param  StartIndex    The index of the first channel ID.
  @param  ChannelCount  The count of channel IDs to get.
  @param  ChannelIds    An array to store the channel IDs got. Caller should
                        ensure it is large enough to store all of them.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxyGetChannelIds (
  IN  UINT32  StartIndex,
  IN  UINT32  ChannelCount,
  OUT UINT32  *ChannelIds
  );

/**
  Read SBI MPXY channel attributes.

  @param  ChannelId     The channel whose attributes to be read.
  @param  BaseAttrId    The start index of the attribute range.
  @param  AttrCount     The count of attributes to read.
  @param  Attrs         An array to store the attributes read. Caller should
                        ensure it is large enough to store all of them.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxyReadChannelAttrs (
  IN  UINT32  ChannelId,
  IN  UINT32  BaseAttrId,
  IN  UINT32  AttrCount,
  OUT UINT32  *Attrs
  );

/**
  Write SBI MPXY channel attributes.

  @param  ChannelId     The channel whose attributes to be written.
  @param  BaseAttrId    The start index of the attribute range.
  @param  AttrCount     The count of attributes to write.
  @param  Attrs         An array storing the attributes to write.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxyWriteChannelAttrs (
  IN        UINT32  ChannelId,
  IN        UINT32  BaseAttrId,
  IN        UINT32  AttrCount,
  IN CONST  UINT32  *Attrs
  );

/**
  SBI MPXY send message with waiting for a response.

  @param  ChannelId     MPXY channel ID.
  @param  MessageId     Message protocol specific ID of the message to be sent.
  @param  TxBuf         A buffer storing the message data to be sent.
                        It can be NULL if the message data is empty.
  @param  TxLen         The length of the message data to be sent, in bytes.
                        Caller should ensure it doesn't exceed the size of MPXY
                        shared memory. It can be 0 if the message data is empty.
  @param  RxBuf         A buffet to store the response data. Caller should
                        ensure it is large enough to store all the response data.
                        It can be NULL if the response data is empty or the
                        caller doesn't care about it.
  @param  RxBufLen      The length of RxBuf, in bytes.
  @param  RxLen         The length of the response data, in bytes.
                        It can be NULL if the caller doesn't care about it.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxySendMessageWithResponse (
  IN        UINT32    ChannelId,
  IN        UINT32    MessageId,
  IN  CONST VOID      *TxBuf      OPTIONAL,
  IN        UINTN     TxLen       OPTIONAL,
  OUT       VOID      *RxBuf      OPTIONAL,
  IN        UINTN     RxBufLen    OPTIONAL,
  OUT       UINTN     *RxLen      OPTIONAL
  );

/**
  SBI MPXY send message without waiting for a response.

  @param  ChannelId     MPXY channel ID.
  @param  MessageId     Message protocol specific ID of the message to be sent.
  @param  TxBuf         A buffer storing the message data to be sent.
                        It can be NULL if the message data is empty.
  @param  TxLen         The length of the message data to be sent, in bytes.
                        Caller should ensure it doesn't exceed the size of MPXY
                        shared memory. It can be 0 if the message data is empty.

  @retval EFI_SUCCESS   Succeed.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxySendMessageWithoutResponse (
  IN        UINT32    ChannelId,
  IN        UINT32    MessageId,
  IN  CONST VOID      *TxBuf      OPTIONAL,
  IN        UINTN     TxLen       OPTIONAL
  );

/**
  SBI MPXY get notifications.

  @param  ChannelId       MPXY channel ID.
  @param  NotifData       A buffer to store the notification events data got.
                          Caller should ensure it is large enough to store all
                          the data.
  @param  EventsDataLen   The length of events data got, in bytes.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

 **/
EFI_STATUS
EFIAPI
SbiMpxyGetNotificationEvents (
  IN  UINT32                              ChannelId,
  OUT SBI_MPXY_NOTIFICATION_EVENTS_DATA   *NotifData,
  OUT UINTN                               *EventsDataLen
  );

#endif /* ifndef __RISCV_SBI_MPXY_LIB_H__ */
