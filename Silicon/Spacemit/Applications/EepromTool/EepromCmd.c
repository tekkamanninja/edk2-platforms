/********************************************************************************
Copyright (C) 2024 Spacemit Ltd.

SPDX-License-Identifier: BSD-2-Clause-Patent

*******************************************************************************/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/HiiLib.h>
#include <Library/FileHandleLib.h>
#include <Protocol/I2cIo.h>

#include "../Drivers/I2CMaster/Eeprom/Eeprom.h"

#define MAX_BUFFER_SIZE  256

CONST CHAR16  gShellEepromFileName[] = L"ShellCommand";
EFI_HANDLE    gShellEepromHiiHandle  = NULL;

STATIC CONST SHELL_PARAM_ITEM  ParamList[] = {
  { L"read",  TypeFlag },
  { L"write", TypeFlag },
  { L"list",  TypeFlag },
  { L"help",  TypeFlag },
  { NULL,     TypeMax  }
};

STATIC VOID
PrintUsage (
  VOID
  );

/**
  Return the file name of the help text file if not using HII.

  @return The string pointer to the file name.
**/
CONST CHAR16 *
EFIAPI
ShellCommandGetManFileNameEeprom (
  VOID
  )
{
  return gShellEepromFileName;
}

SHELL_STATUS
EFIAPI
ShellCommandRunEeprom (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  LIST_ENTRY                *CheckPackage;
  CHAR16                    *ProblemParam;
  SPACEMIT_EEPROM_PROTOCOL  *EepromProtocol;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINT8                     Buffer[MAX_BUFFER_SIZE];
  UINTN                     DeviceNum;
  UINTN                     Address = 0;
  UINTN                     Length  = 0;
  UINTN                     i;
  BOOLEAN                   IsRead;
  CONST CHAR16              *DeviceStr  = NULL;
  CONST CHAR16              *AddressStr = NULL;
  CONST CHAR16              *LengthStr  = NULL;
  CONST CHAR16              *DataStr    = NULL;

  // Parse Shell command line
  Status = ShellCommandLineParse (ParamList, &CheckPackage, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    Print (L"Error while parsing command line\n");
    return SHELL_ABORTED;
  }

  if (ShellCommandLineGetFlag (CheckPackage, L"help")) {
    PrintUsage ();
    return SHELL_SUCCESS;
  }

  if (ShellCommandLineGetFlag (CheckPackage, L"list")) {
    Status = gBS->LocateHandleBuffer (
                                      ByProtocol,
                                      &gSpacemitEepromProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &HandleBuffer
                                      );
    if (EFI_ERROR (Status) || (HandleCount == 0)) {
      Print (L"No EEPROM device found\n");
      return SHELL_NOT_FOUND;
    }

    Print (L"Found %d EEPROM device(s):\n", HandleCount);
    for (i = 0; i < HandleCount; i++) {
      SPACEMIT_EEPROM_PROTOCOL  *TempProtocol;
      Status = gBS->HandleProtocol (
                                    HandleBuffer[i],
                                    &gSpacemitEepromProtocolGuid,
                                    (VOID **)&TempProtocol
                                    );
      if (EFI_ERROR (Status)) {
        Print (L"Error retrieving device %d\n", i);
      } else {
        Print (L" [%d] Identifier: 0x%04x, I2C Address: 0x%02x\n", i, TempProtocol->Identifier, I2C_DEVICE_ADDRESS (TempProtocol->Identifier));
      }
    }

    gBS->FreePool (HandleBuffer);
    return SHELL_SUCCESS;
  }

  if (ShellCommandLineGetFlag (CheckPackage, L"read")) {
    IsRead     = TRUE;
    DeviceStr  = ShellCommandLineGetRawValue (CheckPackage, 1);
    AddressStr = ShellCommandLineGetRawValue (CheckPackage, 2);
    LengthStr  = ShellCommandLineGetRawValue (CheckPackage, 3);
  } else if (ShellCommandLineGetFlag (CheckPackage, L"write")) {
    IsRead     = FALSE;
    DeviceStr  = ShellCommandLineGetRawValue (CheckPackage, 1);
    AddressStr = ShellCommandLineGetRawValue (CheckPackage, 2);
    DataStr    = ShellCommandLineGetRawValue (CheckPackage, 3);
  } else {
    PrintUsage ();
    return SHELL_INVALID_PARAMETER;
  }

  if (DeviceStr == NULL) {
    Print (L"Device parameter missing\n");
    return SHELL_INVALID_PARAMETER;
  }

  DeviceNum = ShellHexStrToUintn (DeviceStr);

  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gSpacemitEepromProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    Print (L"No EEPROM device found\n");
    return SHELL_NOT_FOUND;
  }

  if (DeviceNum >= HandleCount) {
    Print (L"Invalid device number %d. Only %d device(s) available.\n", DeviceNum, HandleCount);
    gBS->FreePool (HandleBuffer);
    return SHELL_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (
                                HandleBuffer[DeviceNum],
                                &gSpacemitEepromProtocolGuid,
                                (VOID **)&EepromProtocol
                                );
  if (EFI_ERROR (Status)) {
    Print (L"Failed to get EEPROM protocol for device %d\n", DeviceNum);
    gBS->FreePool (HandleBuffer);
    return SHELL_NOT_FOUND;
  }

  if (IsRead) {
    if (AddressStr == NULL) {
      Print (L"Address parameter missing\n");
      gBS->FreePool (HandleBuffer);
      return SHELL_INVALID_PARAMETER;
    }

    Address = ShellHexStrToUintn (AddressStr);
    if (LengthStr == NULL) {
      Print (L"Length parameter missing\n");
      gBS->FreePool (HandleBuffer);
      return SHELL_INVALID_PARAMETER;
    }

    Length = ShellHexStrToUintn (LengthStr);
    if (Length > MAX_BUFFER_SIZE) {
      Print (L"Maximum read length is %d bytes\n", MAX_BUFFER_SIZE);
      gBS->FreePool (HandleBuffer);
      return SHELL_INVALID_PARAMETER;
    }
  } else {
    if (AddressStr == NULL) {
      Print (L"Address parameter missing\n");
      gBS->FreePool (HandleBuffer);
      return SHELL_INVALID_PARAMETER;
    }

    Address = ShellHexStrToUintn (AddressStr);
    Length  = 0;
    while ((DataStr = ShellCommandLineGetRawValue (CheckPackage, Length + 3)) != NULL && Length < MAX_BUFFER_SIZE) {
      Buffer[Length] = (UINT8)ShellHexStrToUintn (DataStr);
      Length++;
    }
  }

  if (IsRead) {
    Status = EepromProtocol->Transfer (
                                       EepromProtocol,
                                       (UINT16)Address,
                                       (UINT32)Length,
                                       Buffer,
                                       I2C_FLAG_READ
                                       );
    if (EFI_ERROR (Status)) {
      Print (L"Read failed - %r\n", Status);
      gBS->FreePool (HandleBuffer);
      return SHELL_DEVICE_ERROR;
    }

    Print (L"Data at 0x%04x:\n", Address);
    for (i = 0; i < Length; i++) {
      if (i % 16 == 0) {
        Print (L"%04x: ", Address + i);
      }

      Print (L"%02x ", Buffer[i]);
      if (((i + 1) % 16 == 0) || (i == Length - 1)) {
        Print (L"\n");
      }
    }
  } else {
    Status = EepromProtocol->Transfer (
                                       EepromProtocol,
                                       (UINT16)Address,
                                       (UINT32)Length,
                                       Buffer,
                                       0
                                       );
    if (EFI_ERROR (Status)) {
      Print (L"Write failed - %r\n", Status);
      gBS->FreePool (HandleBuffer);
      return SHELL_DEVICE_ERROR;
    }

    Print (L"Successfully wrote %d bytes at address 0x%04x to device %d\n", Length, Address, DeviceNum);
  }

  gBS->FreePool (HandleBuffer);
  return SHELL_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellEepromLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gShellEepromHiiHandle = NULL;

  gShellEepromHiiHandle = HiiAddPackages (
                                          &gShellEepromHiiGuid,
                                          gImageHandle,
                                          UefiShellEepromLibStrings,
                                          NULL
                                          );

  if (gShellEepromHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  ShellCommandRegisterCommandName (
                                   L"eepromtool",
                                   ShellCommandRunEeprom,
                                   ShellCommandGetManFileNameEeprom,
                                   0,
                                   L"eepromtool",
                                   TRUE,
                                   gShellEepromHiiHandle,
                                   STRING_TOKEN (STR_GET_HELP_EEPROM)
                                   );

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellEepromLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gShellEepromHiiHandle != NULL) {
    HiiRemovePackages (gShellEepromHiiHandle);
  }

  return EFI_SUCCESS;
}

STATIC
VOID
PrintUsage (
  VOID
  )
{
  Print (L"EEPROM Tool Usage:\n");
  Print (L"  eepromtool list\n");
  Print (L"  eepromtool read  <device> <address> <length>\n");
  Print (L"  eepromtool write <device> <address> <data1> [data2] [data3] ...\n");
  Print (L"Example:\n");
  Print (L"  eepromtool list                # List all available EEPROM devices\n");
  Print (L"  eepromtool read 0 0x0 0xA       # Read 10 bytes from address 0 of device 0\n");
  Print (L"  eepromtool write 0 0x0 0x11 0x22 0x33   # Write bytes to address 0 of device 0\n");
}
