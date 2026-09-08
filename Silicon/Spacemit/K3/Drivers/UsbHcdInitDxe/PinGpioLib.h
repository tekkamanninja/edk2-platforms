/** @file

  Copyright 2025, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_GPIOLIB_H_
#define USB_GPIOLIB_H_

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <Protocol/PinCtrl.h>

STATIC EMBEDDED_GPIO  *mGpio           = NULL;
STATIC BOOLEAN        mGpioInitialized = FALSE;

STATIC SILICON_PINCTRL_PROTOCOL  *mPinctrl           = NULL;
STATIC BOOLEAN                   mPinctrlInitialized = FALSE;

/**
 * Initialize GPIO protocol
 *
 * @retval EFI_SUCCESS     Initialization successful
 * @retval Others          Initialization failed
 */
STATIC
EFI_STATUS
InitializeGpioProtocol (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *HandleBuffer;
  UINTN       HandleCount;
  UINTN       Index;

  if (mGpioInitialized) {
    return EFI_SUCCESS;
  }

  // Locate GPIO protocol
  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEmbeddedGpioProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "USB: Failed to locate GPIO protocol: %r\n", Status));
    return Status;
  }

  // Get GPIO protocol
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEmbeddedGpioProtocolGuid,
                                  (VOID **)&mGpio
                                  );
    if (!EFI_ERROR (Status)) {
      break;
    }
  }

  // Free handle buffer
  gBS->FreePool (HandleBuffer);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "USB: Failed to get GPIO protocol: %r\n", Status));
    return Status;
  }

  mGpioInitialized = TRUE;
  return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
GpioConfigureOutput (
  UINT32   GpioPin,
  BOOLEAN  Value
  )
{
  EFI_STATUS  Status;

  // Initialize GPIO protocol
  Status = InitializeGpioProtocol ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Initial state is off
  Status = mGpio->Set (
                       mGpio,
                       GpioPin,
                       Value ? GPIO_MODE_OUTPUT_1 : GPIO_MODE_OUTPUT_0
                       );
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "USB: Failed to set initial state for GPIO %d: %r\n",
            GpioPin, Status)
           );
    return Status;
  }

  DEBUG ((DEBUG_ERROR, "USB: Using Latest GPIO\n"));
  return EFI_SUCCESS;
}

/**
 * Initialize Pinctrl protocol
 *
 * @retval EFI_SUCCESS     Initialization successful
 * @retval Others          Initialization failed
 */
STATIC
EFI_STATUS
InitializePinctrlProtocol (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *HandleBuffer;
  UINTN       HandleCount;
  UINTN       Index;

  if (mPinctrlInitialized) {
    return EFI_SUCCESS;
  }

  // Locate Pinctrl protocol
  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gSpacemitSiliconPinCtrlProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "USB: Failed to locate Pinctrl protocol: %r\n", Status));
    return Status;
  }

  // Get Pinctrl protocol
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gSpacemitSiliconPinCtrlProtocolGuid,
                                  (VOID **)&mPinctrl
                                  );
    if (!EFI_ERROR (Status)) {
      break;
    }
  }

  // Free handle buffer
  gBS->FreePool (HandleBuffer);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "USB: Failed to get Pinctrl protocol: %r\n", Status));
    return Status;
  }

  mPinctrlInitialized = TRUE;
  return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
PinctrlApplyStateById (
  CONST CHAR8  *ControllerType,
  UINT32       ControllerId,
  CONST CHAR8  *FunctionName OPTIONAL,
  CONST CHAR8  *StateName OPTIONAL
  )
{
  EFI_STATUS  Status;

  if ((ControllerType == NULL) || (ControllerType[0] == '\0')) {
    return EFI_INVALID_PARAMETER;
  }

  Status = InitializePinctrlProtocol ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (mPinctrl->ApplyStateById == NULL) {
    return EFI_UNSUPPORTED;
  }

  return mPinctrl->ApplyStateById (
                    mPinctrl,
                    ControllerType,
                    ControllerId,
                    FunctionName,
                    StateName
                    );
}

#endif
