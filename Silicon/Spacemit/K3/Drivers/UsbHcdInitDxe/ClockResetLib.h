/** @file

  Copyright 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_CLOCKRESETLIB_H_
#define USB_CLOCKRESETLIB_H_

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Protocol/ClockCtrl.h>

SILICON_CLOCKCTRL_PROTOCOL  *mClockCtrlProtocol;
BOOLEAN                     mClockCtrlInitialized = FALSE;

STATIC
EFI_STATUS
InitializeClockCtrlProtocol (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mClockCtrlInitialized) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID *)&mClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
    return Status;
  }

  mClockCtrlInitialized = TRUE;

  return EFI_SUCCESS;
}

EFI_STATUS
ClockEnableAndResetDeassert (
  CHAR8  *Name
  )
{
  EFI_STATUS  Status;

  // Initialize Clock protocol
  Status = InitializeClockCtrlProtocol ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mClockCtrlProtocol->SetClockState (
                                     mClockCtrlProtocol,
                                     Name,
                                     ENABLE_CLOCK
                                     );
  return EFI_SUCCESS;
}

EFI_STATUS
ClockDisableAndResetAssert (
  CHAR8  *Name
  )
{
  EFI_STATUS  Status;

  // Initialize Clock protocol
  Status = InitializeClockCtrlProtocol ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mClockCtrlProtocol->SetClockState (
                                     mClockCtrlProtocol,
                                     Name,
                                     DISABLE_CLOCK
                                     );
  return EFI_SUCCESS;
}

#endif // USB_CLOCKRESETLIB_H_
