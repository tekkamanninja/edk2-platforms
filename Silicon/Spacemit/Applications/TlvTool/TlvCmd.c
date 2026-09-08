/********************************************************************************
Copyright (C) 2025 Spacemit Ltd.

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
#include <Protocol/TlvInfo.h>

#define MAX_BUFFER_SIZE  256

CONST CHAR16  gShellTlvFileName[] = L"ShellCommand";
EFI_HANDLE    gShellTlvHiiHandle  = NULL;

STATIC CONST SHELL_PARAM_ITEM  ParamList[] = {
  { L"show",  TypeFlag  },
  { L"get",   TypeValue },
  { L"set",   TypeValue },
  { L"flush", TypeFlag  },
  { L"clear", TypeFlag  },
  { L"help",  TypeFlag  },
  { NULL,     TypeMax   }
};

STATIC VOID
PrintUsage (
  VOID
  );

// TLV Type Code Names
typedef struct {
  UINT8           Code;
  CONST CHAR16    *Name;
} TLV_CODE_DESC;

STATIC CONST TLV_CODE_DESC  mTlvCodeDescriptions[] = {
  { 0x21, L"Product Name"       },
  { 0x22, L"Part Number"        },
  { 0x23, L"Serial Number"      },
  { 0x24, L"MAC Base Address"   },
  { 0x25, L"Manufacture Date"   },
  { 0x26, L"Device Version"     },
  { 0x27, L"Label Revision"     },
  { 0x28, L"Platform Name"      },
  { 0x29, L"ONIE Version"       },
  { 0x2A, L"MAC Number"         },
  { 0x2B, L"Manufacturer"       },
  { 0x2C, L"Country Code"       },
  { 0x2D, L"Vendor Name"        },
  { 0x2E, L"Diag Version"       },
  { 0x2F, L"Service Tag"        },
  { 0x40, L"SDK Version"        },
  { 0x41, L"DDR CS Number"      },
  { 0x42, L"DDR Type"           },
  { 0x43, L"DDR Datarate"       },
  { 0x44, L"DDR TX ODT"         },
  { 0x60, L"WiFi MAC Address"   },
  { 0x61, L"Bluetooth Address"  },
  { 0x83, L"Second Boot Device" },
  { 0xFE, L"CRC-32"             },
};

/**
  Return the file name of the help text file if not using HII.

  @return The string pointer to the file name.
**/
CONST CHAR16 *EFIAPI
ShellCommandGetManFileNameTlv (
  VOID
  )
{
  return gShellTlvFileName;
}

STATIC CONST CHAR16 *
GetTlvCodeName (
  IN UINT8  Code
  )
{
  UINTN  i;

  for (i = 0; i < ARRAY_SIZE (mTlvCodeDescriptions); i++) {
    if (mTlvCodeDescriptions[i].Code == Code) {
      return mTlvCodeDescriptions[i].Name;
    }
  }

  return L"Unknown";
}

SHELL_STATUS
EFIAPI
ShellCommandRunTlv (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  LIST_ENTRY                  *CheckPackage;
  CHAR16                      *ProblemParam;
  SPACEMIT_TLV_INFO_PROTOCOL  *TlvProtocol;
  CONST CHAR16                *CodeStr;
  CONST CHAR16                *ValueStr;
  UINT8                       TlvCode;
  CHAR8                       Buffer[MAX_BUFFER_SIZE];

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

  // Locate TLV Info Protocol
  Status = gBS->LocateProtocol (&gSpacemitTlvInfoProtocolGuid, NULL, (VOID **)&TlvProtocol);
  if (EFI_ERROR (Status)) {
    Print (L"TLV Info Protocol not found. Make sure EEPROM driver is loaded.\n");
    return SHELL_NOT_FOUND;
  }

  // Handle 'show' command
  if (ShellCommandLineGetFlag (CheckPackage, L"show")) {
    Status = TlvProtocol->ShowTlvInfo (TlvProtocol);
    if (EFI_ERROR (Status)) {
      Print (L"Failed to show TLV info - %r\n", Status);
      return SHELL_DEVICE_ERROR;
    }

    return SHELL_SUCCESS;
  }

  // Handle 'get' command
  if ((CodeStr = ShellCommandLineGetValue (CheckPackage, L"get")) != NULL) {
    UINTN  i;

    TlvCode = (UINT8)ShellHexStrToUintn (CodeStr);

    Status = TlvProtocol->GetTlvInfo (TlvProtocol, TlvCode, Buffer, sizeof (Buffer));
    if (EFI_ERROR (Status)) {
      Print (
             L"Failed to get TLV 0x%02x (%s) - %r\n",
             TlvCode,
             GetTlvCodeName (TlvCode),
             Status
             );
      return SHELL_DEVICE_ERROR;
    }

    Print (L"TLV 0x%02x (%s): ", TlvCode, GetTlvCodeName (TlvCode));

    // Format based on TLV type
    if ((TlvCode == 0x24) || (TlvCode == 0x60) || (TlvCode == 0x61)) {
      // MAC/Bluetooth addresses - 6 bytes hex
      for (i = 0; i < 6; i++) {
        Print (L"%02X", (UINT8)Buffer[i]);
        if (i < 5) {
          Print (L":");
        }
      }

      Print (L"\n");
    } else if (TlvCode == 0x2A) {
      // MAC Number - single byte
      Print (L"%d\n", (UINT8)Buffer[0]);
    } else if (TlvCode == 0xFE) {
      // CRC-32 - 4 bytes hex
      Print (
             L"0x%02X%02X%02X%02X\n",
             (UINT8)Buffer[0],
             (UINT8)Buffer[1],
             (UINT8)Buffer[2],
             (UINT8)Buffer[3]
             );
    } else {
      // Default: ASCII string
      Print (L"%a\n", Buffer);
    }

    return SHELL_SUCCESS;
  }

  // Handle 'set' command
  if ((CodeStr = ShellCommandLineGetValue (CheckPackage, L"set")) != NULL) {
    ValueStr = ShellCommandLineGetRawValue (CheckPackage, 1);
    if (ValueStr == NULL) {
      Print (L"Error: 'set' requires both code and value\n");
      PrintUsage ();
      return SHELL_INVALID_PARAMETER;
    }

    TlvCode = (UINT8)ShellHexStrToUintn (CodeStr);

    // Convert CHAR16 to CHAR8
    UnicodeStrToAsciiStrS (ValueStr, Buffer, sizeof (Buffer));

    Status = TlvProtocol->SetTlvInfo (TlvProtocol, TlvCode, Buffer);
    if (EFI_ERROR (Status)) {
      Print (
             L"Failed to set TLV 0x%02x (%s) - %r\n",
             TlvCode,
             GetTlvCodeName (TlvCode),
             Status
             );
      return SHELL_DEVICE_ERROR;
    }

    Print (L"TLV 0x%02x (%s) set to: %a\n", TlvCode, GetTlvCodeName (TlvCode), Buffer);
    Print (L"Note: Use 'tlvtool flush' to save changes to EEPROM\n");
    return SHELL_SUCCESS;
  }

  // Handle 'flush' command
  if (ShellCommandLineGetFlag (CheckPackage, L"flush")) {
    Status = TlvProtocol->FlushTlvInfo (TlvProtocol);
    if (EFI_ERROR (Status)) {
      Print (L"Failed to flush TLV info to EEPROM - %r\n", Status);
      return SHELL_DEVICE_ERROR;
    }

    Print (L"TLV info successfully saved to EEPROM\n");
    return SHELL_SUCCESS;
  }

  // Handle 'clear' command
  if (ShellCommandLineGetFlag (CheckPackage, L"clear")) {
    Status = TlvProtocol->ClearTlvInfo (TlvProtocol);
    if (EFI_ERROR (Status)) {
      Print (L"Failed to clear TLV info - %r\n", Status);
      return SHELL_DEVICE_ERROR;
    }

    Print (L"TLV info cleared. Use 'tlvtool flush' to save changes.\n");
    return SHELL_SUCCESS;
  }

  // No valid command specified
  PrintUsage ();
  return SHELL_INVALID_PARAMETER;
}

/**
  Function for 'tlvtool' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunTlvMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return ShellCommandRunTlv (ImageHandle, SystemTable);
}

/**
  This is the shell command handler function pointer callback type.  This
  function handles the command when it is invoked in the shell.

  @param[in] This                   The instance of the EFI_SHELL_PROTOCOL.
  @param[in] SystemTable            The pointer to the system table.
  @param[in] ShellParameters        The parameters associated with the command.
  @param[in] Shell                  The instance of the shell protocol used in the
                                     context of processing this command.

  @return EFI_SUCCESS               The operation was successful.
  @return other                     The operation failed.
**/
EFI_STATUS
EFIAPI
ShellTlvLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gShellTlvHiiHandle = HiiAddPackages (
                                       &gShellTlvToolHiiGuid,
                                       ImageHandle,
                                       UefiShellTlvLibStrings,
                                       NULL
                                       );
  if (gShellTlvHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  // Install our Shell command handler
  ShellCommandRegisterCommandName (
                                   L"tlvtool",
                                   ShellCommandRunTlvMain,
                                   ShellCommandGetManFileNameTlv,
                                   0,
                                   L"tlvtool",
                                   TRUE,
                                   gShellTlvHiiHandle,
                                   STRING_TOKEN (STR_GET_HELP_TLVTOOL)
                                   );

  return EFI_SUCCESS;
}

/**
  Destructor for the library.  free any resources.

  @param ImageHandle            The image handle of the process.
  @param SystemTable            The EFI System Table pointer.
**/
EFI_STATUS
EFIAPI
ShellTlvLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gShellTlvHiiHandle != NULL) {
    HiiRemovePackages (gShellTlvHiiHandle);
  }

  return EFI_SUCCESS;
}

STATIC
VOID
PrintUsage (
  VOID
  )
{
  Print (L"TLV EEPROM Tool Usage:\n");
  Print (L"  tlvtool show                    - Display all TLV information\n");
  Print (L"  tlvtool get <code>              - Get TLV value by code\n");
  Print (L"  tlvtool set <code> <value>      - Set TLV value (not saved until flush)\n");
  Print (L"  tlvtool flush                   - Save changes to EEPROM\n");
  Print (L"  tlvtool clear                   - Clear all TLV data\n");
  Print (L"\n");
  Print (L"Common TLV Codes:\n");
  Print (L"  0x21 - Product Name      0x22 - Part Number\n");
  Print (L"  0x23 - Serial Number     0x24 - MAC Base Address\n");
  Print (L"  0x25 - Manufacture Date  0x26 - Device Version\n");
  Print (L"  0x28 - Platform Name     0x2A - MAC Number\n");
  Print (L"  0x2B - Manufacturer      0x2C - Country Code\n");
  Print (L"  0x60 - WiFi MAC Address  0x61 - Bluetooth Address\n");
  Print (L"\n");
  Print (L"Examples:\n");
  Print (L"  tlvtool show                          # Display all TLV info\n");
  Print (L"  tlvtool get 0x23                      # Get serial number\n");
  Print (L"  tlvtool set 0x21 \"MUSE-Pico\"        # Set product name\n");
  Print (L"  tlvtool set 0x23 \"SN12345678\"       # Set serial number\n");
  Print (L"  tlvtool flush                         # Save to EEPROM\n");
}
