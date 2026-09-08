/** @file
  RISC-V SBI MPXY interfaces

  Copyright (c) 2024, Ventana Micro Systems, Inc.
  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseRiscVSbiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Library/RiscVSbiMpxyLib.h>

#define SBI_MPXY_INVAL_PHYS_ADDR    (-1U)

STATIC VOID     *mTempShmemAddr   = NULL;
STATIC UINT64   mTempShmemSize    = 0;

STATIC
inline
VOID *
AllocateMpxyShmem(
  IN  UINT64  Size
  )
{
  // The shared memory must be 4096 bytes aligned.
  return AllocateAlignedPages (EFI_SIZE_TO_PAGES (Size), SIZE_4KB);
}

STATIC
inline
VOID
FreeMpxyShmem (
  IN  VOID    *Addr,
  IN  UINT64  Size
  )
{
  FreeAlignedPages (Addr, EFI_SIZE_TO_PAGES (Size));
}

STATIC
BOOLEAN
SbiMpxyExtAvailable (
  VOID
  )
{
  SBI_RET   Ret;

  Ret = SbiCall (SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, 1, SBI_EXT_MPXY);
  if (Ret.Error != SBI_SUCCESS) {
    return FALSE;
  }

  return (Ret.Value == 1) ? TRUE : FALSE;
}

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
  )
{
  SBI_RET   Ret;

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_GET_SHMEM_SIZE, 0);
  if (Ret.Error != SBI_SUCCESS) {
    return TranslateError (Ret.Error);
  }

  *ShmemSize = Ret.Value;
  return EFI_SUCCESS;
}

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
  )
{
  SBI_RET   Ret;

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_SET_SHMEM, 3,
                 ShmemPhysAddr, 0, Flags);
  if (Ret.Error != SBI_SUCCESS) {
    return TranslateError (Ret.Error);
  }

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS  Status;
  UINT64      Flags;
  UINTN       *ShmemBuf;

  if (GetPrevShmemAddr) {
    if (PrevShmemPhysAddr == NULL) {
      return EFI_INVALID_PARAMETER;
    }
    Flags = SBI_MPXY_SHMEM_FLAG_OVERWRITE_RETURN;
  } else {
    Flags = SBI_MPXY_SHMEM_FLAG_OVERWRITE;
  }

  Status = SbiMpxySetShmem (ShmemPhysAddr, Flags);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  if (GetPrevShmemAddr) {
    ShmemBuf = (UINTN *) ShmemPhysAddr;
    *PrevShmemPhysAddr = ShmemBuf[0];
  }

  return EFI_SUCCESS;
}

/**
  Unset (disable) SBI MPXY shared memory.

  @retval EFI_SUCCESS     Succeed.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
SbiMpxyUnsetShmem (
  VOID
  )
{
  SBI_RET   Ret;

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_SET_SHMEM, 3,
                 SBI_MPXY_INVAL_PHYS_ADDR, SBI_MPXY_INVAL_PHYS_ADDR, 0);
  if (Ret.Error != SBI_SUCCESS) {
    return TranslateError (Ret.Error);
  }

  return EFI_SUCCESS;
}

/**
  Set temporary SBI MPXY shared memory, with saving the previous shared memory address.

  @param  PrevShmemAddr   A variable to save the previous shared memory address.

  @retval The temporary shared memory on success, otherwise NULL on failure.

**/
STATIC
VOID *
SbiMpxySetTempShmem (
  OUT UINT64  *PrevShmemAddr
  )
{
  EFI_STATUS    Status;
  UINT64        PrevAddr;

  if (mTempShmemAddr == NULL || PrevShmemAddr == NULL) {
    return NULL;
  }

  Status = SbiMpxySetShmemWithPrevGot ((UINT64) mTempShmemAddr, TRUE, &PrevAddr);
  if (Status != EFI_SUCCESS) {
    return NULL;
  }

  *PrevShmemAddr = PrevAddr;
  return mTempShmemAddr;
}

/**
  Restore back the previous shared memory.

  @param  PrevShmemAddr     The previous shared memory address.

  @retval EFI_SUCCESS       Succeed.
  @retval Other             Return error status.

**/
STATIC
EFI_STATUS
SbiMpxyRestorePrevShmem (
  IN  UINT64  PrevShmemAddr
  )
{
  return SbiMpxySetShmemWithPrevGot (PrevShmemAddr, FALSE, NULL);
}

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
  )
{
  EFI_STATUS                  Status;
  SBI_RET                     Ret;
  UINT64                      PrevShmemAddr;
  UINT32                      StartIndex;
  SBI_MPXY_CHANNEL_IDS_DATA   *Shmem;

  if (ChannelCount == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Out;
  }

  Shmem = (SBI_MPXY_CHANNEL_IDS_DATA *) SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  StartIndex = 0;
  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_GET_CHANNEL_IDS, 1, StartIndex);
  if (Ret.Error != SBI_SUCCESS) {
    Status = TranslateError (Ret.Error);
    goto RestorePrevShmem;
  }

  *ChannelCount = Shmem->Remaining + Shmem->Returned;

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

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
  )
{
  EFI_STATUS                  Status;
  SBI_RET                     Ret;
  UINT64                      PrevShmemAddr;
  UINT32                      Remaining;
  UINT32                      Returned;
  UINT32                      ChannelIndex;
  UINT32                      RetIndex;
  SBI_MPXY_CHANNEL_IDS_DATA   *Shmem;

  if (ChannelCount == 0 || ChannelIds == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Out;
  }

  Shmem = (SBI_MPXY_CHANNEL_IDS_DATA *) SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  ChannelIndex = 0;
  do {
    Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_GET_CHANNEL_IDS, 1, StartIndex);
    if (Ret.Error != SBI_SUCCESS) {
      Status = TranslateError (Ret.Error);
      goto RestorePrevShmem;
    }

    Remaining = Shmem->Remaining;
    Returned = Shmem->Returned;

    for (RetIndex = 0; RetIndex < Returned && ChannelIndex < ChannelCount; RetIndex++) {
      ChannelIds[ChannelIndex] = Shmem->ChannelIds[RetIndex];
      ChannelIndex++;
    }

    StartIndex += Returned;
  } while (Remaining > 0);

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

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
  )
{
  EFI_STATUS      Status;
  SBI_RET         Ret;
  VOID            *Shmem;
  UINT64          PrevShmemAddr;

  if (AttrCount == 0 || Attrs == NULL || sizeof (UINT32) * AttrCount > mTempShmemSize) {
    Status = EFI_INVALID_PARAMETER;
    goto Out;
  }

  Shmem = SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_READ_ATTRS, 3,
                 ChannelId, BaseAttrId, AttrCount);
  if (Ret.Error != SBI_SUCCESS) {
    Status = TranslateError (Ret.Error);
    goto RestorePrevShmem;
  }

  CopyMem ((VOID *) Attrs, Shmem, sizeof (UINT32) * AttrCount);

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

/**
  Write SBI MPXY channel attributes.

  @param  ChannelId     The channel whose attributes to be written.
  @param  BaseAttrId    The start index of the attribute range.
  @param  AttrCount     The count of attributes to write.
  @param  Attrs         An array storing the attributes to be written.

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
  )
{
  EFI_STATUS      Status;
  SBI_RET         Ret;
  VOID            *Shmem;
  UINT64          PrevShmemAddr;

  if (AttrCount == 0 || Attrs == NULL || sizeof (UINT32) * AttrCount > mTempShmemSize) {
    Status = EFI_INVALID_PARAMETER;
    goto Out;
  }

  Shmem = SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  CopyMem (Shmem, (CONST VOID *) Attrs, sizeof (UINT32) * AttrCount);

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_WRITE_ATTRS, 3,
                 ChannelId, BaseAttrId, AttrCount);
  if (Ret.Error != SBI_SUCCESS) {
    Status = TranslateError (Ret.Error);
    goto RestorePrevShmem;
  }

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

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
  )
{
  EFI_STATUS      Status;
  SBI_RET         Ret;
  VOID            *Shmem;
  UINT64          PrevShmemAddr;
  UINTN           RxBytes;

  if ((TxLen != 0 && TxBuf == NULL) || TxLen > mTempShmemSize || RxBufLen > mTempShmemSize) {
    Status = EFI_INVALID_PARAMETER;
    goto Out;
  }

  Shmem = SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  if (TxLen != 0) {
    CopyMem (Shmem, TxBuf, TxLen);
  }

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_SEND_MSG_WITH_RESP, 3,
                 ChannelId, MessageId, TxLen);
  if (Ret.Error != SBI_SUCCESS) {
    Status = TranslateError (Ret.Error);
    goto RestorePrevShmem;
  }

  if (RxBuf != NULL) {
    RxBytes = Ret.Value;
    if (RxBytes > RxBufLen) {
      Status = EFI_UNSUPPORTED;
      goto RestorePrevShmem;
    }

    CopyMem (RxBuf, Shmem, RxBytes);
    if (RxLen != NULL) {
      *RxLen = RxBytes;
    }
  }

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

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
  )
{
  EFI_STATUS      Status;
  SBI_RET         Ret;
  VOID            *Shmem;
  UINT64          PrevShmemAddr;

  if ((TxLen != 0 && TxBuf == NULL) || TxLen > mTempShmemSize) {
    Status = EFI_INVALID_PARAMETER;
    goto Out;
  }

  Shmem = SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  if (TxLen != 0) {
    CopyMem (Shmem, TxBuf, TxLen);
  }

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_SEND_MSG_NO_RESP, 3,
                 ChannelId, MessageId, TxLen);
  if (Ret.Error != SBI_SUCCESS) {
    Status = TranslateError (Ret.Error);
    goto RestorePrevShmem;
  }

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

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
  )
{
  EFI_STATUS      Status;
  SBI_RET         Ret;
  VOID            *Shmem;
  UINT64          PrevShmemAddr;

  if (NotifData == NULL || EventsDataLen == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  Shmem = SbiMpxySetTempShmem (&PrevShmemAddr);
  if (Shmem == NULL) {
    Status = EFI_UNSUPPORTED;
    goto Out;
  }

  Ret = SbiCall (SBI_EXT_MPXY, SBI_EXT_MPXY_GET_NOTIFICATION_EVENTS, 1, ChannelId);
  if (Ret.Error != SBI_SUCCESS) {
    Status = TranslateError (Ret.Error);
    goto RestorePrevShmem;
  }

  CopyMem ((VOID *) NotifData, Shmem, Ret.Value + sizeof (SBI_MPXY_NOTIFICATION_EVENTS_DATA));
  *EventsDataLen = Ret.Value;

  Status = EFI_SUCCESS;

RestorePrevShmem:
  SbiMpxyRestorePrevShmem (PrevShmemAddr);
Out:
  return Status;
}

/**
  Constructor to do some initialization for SBI MPXY Extension.

  @param  ImageHandle   The image handle.
  @param  SystemTable   The system table.

  @retval EFI_SUCCESS   Init successfully.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
RiscVSbiMpxyLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS      Status;
  UINT64          ShmemSize;
  VOID            *Shmem;

  if (!SbiMpxyExtAvailable()) {
    DEBUG ((DEBUG_INFO, "%a: SBI MPXY Extension not available\n", __func__));
    //
    // To be compatible with the SBI implementation that MPXY is not available,
    // we should return EFI_SUCCESS here, otherwise the whole EDK2 will hang due
    // to the library constructor failure.
    //
    Status = EFI_SUCCESS;
    goto Out;
  }

  Status = SbiMpxyGetShmemSize (&ShmemSize);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_WARN, "%a: Failed to get SBI MPXY shared memory size\n", __func__));
    goto Out;
  }

  Shmem = AllocateMpxyShmem (ShmemSize);
  if (Shmem == NULL) {
    DEBUG ((DEBUG_WARN, "%a: Failed to allocate SBI MPXY shared memory\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Out;
  }

  DEBUG ((DEBUG_INFO, "%a: Allocate %lu bytes for SBI MPXY shared memory\n", __func__, ShmemSize));

  mTempShmemAddr = Shmem;
  mTempShmemSize = ShmemSize;

  Status = EFI_SUCCESS;
Out:
  return Status;
}

/**
  Destructor to free resources.

  @param  ImageHandle   The image handle.
  @param  SystemTable   The system table.

  @retval EFI_SUCCESS   Always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
RiscVSbiMpxyLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  //
  // Free shared memory.
  //
  if (mTempShmemAddr != NULL && mTempShmemSize != 0) {
    FreeMpxyShmem (mTempShmemAddr, mTempShmemSize);
    mTempShmemAddr = NULL;
    mTempShmemSize = 0;
  }

  return EFI_SUCCESS;
}
