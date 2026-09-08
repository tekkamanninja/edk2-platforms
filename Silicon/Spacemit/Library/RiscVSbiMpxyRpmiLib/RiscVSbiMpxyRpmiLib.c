/** @file
  RISC-V RPMI interfaces integrated with SBI MPXY extension.

  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/RiscVSbiMpxyLib.h>

#include <Library/RiscVSbiMpxyRpmiLib.h>

//
// Helper macro to traverse each entry in a list.
//
#define LIST_FOR_EACH_ENTRY(Entry, Head)          \
  for (Entry = GetFirstNode (Head);               \
       !IsNull (Head, Entry);                     \
       Entry = GetNextNode (Head, Entry))

//
// Helper macro to get the MPXY_RPMI_CHANNEL instance from its LIST_ENTRY member.
//
#define LIST_ENTRY_TO_MPXY_RPMI_CHANNEL(Entry)    \
  BASE_CR (Entry, MPXY_RPMI_CHANNEL, Link)

//
// A list to store MPXY_RPMI_CHANNEL instances.
//
STATIC LIST_ENTRY   mMpxyRpmiChanList;

STATIC
inline
MPXY_RPMI_CHANNEL *
AllocateMpxyRpmiChannel (
  VOID
  )
{
  return (MPXY_RPMI_CHANNEL *) AllocateZeroPool (sizeof (MPXY_RPMI_CHANNEL));
}

STATIC
inline
VOID
FreeMpxyRpmiChannel (
  IN  MPXY_RPMI_CHANNEL   *Chan
  )
{
  FreePool (Chan);
}

STATIC
VOID
FreeAllMpxyRpmiChannels (
  VOID
  )
{
  LIST_ENTRY            *Entry;
  LIST_ENTRY            *NextEntry;
  MPXY_RPMI_CHANNEL     *Chan;

  Entry = GetFirstNode (&mMpxyRpmiChanList);
  while (!IsNull (&mMpxyRpmiChanList, Entry)) {
    Chan = LIST_ENTRY_TO_MPXY_RPMI_CHANNEL (Entry);
    NextEntry = GetNextNode (&mMpxyRpmiChanList, Entry);

    RemoveEntryList (Entry);
    FreeMpxyRpmiChannel (Chan);

    Entry = NextEntry;
  }
}

STATIC
MPXY_RPMI_CHANNEL *
FindMpxyRpmiChannelFromList (
  IN  UINT32    ChannelId
  )
{
  LIST_ENTRY          *Entry;
  MPXY_RPMI_CHANNEL   *Chan;

  LIST_FOR_EACH_ENTRY (Entry, &mMpxyRpmiChanList) {
    Chan = LIST_ENTRY_TO_MPXY_RPMI_CHANNEL (Entry);

    if (Chan->ChannelId == ChannelId) {
      return Chan;
    }
  }

  return NULL;
}

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
  )
{
  MPXY_RPMI_CHANNEL     *Chan;
  EFI_STATUS            Status;

  Chan = FindMpxyRpmiChannelFromList (ChannelId);
  if (Chan != NULL) {
    Chan->RefCount++;
    return Chan;
  }

  Chan = AllocateMpxyRpmiChannel ();
  if (Chan == NULL) {
    DEBUG ((DEBUG_WARN, "%a: MPXY channel 0x%x: failed to allocate channel instance\n",
            __func__, ChannelId));
    goto ErrOut;
  }

  Chan->ChannelId = ChannelId;
  Chan->RefCount = 1;

  Status = SbiMpxyReadChannelAttrs (ChannelId,
                                    SbiMpxyChanAttrMsgProtId,
                                    sizeof (Chan->StdAttrs) / sizeof (UINT32),
                                    (UINT32 *) &Chan->StdAttrs);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_WARN, "%a: MPXY channel 0x%x: failed to read standard attributes\n",
            __func__, ChannelId));
    goto FreeMpxyRpmiChan;
  }

  if (Chan->StdAttrs.MsgProtId != SbiMpxyMsgProtIdRpmi) {
    DEBUG ((DEBUG_WARN, "%a: MPXY channel 0x%x uses protocol with ID 0x%x rather than RPMI\n",
            __func__, ChannelId, Chan->StdAttrs.MsgProtId));
    goto FreeMpxyRpmiChan;
  }

  Status = SbiMpxyReadChannelAttrs (ChannelId,
                                    SbiMpxyChanAttrMsgProtAttrStart,
                                    sizeof (Chan->RpmiAttrs) / sizeof (UINT32),
                                    (UINT32 *) &Chan->RpmiAttrs);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_WARN, "%a: MPXY channel 0x%x: failed to read RPMI attributes\n",
            __func__, ChannelId));
    goto FreeMpxyRpmiChan;
  }

  InsertTailList (&mMpxyRpmiChanList, &Chan->Link);

  return Chan;

FreeMpxyRpmiChan:
  FreeMpxyRpmiChannel (Chan);
ErrOut:
  return NULL;
}

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
  )
{
  if (Chan == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Chan->RefCount > 0) {
    Chan->RefCount--;
    if (Chan->RefCount <= 0) {
      RemoveEntryList (&Chan->Link);
      FreeMpxyRpmiChannel (Chan);
    }
  }

  return EFI_SUCCESS;
}

/**
  Get the maximum message length of a MPXY RPMI channel.

  @param  Chan      MPXY RPMI channel.

  @retval The maximum message length in bytes.

**/
UINT32
EFIAPI
MpxyRpmiMessageLengthMax (
  IN  CONST MPXY_RPMI_CHANNEL   *Chan
  )
{
  return Chan->StdAttrs.MsgDataMaxLen;
}

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
  )
{
  EFI_STATUS    Status;

  switch (Msg->Type) {
    case RpmiMsgTypeNormalRequest:
      if ((Chan->StdAttrs.ChannelCapability & SBI_MPXY_CHAN_CAP_SEND_MSG_WITH_RESP) == 0) {
        return EFI_UNSUPPORTED;
      }
      Status = SbiMpxySendMessageWithResponse (Chan->ChannelId,
                                               Msg->ServiceId,
                                               Msg->TxBuf,
                                               Msg->TxLen,
                                               Msg->RxBuf,
                                               Msg->RxBufLen,
                                               Msg->RxLen);
      if (Status != EFI_SUCCESS) {
        return Status;
      }
      break;
    case RpmiMsgTypePostedRequest:
      if ((Chan->StdAttrs.ChannelCapability & SBI_MPXY_CHAN_CAP_SEND_MSG_WITHOUT_RESP) == 0) {
        return EFI_UNSUPPORTED;
      }
      Status = SbiMpxySendMessageWithoutResponse (Chan->ChannelId,
                                                  Msg->ServiceId,
                                                  Msg->TxBuf,
                                                  Msg->TxLen);
      if (Status != EFI_SUCCESS) {
        return Status;
      }
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Constructor to do some initialization for MPXY RPMI interfaces.

  @param  ImageHandle   The image handle.
  @param  SystemTable   The system table.

  @retval EFI_SUCCESS   Init successfully.
  @retval Other         Return error status.

**/
EFI_STATUS
EFIAPI
RiscVSbiMpxyRpmiLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  InitializeListHead (&mMpxyRpmiChanList);

  return EFI_SUCCESS;
}

/**
  Destructor to free resources.

  @param  ImageHandle   The image handle.
  @param  SystemTable   The system table.

  @retval EFI_SUCCESS   Always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
RiscVSbiMpxyRpmiLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  FreeAllMpxyRpmiChannels ();

  return EFI_SUCCESS;
}
