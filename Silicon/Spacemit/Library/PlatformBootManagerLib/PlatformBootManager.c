/** @file
  PlatformBootManagerLib implementation.

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <IndustryStandard/Pci.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BootLogoLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Guid/SerialPortLibVendor.h>
#include <Guid/TtyTerm.h>

#define DP_NODE_LEN(Type)  { (UINT8)sizeof (Type), (UINT8)(sizeof (Type) >> 8) }

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH       SerialDxe;
  UART_DEVICE_PATH         Uart;
  VENDOR_DEVICE_PATH       TerminalType;
  EFI_DEVICE_PATH_PROTOCOL End;
} SERIAL_CONSOLE_DEVICE_PATH;
#pragma pack ()

STATIC SERIAL_CONSOLE_DEVICE_PATH mSerialConsoleDevicePath = {
  .SerialDxe = {
    .Header = { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, DP_NODE_LEN (VENDOR_DEVICE_PATH) },
    .Guid = EDKII_SERIAL_PORT_LIB_VENDOR_GUID,
  },
  .Uart = {
    .Header = { MESSAGING_DEVICE_PATH, MSG_UART_DP, DP_NODE_LEN (UART_DEVICE_PATH) },
    .Reserved = 0,
    // TODO: use FixedPcdGet?
    .BaudRate = 115200,
    .DataBits = 8,
    .Parity = 1,
    .StopBits = 1,
  },
  .TerminalType = {
    .Header = { MESSAGING_DEVICE_PATH, MSG_VENDOR_DP, DP_NODE_LEN (VENDOR_DEVICE_PATH) },
    .Guid = EFI_TTY_TERM_GUID,  // TODO: fill in this Guid dynamically?
  },
  .End = { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, DP_NODE_LEN (EFI_DEVICE_PATH_PROTOCOL) },
};

#pragma pack(1)
typedef struct {
  USB_CLASS_DEVICE_PATH Keyboard;
  EFI_DEVICE_PATH_PROTOCOL End;
} PLATFORM_USB_KEYBOARD;
#pragma pack()

STATIC PLATFORM_USB_KEYBOARD mUsbKeyboard = {
  //
  // USB_CLASS_DEVICE_PATH Keyboard
  //
  .Keyboard = {
    .Header = { MESSAGING_DEVICE_PATH, MSG_USB_CLASS_DP, DP_NODE_LEN(USB_CLASS_DEVICE_PATH) },
    .VendorId = 0xFFFF,
    .ProductId = 0xFFFF,
    .DeviceClass = 3,
    .DeviceSubClass = 1,
    .DeviceProtocol = 1,
  },

  //
  // EFI_DEVICE_PATH_PROTOCOL End
  //
  .End = { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, DP_NODE_LEN(EFI_DEVICE_PATH_PROTOCOL) },
};

/**
  Check if the handle satisfies a particular condition.

  @param[in] Handle      The handle to check.
  @param[in] ReportText  A caller-allocated string passed in for reporting
                         purposes. It must never be NULL.

  @retval TRUE   The condition is satisfied.
  @retval FALSE  Otherwise. This includes the case when the condition could not
                 be fully evaluated due to an error.
**/
typedef
BOOLEAN
(EFIAPI *FILTER_FUNCTION)(
  IN EFI_HANDLE   Handle,
  IN CONST CHAR16 *ReportText
  );

/**
  Process a handle.

  @param[in] Handle      The handle to process.
  @param[in] ReportText  A caller-allocated string passed in for reporting
                         purposes. It must never be NULL.
**/
typedef
VOID
(EFIAPI *CALLBACK_FUNCTION)(
  IN EFI_HANDLE   Handle,
  IN CONST CHAR16 *ReportText
  );

/**
  Locate all handles that carry the specified protocol, filter them with a
  callback function, and pass each handle that passes the filter to another
  callback.

  @param[in] ProtocolGuid  The protocol to look for.

  @param[in] Filter        The filter function to pass each handle to. If this
                           parameter is NULL, then all handles are processed.

  @param[in] Process       The callback function to pass each handle to that
                           clears the filter.
**/
STATIC
VOID
FilterAndProcess (
  IN EFI_GUID           *ProtocolGuid,
  IN FILTER_FUNCTION    Filter         OPTIONAL,
  IN CALLBACK_FUNCTION  Process
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       NoHandles;
  UINTN       Idx;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  ProtocolGuid,
                  NULL /* SearchKey */,
                  &NoHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    //
    // This is not an error, just an informative condition.
    //
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: %g: %r\n",
      __func__,
      ProtocolGuid,
      Status
      ));
    return;
  }

  ASSERT (NoHandles > 0);
  for (Idx = 0; Idx < NoHandles; ++Idx) {
    CHAR16         *DevicePathText;
    STATIC CHAR16  Fallback[] = L"<device path unavailable>";

    //
    // The ConvertDevicePathToText() function handles NULL input transparently.
    //
    DevicePathText = ConvertDevicePathToText (
                       DevicePathFromHandle (Handles[Idx]),
                       FALSE, // DisplayOnly
                       FALSE  // AllowShortcuts
                       );
    if (DevicePathText == NULL) {
      DevicePathText = Fallback;
    }

    if ((Filter == NULL) || Filter (Handles[Idx], DevicePathText)) {
      Process (Handles[Idx], DevicePathText);
    }

    if (DevicePathText != Fallback) {
      FreePool (DevicePathText);
    }
  }

  gBS->FreePool (Handles);
}

/**
  This FILTER_FUNCTION checks if a handle corresponds to a PCI display device.
**/
STATIC
BOOLEAN
EFIAPI
IsPciDisplay (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  PCI_TYPE00           Pci;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo
                  );
  if (EFI_ERROR (Status)) {
    //
    // This is not an error worth reporting.
    //
    return FALSE;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0 /* Offset */,
                        sizeof Pci / sizeof (UINT32),
                        &Pci
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s: %r\n", __func__, ReportText, Status));
    return FALSE;
  }

  return IS_PCI_DISPLAY (&Pci);
}

/**
  This CALLBACK_FUNCTION attempts to connect a handle non-recursively, asking
  the matching driver to produce all first-level child handles.
**/
STATIC
VOID
EFIAPI
Connect (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS  Status;

  Status = gBS->ConnectController (
                  Handle, // ControllerHandle
                  NULL,   // DriverImageHandle
                  NULL,   // RemainingDevicePath -- produce all children
                  FALSE   // Recursive
                  );
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_VERBOSE,
    "%a: %s: %r\n",
    __func__,
    ReportText,
    Status
    ));
}

/**
  This CALLBACK_FUNCTION retrieves the EFI_DEVICE_PATH_PROTOCOL from the
  handle, and adds it to ConOut and ErrOut.
**/
STATIC
VOID
EFIAPI
AddOutput (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  DevicePath = DevicePathFromHandle (Handle);
  if (DevicePath == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s: handle %p: device path not found\n",
      __func__,
      ReportText,
      Handle
      ));
    return;
  }

  Status = EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s: adding to ConOut: %r\n",
      __func__,
      ReportText,
      Status
      ));
    return;
  }

  Status = EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s: adding to ErrOut: %r\n",
      __func__,
      ReportText,
      Status
      ));
    return;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: %s: added to ConOut and ErrOut\n",
    __func__,
    ReportText
    ));
}

STATIC
VOID
PlatformRegisterOptionsAndKeys (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_INPUT_KEY                 Enter;
  EFI_INPUT_KEY                 F2;
  EFI_INPUT_KEY                 Esc;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption;

  //
  // Register ENTER as CONTINUE key
  //
  Enter.ScanCode    = SCAN_NULL;
  Enter.UnicodeChar = CHAR_CARRIAGE_RETURN;
  Status            = EfiBootManagerRegisterContinueKeyOption (0, &Enter, NULL);
  ASSERT_EFI_ERROR (Status);

  //
  // Map F2 to enter Boot Manager Menu
  //
  F2.ScanCode     = SCAN_F2;
  F2.UnicodeChar  = CHAR_NULL;
  Status          = EfiBootManagerGetBootManagerMenu (&BootOption);
  ASSERT_EFI_ERROR (Status);
  Status = EfiBootManagerAddKeyOptionVariable (
             NULL,
             (UINT16)BootOption.OptionNumber,
             0,
             &F2,
             NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);

  //
  // Map ESC to enter Boot Manager Menu
  //
  if (FeaturePcdGet (PcdEscEnterBootMenu)) {
    Esc.ScanCode    = SCAN_ESC;
    Esc.UnicodeChar = CHAR_NULL;
    Status = EfiBootManagerAddKeyOptionVariable (
               NULL,
               (UINT16)BootOption.OptionNumber,
               0,
               &Esc,
               NULL
               );
    ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
  }
}

/**
  Register a boot option using a file GUID in the FV.

  @param FileGuid     The file GUID name in FV.
  @param Description  The boot option description.
  @param Attributes   The attributes used for the boot option loading.
**/
STATIC
VOID
PlatformRegisterFvBootOption (
  EFI_GUID  *FileGuid,
  CHAR16    *Description,
  UINT32    Attributes
  )
{
  EFI_STATUS                         Status;
  INTN                               OptionIndex;
  EFI_BOOT_MANAGER_LOAD_OPTION       NewOption;
  EFI_BOOT_MANAGER_LOAD_OPTION       *BootOptions;
  UINTN                              BootOptionCount;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  ASSERT_EFI_ERROR (Status);

  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  ASSERT (DevicePath != NULL);
  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  ASSERT (DevicePath != NULL);

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             Attributes,
             Description,
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);
  FreePool (DevicePath);

  BootOptions = EfiBootManagerGetLoadOptions (
                  &BootOptionCount,
                  LoadOptionTypeBoot
                  );

  OptionIndex = EfiBootManagerFindLoadOption (
                  &NewOption,
                  BootOptions,
                  BootOptionCount
                  );

  if (OptionIndex == -1) {
    Status = EfiBootManagerAddLoadOptionVariable (&NewOption, MAX_UINTN);
    ASSERT_EFI_ERROR (Status);
  }

  EfiBootManagerFreeLoadOption (&NewOption);
  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

/**
  Do the platform specific action before the console is connected.

  Such as:
    Update console variable;
    Register new Driver#### or Boot####;
    Signal ReadyToLock event.
**/
VOID
EFIAPI
PlatformBootManagerBeforeConsole (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "PlatformBootManagerBeforeConsole\n"));

  //
  // Signal EndOfDxe PI Event
  //
  EfiEventGroupSignal (&gEfiEndOfDxeEventGroupGuid);

  // TODO:
  //  ESRT table?

  //
  // Locate the PCI root bridges and make the PCI bus driver connect each,
  // non-recursively. This will produce a number of child handles with PciIo on
  // them.
  //
  FilterAndProcess (&gEfiPciRootBridgeIoProtocolGuid, NULL, Connect);

  //
  // Find all display class PCI devices (using the handles from the previous
  // step), and connect them non-recursively. This should produce a number of
  // child handles with GOPs on them.
  //
  FilterAndProcess (&gEfiPciIoProtocolGuid, IsPciDisplay, Connect);

  //
  // Now add the device path of all handles with GOP on them to ConOut and
  // ErrOut.
  //
  FilterAndProcess (&gEfiGraphicsOutputProtocolGuid, NULL, AddOutput);

  //
  // Add the hardcoded short-form USB keyboard device path to ConIn.
  //
  EfiBootManagerUpdateConsoleVariable(ConIn, (EFI_DEVICE_PATH_PROTOCOL *)&mUsbKeyboard, NULL);

  //
  // Add the hardcoded serial console device path to ConIn, ConOut, ErrOut.
  //
  EfiBootManagerUpdateConsoleVariable(ConIn, (EFI_DEVICE_PATH_PROTOCOL *)&mSerialConsoleDevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable(ConOut, (EFI_DEVICE_PATH_PROTOCOL *)&mSerialConsoleDevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable(ErrOut, (EFI_DEVICE_PATH_PROTOCOL *)&mSerialConsoleDevicePath, NULL);

  //
  // Register platform-specific boot options and keyboard shortcuts.
  //
  PlatformRegisterOptionsAndKeys ();
}

/**
  Do the platform specific action after the console is connected.

  Such as:
    Dynamically switch output mode;
    Signal console ready platform customized event;
    Run diagnostics like memory testing;
    Connect certain devices;
    Dispatch aditional option roms.
**/
VOID
EFIAPI
PlatformBootManagerAfterConsole (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "PlatformBootManagerAfterConsole\n"));

  //
  // Show the splash screen.
  //
  BootLogoEnableLogo ();

  //
  // Connect the rest of the devices.
  //
  EfiBootManagerConnectAll ();

  //
  // Enumerate all possible boot options.
  //
  EfiBootManagerRefreshAllBootOption ();

  //
  // Register UEFI Shell
  //
  PlatformRegisterFvBootOption (&gUefiShellFileGuid, L"UEFI Shell", LOAD_OPTION_ACTIVE);

  //
  // Signal After Console Event
  //
  EfiEventGroupSignal (&gSpacemitBdsAfterConsoleEventGroupGuid);
}

STATIC
VOID
PrintAtBottomRow (
  IN CHAR16  *Str
  )
{
  UINTN  Columns;
  UINTN  Rows;

  if (gST->ConOut == NULL) {
    return;
  }

  if (!EFI_ERROR (gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows))) {
    gST->ConOut->SetCursorPosition (gST->ConOut, 0, Rows - 1);
  }

  Print (Str);
}

/**
  This function is called each second during the boot manager waits the timeout.

  @param TimeoutRemain  The remaining timeout.
**/
VOID
EFIAPI
PlatformBootManagerWaitCallback (
  UINT16  TimeoutRemain
  )
{
  CHAR16  Buf[80];

  UnicodeSPrint (Buf, sizeof (Buf), L"\r  Boot in %2d second(s), press F2 to enter Boot Menu ... ", TimeoutRemain);
  PrintAtBottomRow (Buf);
}

/**
  The function is called when no boot option could be launched,
  including platform recovery options and options pointing to applications
  built into firmware volumes.

  If this function returns, BDS attempts to enter an infinite loop.
**/
VOID
EFIAPI
PlatformBootManagerUnableToBoot (
  VOID
  )
{
  // TODO
  return;
}
