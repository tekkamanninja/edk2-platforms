/********************************************************************************
Copyright (C) 2023 Spacemit Ltd.

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
#include <Protocol/I2cEnumerate.h>

#include "../Drivers/I2CMaster/I2CMaster/I2cMaster.h"

#define SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE  SIGNATURE_32('I','2','C','T')

typedef struct {
  UINT32                     Signature;
  EFI_I2C_IO_PROTOCOL        I2cIo;
  EFI_I2C_MASTER_PROTOCOL    *I2cMaster;
  UINT8                      SlaveAddress;
} SPACEMIT_I2C_IO_PROTOCOL;

extern unsigned char  UefiShellI2cLibStrings[];

#define MAX_BUFFER_SIZE     256
#define DEFAULT_DUMP_COUNT  16

CONST CHAR16  gShellI2cFileName[] = L"ShellCommand";
EFI_HANDLE    gShellI2cHiiHandle  = NULL;

STATIC CONST SHELL_PARAM_ITEM  ParamList[] __attribute__ ((unused)) = {
  { L"list",   TypeFlag  },
  { L"detect", TypeValue },
  { L"dump",   TypeValue },
  { L"get",    TypeValue },
  { L"set",    TypeValue },
  { L"write",  TypeValue },
  { L"read",   TypeValue },
  { L"help",   TypeFlag  },
  { NULL,      TypeMax   }
};

/**
  Display usage information for the I2C tool.
**/
STATIC
VOID
PrintUsage (
  VOID
  )
{
  Print (L"I2C Tool - Provides commands for I2C bus and device operations.\n");
  Print (L"Usage:\n");
  Print (L"  i2ctool list\n");
  Print (L"  i2ctool detect <bus>\n");
  Print (L"  i2ctool dump <bus> <addr> [start_reg] [count]\n");
  Print (L"  i2ctool get <bus> <addr> <reg>\n");
  Print (L"  i2ctool set <bus> <addr> <reg> <value>\n");
  Print (L"  i2ctool write <bus> <addr> <reg> <byte1> [byte2] [byte3] ...\n");
  Print (L"  i2ctool read <bus> <addr> <reg> <count>\n");
}

EFI_STATUS
EFIAPI
CustomQueueRequest (
  IN CONST EFI_I2C_IO_PROTOCOL  *This,
  IN UINTN                      SlaveAddressIndex,
  IN EFI_EVENT                  Event      OPTIONAL,
  IN EFI_I2C_REQUEST_PACKET     *RequestPacket,
  OUT EFI_STATUS                *I2cStatus OPTIONAL
  )
{
  SPACEMIT_I2C_IO_PROTOCOL  *CustomI2cIo;

  if ((This == NULL) || (RequestPacket == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CustomI2cIo = CR (This, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);

  if (CustomI2cIo->I2cMaster != NULL) {
    return CustomI2cIo->I2cMaster->StartRequest (
                                                 CustomI2cIo->I2cMaster,
                                                 CustomI2cIo->SlaveAddress,
                                                 RequestPacket,
                                                 Event,
                                                 I2cStatus
                                                 );
  }

  return EFI_UNSUPPORTED;
}

/**
  Find an I2C device on a specific bus with a given device address.

  @param[in]  BusNum       The I2C bus number
  @param[in]  DevAddr      The I2C device address
  @param[out] I2cIo        Pointer to the I2C IO protocol interface

  @retval EFI_SUCCESS      Device found
  @retval EFI_NOT_FOUND    Device not found
**/
STATIC
EFI_STATUS
FindI2cDevice (
  IN  UINTN                BusNum,
  IN  UINT16               DevAddr,
  OUT EFI_I2C_IO_PROTOCOL  **I2cIo
  )
{
  EFI_STATUS                  Status;
  UINTN                       HandleCount;
  EFI_HANDLE                  *HandleBuffer;
  UINTN                       Index;
  EFI_I2C_ENUMERATE_PROTOCOL  *I2cEnumerate = NULL;
  EFI_I2C_MASTER_PROTOCOL     *I2cMaster    = NULL;
  I2C_MASTER_CONTEXT          *I2cContext;
  BOOLEAN                     BusFound = FALSE;
  SPACEMIT_I2C_IO_PROTOCOL    *CustomI2cIo;

  *I2cIo = NULL;

  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEfiI2cEnumerateProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    return EFI_NOT_FOUND;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEfiI2cEnumerateProtocolGuid,
                                  (VOID **)&I2cEnumerate
                                  );
    if (EFI_ERROR (Status)) {
      continue;
    }

    I2cContext = I2C_MASTER_CONTEXT_FROM_ENUMERATE (I2cEnumerate);

    if (I2cContext->ControllerId != BusNum) {
      continue;
    }

    BusFound = TRUE;

    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEfiI2cMasterProtocolGuid,
                                  (VOID **)&I2cMaster
                                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get I2C master protocol for bus %u: %r\n", BusNum, Status));
      FreePool (HandleBuffer);
      return EFI_DEVICE_ERROR;
    }

    break;
  }

  FreePool (HandleBuffer);

  if (!BusFound || (I2cMaster == NULL)) {
    return EFI_NOT_FOUND;
  }

  CustomI2cIo = AllocateZeroPool (sizeof (SPACEMIT_I2C_IO_PROTOCOL));
  if (CustomI2cIo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CustomI2cIo->Signature                       = SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE;
  CustomI2cIo->I2cIo.QueueRequest              = CustomQueueRequest;
  CustomI2cIo->I2cIo.DeviceGuid                = &gEfiI2cDeviceGuid;
  CustomI2cIo->I2cIo.DeviceIndex               = DevAddr;
  CustomI2cIo->I2cIo.HardwareRevision          = 0;
  CustomI2cIo->I2cIo.I2cControllerCapabilities = I2cMaster->I2cControllerCapabilities;
  CustomI2cIo->I2cMaster                       = I2cMaster;
  CustomI2cIo->SlaveAddress                    = (UINT8)DevAddr;

  *I2cIo = &CustomI2cIo->I2cIo;
  return EFI_SUCCESS;
}

/**
  List all available I2C buses.

  @retval SHELL_SUCCESS            Command completed successfully.
  @retval SHELL_NOT_FOUND          No I2C buses found.
**/
STATIC
SHELL_STATUS
ListI2cBuses (
  VOID
  )
{
  EFI_STATUS                  Status;
  UINTN                       HandleCount;
  EFI_HANDLE                  *HandleBuffer;
  UINTN                       Index;
  UINTN                       BusCount = 0;
  EFI_I2C_ENUMERATE_PROTOCOL  *I2cEnumerate;
  UINTN                       BusFrequency;
  I2C_MASTER_CONTEXT          *I2cContext;

  // Get all handles that support the I2C Enumerate Protocol
  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEfiI2cEnumerateProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status)) {
    Print (L"Error locating I2C Enumerate protocols: %r\n", Status);
    return SHELL_NOT_FOUND;
  }

  if (HandleCount == 0) {
    Print (L"No I2C buses found\n");
    return SHELL_NOT_FOUND;
  }

  Print (L"Available I2C buses:\n");

  // Iterate through all I2C controllers
  for (Index = 0; Index < HandleCount; Index++) {
    // Get the I2C Enumerate Protocol
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEfiI2cEnumerateProtocolGuid,
                                  (VOID **)&I2cEnumerate
                                  );
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Get bus frequency for this controller
    Status = I2cEnumerate->GetBusFrequency (I2cEnumerate, 0, &BusFrequency);
    if (EFI_ERROR (Status)) {
      BusFrequency = 0;
    }

    // Get the I2C context from the I2C Enumerate Protocol
    I2cContext = I2C_MASTER_CONTEXT_FROM_ENUMERATE (I2cEnumerate);

    // Use the actual controller ID from the context
    UINT8  ControllerId = I2cContext->ControllerId;

    // Display the bus information with the actual controller ID
    Print (
           L"  [%d] I2C Bus %d (%d KHz)\n",
           ControllerId,
           ControllerId,
           BusFrequency / 1000
           );
    BusCount++;
  }

  gBS->FreePool (HandleBuffer);

  if (BusCount == 0) {
    Print (L"No I2C buses found\n");
    return SHELL_NOT_FOUND;
  }

  return SHELL_SUCCESS;
}

STATIC
EFI_STATUS
I2cRead (
  IN  UINTN   Bus,
  IN  UINT16  Address,
  IN  UINT8   Register,
  OUT UINT8   *Data,
  IN  UINTN   Length
  )
{
  EFI_STATUS                Status;
  EFI_I2C_IO_PROTOCOL       *I2cIo;
  EFI_I2C_REQUEST_PACKET    *RequestPacket;
  UINT8                     *ReadBuffer;
  SPACEMIT_I2C_IO_PROTOCOL  *CustomI2cIo;

  Status = FindI2cDevice (Bus, Address, &I2cIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ReadBuffer = AllocateZeroPool (Length);
  if (ReadBuffer == NULL) {
    CustomI2cIo = CR (I2cIo, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);
    FreePool (CustomI2cIo);
    return EFI_OUT_OF_RESOURCES;
  }

  RequestPacket = AllocateZeroPool (sizeof (EFI_I2C_REQUEST_PACKET) + sizeof (EFI_I2C_OPERATION) * 2);
  if (RequestPacket == NULL) {
    FreePool (ReadBuffer);
    CustomI2cIo = CR (I2cIo, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);
    FreePool (CustomI2cIo);
    return EFI_OUT_OF_RESOURCES;
  }

  RequestPacket->OperationCount = 2;

  RequestPacket->Operation[0].Flags         = 0; // Write operation
  RequestPacket->Operation[0].LengthInBytes = 1;
  RequestPacket->Operation[0].Buffer        = &Register;

  RequestPacket->Operation[1].Flags         = I2C_FLAG_READ;
  RequestPacket->Operation[1].LengthInBytes = Length;
  RequestPacket->Operation[1].Buffer        = ReadBuffer;

  Status = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);

  if (!EFI_ERROR (Status)) {
    CopyMem (Data, ReadBuffer, Length);
  }

  FreePool (ReadBuffer);
  FreePool (RequestPacket);
  CustomI2cIo = CR (I2cIo, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);
  FreePool (CustomI2cIo);

  return Status;
}

STATIC
EFI_STATUS
I2cWrite (
  IN UINTN   Bus,
  IN UINT16  Address,
  IN UINT8   Register,
  IN UINT8   *Data,
  IN UINTN   Length
  )
{
  EFI_STATUS                Status;
  EFI_I2C_IO_PROTOCOL       *I2cIo;
  EFI_I2C_REQUEST_PACKET    *RequestPacket;
  UINT8                     *WriteBuffer;
  SPACEMIT_I2C_IO_PROTOCOL  *CustomI2cIo;

  Status = FindI2cDevice (Bus, Address, &I2cIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  WriteBuffer = AllocateZeroPool (Length + 1);
  if (WriteBuffer == NULL) {
    CustomI2cIo = CR (I2cIo, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);
    FreePool (CustomI2cIo);
    return EFI_OUT_OF_RESOURCES;
  }

  // Set write buffer
  WriteBuffer[0] = Register;               // Register address
  CopyMem (WriteBuffer + 1, Data, Length); // Copy data

  // Create request packet
  RequestPacket = AllocateZeroPool (sizeof (EFI_I2C_REQUEST_PACKET) + sizeof (EFI_I2C_OPERATION));
  if (RequestPacket == NULL) {
    FreePool (WriteBuffer);
    CustomI2cIo = CR (I2cIo, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);
    FreePool (CustomI2cIo);
    return EFI_OUT_OF_RESOURCES;
  }

  // Set request packet
  RequestPacket->OperationCount             = 1;
  RequestPacket->Operation[0].Flags         = 0;          // Write operation
  RequestPacket->Operation[0].LengthInBytes = Length + 1; // Including register address
  RequestPacket->Operation[0].Buffer        = WriteBuffer;

  // Execute write operation
  Status = I2cIo->QueueRequest (I2cIo, 0, NULL, RequestPacket, NULL);

  // Delay after write operation
  gBS->Stall (10000);  // 10ms delay

  FreePool (WriteBuffer);
  FreePool (RequestPacket);
  CustomI2cIo = CR (I2cIo, SPACEMIT_I2C_IO_PROTOCOL, I2cIo, SPACEMIT_I2C_IO_PROTOCOL_SIGNATURE);
  FreePool (CustomI2cIo);

  return Status;
}

/**
  Parse bus number from string, handling both decimal and hex formats.

  @param[in] BusNumStr    The bus number string
  @param[out] BusNum      The parsed bus number

  @retval TRUE   Parsing successful
  @retval FALSE  Invalid format
**/
STATIC
BOOLEAN
ParseBusNumber (
  IN  CONST CHAR16  *BusNumStr,
  OUT UINTN         *BusNum
  )
{
  if (BusNumStr == NULL) {
    return FALSE;
  }

  if ((StrnCmp (BusNumStr, L"0x", 2) == 0) || (StrnCmp (BusNumStr, L"0X", 2) == 0)) {
    *BusNum = ShellHexStrToUintn (BusNumStr);
  } else {
    *BusNum = ShellStrToUintn (BusNumStr);
  }

  return TRUE;
}

/**
  Detect I2C devices on a specific bus by sending probe commands.

  @param[in] BusNum                The I2C bus number to scan

  @retval SHELL_SUCCESS            Command completed successfully.
  @retval SHELL_INVALID_PARAMETER  Invalid bus number.
  @retval SHELL_NOT_FOUND          No I2C devices found.
**/
STATIC
SHELL_STATUS
DetectI2cDevices (
  IN UINTN  BusNum
  )
{
  EFI_STATUS                  Status;
  EFI_I2C_ENUMERATE_PROTOCOL  *I2cEnumerate = NULL;
  EFI_I2C_MASTER_PROTOCOL     *I2cMaster    = NULL;
  UINTN                       HandleCount;
  EFI_HANDLE                  *HandleBuffer;
  UINTN                       Index;
  BOOLEAN                     DevicesFound = FALSE;
  UINTN                       Address;
  EFI_I2C_REQUEST_PACKET      *Request;
  EFI_STATUS                  I2cStatus;
  I2C_MASTER_CONTEXT          *I2cContext  = NULL;
  BOOLEAN                     BusFound     = FALSE;
  UINTN                       BusFrequency = 0;

  // Find I2C enumeration protocol
  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEfiI2cEnumerateProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    Print (L"No I2C enumeration protocol found\n");
    return SHELL_NOT_FOUND;
  }

  // Find I2C master for the specified bus number
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEfiI2cEnumerateProtocolGuid,
                                  (VOID **)&I2cEnumerate
                                  );
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Get the I2C context from the I2C Enumerate Protocol
    I2cContext = I2C_MASTER_CONTEXT_FROM_ENUMERATE (I2cEnumerate);

    // Check if this is the bus we're looking for
    if (I2cContext->ControllerId != BusNum) {
      continue;
    }

    BusFound = TRUE;

    // Get I2C master protocol
    Status = gBS->HandleProtocol (
                                  HandleBuffer[Index],
                                  &gEfiI2cMasterProtocolGuid,
                                  (VOID **)&I2cMaster
                                  );
    if (EFI_ERROR (Status)) {
      Print (L"Failed to get I2C master protocol for bus %u\n", BusNum);
      FreePool (HandleBuffer);
      return SHELL_DEVICE_ERROR;
    }

    // Get bus frequency
    Status = I2cEnumerate->GetBusFrequency (I2cEnumerate, 0, &BusFrequency);
    if (EFI_ERROR (Status)) {
      BusFrequency = 0;
    }

    // Try to reset the bus before scanning
    Status = I2cMaster->Reset (I2cMaster);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "I2C: Bus reset failed: %r\n", Status));
      // Continue anyway
    }

    // Scan all possible I2C addresses (7-bit addressing: 0x08-0x77)
    Print (L"Scanning I2C bus %u (%d KHz)...\n");
    Print (L"     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (Address = 0; Address < 128; Address++) {
      // Skip reserved addresses
      if ((Address < 0x08) || (Address > 0x77)) {
        continue;
      }

      // Print row header
      if (Address % 16 == 0) {
        Print (L"%02x:", Address & 0xF0);
      }

      // Setup a zero-length write operation to probe the device
      Request = AllocateZeroPool (sizeof (EFI_I2C_REQUEST_PACKET) + sizeof (EFI_I2C_OPERATION));
      if (Request == NULL) {
        Print (L"Memory allocation failed\n");
        FreePool (HandleBuffer);
        return SHELL_OUT_OF_RESOURCES;
      }

      // Configure a simple write operation with no data (just address probe)
      Request->OperationCount             = 1;
      Request->Operation[0].Flags         = 0; // Write operation
      Request->Operation[0].LengthInBytes = 0; // No data, just address probe
      Request->Operation[0].Buffer        = NULL;

      // Send the request - check if device responds
      I2cStatus = I2cMaster->StartRequest (
                                           I2cMaster,
                                           Address,
                                           Request,
                                           NULL,      // No completion event
                                           &I2cStatus // Return status
                                           );

      FreePool (Request);

      if (!EFI_ERROR (I2cStatus)) {
        // Device found
        Print (L" %02x", Address);
        DevicesFound = TRUE;
      } else {
        // No device at this address
        Print (L" --");

        // If we encounter a bus error, try to reset the bus
        if (I2cStatus == EFI_DEVICE_ERROR) {
          Status = I2cMaster->Reset (I2cMaster);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Bus reset failed: %r\n", Status));
            // Add delay to give the bus more time to recover
            gBS->Stall (10000); // 10ms delay
          }
        }
      }

      // Print newline at the end of each row
      if ((Address + 1) % 16 == 0) {
        Print (L"\n");
      }
    }

    // Add final newline if needed
    if (Address % 16 != 0) {
      Print (L"\n");
    }

    // We found the bus we were looking for, so break out of the loop
    break;
  }

  FreePool (HandleBuffer);

  if (!BusFound) {
    Print (L"I2C bus %u not found\n", BusNum);
    return SHELL_INVALID_PARAMETER;
  }

  if (!DevicesFound) {
    Print (L"\nNo I2C devices found on bus %u\n", BusNum);
    return SHELL_NOT_FOUND;
  } else {
    Print (L"\nFound I2C device(s) on bus %u\n", BusNum);
    return SHELL_SUCCESS;
  }
}

/**
  Dump registers from an I2C device.

  @param[in] BusNum                The I2C bus number
  @param[in] DevAddr               The I2C device address
  @param[in] StartReg              The starting register address
  @param[in] Count                 The number of registers to dump

  @retval SHELL_SUCCESS            Command completed successfully.
  @retval SHELL_INVALID_PARAMETER  Invalid parameter.
  @retval SHELL_NOT_FOUND          I2C device not found.
  @retval SHELL_DEVICE_ERROR       Error communicating with the device.
**/
STATIC
SHELL_STATUS
DumpI2cRegisters (
  IN UINTN   BusNum,
  IN UINT16  DevAddr,
  IN UINT8   StartReg,
  IN UINTN   Count
  )
{
  EFI_STATUS           Status;
  EFI_I2C_IO_PROTOCOL  *I2cIo;
  UINT8                *Buffer;
  UINTN                Index;

  // EFI_I2C_REQUEST_PACKET  *RequestPacket;

  // Find the I2C device
  Status = FindI2cDevice (BusNum, DevAddr, &I2cIo);
  if (EFI_ERROR (Status)) {
    Print (L"I2C device not found at bus %u address 0x%02x\n", BusNum, DevAddr);
    return SHELL_NOT_FOUND;
  }

  // Allocate buffer for the register data
  Buffer = AllocateZeroPool (Count);
  if (Buffer == NULL) {
    return SHELL_OUT_OF_RESOURCES;
  }

  // Read data
  Status = I2cRead (BusNum, DevAddr, StartReg, Buffer, Count);
  if (EFI_ERROR (Status)) {
    Print (L"Error reading from I2C device: %r\n", Status);
    FreePool (Buffer);
    return SHELL_DEVICE_ERROR;
  }

  // Display the register contents
  Print (
         L"Registers from 0x%02x to 0x%02x on I2C bus %u device 0x%02x:\n",
         StartReg,
         StartReg + Count - 1,
         BusNum,
         DevAddr
         );

  for (Index = 0; Index < Count; Index++) {
    if (Index % 16 == 0) {
      if (Index > 0) {
        Print (L"\n");
      }

      Print (L"%02x:", StartReg + Index);
    }

    Print (L" %02x", Buffer[Index]);
  }

  Print (L"\n");

  FreePool (Buffer);
  return SHELL_SUCCESS;
}

STATIC
EFI_STATUS
SetI2cRegister (
  IN UINTN   Bus,
  IN UINT16  Address,
  IN UINT8   Register,
  IN UINT8   Value
  )
{
  EFI_STATUS  Status;
  UINT8       OriginalValue;
  UINT8       ReadbackValue;

  // Read the original value
  Status = I2cRead (Bus, Address, Register, &OriginalValue, 1);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read original value: %r\n", Status);
    return Status;
  }

  // Write the new value
  Status = I2cWrite (Bus, Address, Register, &Value, 1);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to write value: %r\n", Status);
    return Status;
  }

  // Delay after write operation
  gBS->Stall (10000); // 10ms delay

  // Read back to verify
  Status = I2cRead (Bus, Address, Register, &ReadbackValue, 1);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read back value: %r\n", Status);
    return Status;
  }

  if (ReadbackValue != Value) {
    Print (
           L"Warning: Write verification failed. Original: 0x%02x, Attempted: 0x%02x, Read back: 0x%02x\n",
           OriginalValue,
           Value,
           ReadbackValue
           );
    return EFI_DEVICE_ERROR;
  }

  Print (L"Successfully wrote 0x%02x to register 0x%02x\n", Value, Register);
  return EFI_SUCCESS;
}

STATIC
SHELL_STATUS
HandleSetCommand (
  IN LIST_ENTRY  *Package
  )
{
  CONST CHAR16  *BusNumStr;
  CONST CHAR16  *DevAddrStr;
  CONST CHAR16  *RegStr;
  CONST CHAR16  *ValueStr;
  UINTN         BusNum;
  UINT16        DevAddr;
  UINT8         RegAddr;
  UINT8         RegValue;
  EFI_STATUS    Status;

  if ((ShellCommandLineGetRawValue (Package, 2) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 3) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 4) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 5) == NULL))
  {
    Print (L"Missing parameters\n");
    PrintUsage ();
    return SHELL_INVALID_PARAMETER;
  }

  BusNumStr  = ShellCommandLineGetRawValue (Package, 2);
  DevAddrStr = ShellCommandLineGetRawValue (Package, 3);
  RegStr     = ShellCommandLineGetRawValue (Package, 4);
  ValueStr   = ShellCommandLineGetRawValue (Package, 5);

  if (!ParseBusNumber (BusNumStr, &BusNum)) {
    Print (L"Invalid bus number\n");
    return SHELL_INVALID_PARAMETER;
  }

  DevAddr  = (UINT16)ShellHexStrToUintn (DevAddrStr);
  RegAddr  = (UINT8)ShellHexStrToUintn (RegStr);
  RegValue = (UINT8)ShellHexStrToUintn (ValueStr);

  Status = SetI2cRegister (BusNum, DevAddr, RegAddr, RegValue);
  if (EFI_ERROR (Status)) {
    return SHELL_DEVICE_ERROR;
  }

  return SHELL_SUCCESS;
}

/**
  Handle the I2C list command.

  @param[in] Package    The parsed command line package

  @retval Shell status code
**/
STATIC
SHELL_STATUS
HandleListCommand (
  IN LIST_ENTRY  *Package
  )
{
  return ListI2cBuses ();
}

/**
  Handle the I2C detect command.

  @param[in] Package    The parsed command line package

  @retval Shell status code
**/
STATIC
SHELL_STATUS
HandleDetectCommand (
  IN LIST_ENTRY  *Package
  )
{
  CONST CHAR16  *BusNumStr;
  UINTN         BusNum;

  BusNumStr = ShellCommandLineGetRawValue (Package, 2);
  if (!ParseBusNumber (BusNumStr, &BusNum)) {
    Print (L"Missing or invalid bus number\n");
    return SHELL_INVALID_PARAMETER;
  }

  return DetectI2cDevices (BusNum);
}

STATIC
SHELL_STATUS
HandleDumpCommand (
  IN LIST_ENTRY  *Package
  )
{
  CONST CHAR16  *BusNumStr;
  CONST CHAR16  *DevAddrStr;
  CONST CHAR16  *RegStr;
  CONST CHAR16  *CountStr;
  UINTN         BusNum;
  UINT16        DevAddr;
  UINT8         StartReg = 0;
  UINTN         RegCount = DEFAULT_DUMP_COUNT;

  if ((ShellCommandLineGetRawValue (Package, 2) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 3) == NULL))
  {
    Print (L"Missing parameters\n");
    PrintUsage ();
    return SHELL_INVALID_PARAMETER;
  }

  BusNumStr  = ShellCommandLineGetRawValue (Package, 2);
  DevAddrStr = ShellCommandLineGetRawValue (Package, 3);

  if (!ParseBusNumber (BusNumStr, &BusNum)) {
    Print (L"Invalid bus number\n");
    return SHELL_INVALID_PARAMETER;
  }

  DevAddr = (UINT16)ShellHexStrToUintn (DevAddrStr);

  if (ShellCommandLineGetRawValue (Package, 4) != NULL) {
    RegStr   = ShellCommandLineGetRawValue (Package, 4);
    StartReg = (UINT8)ShellHexStrToUintn (RegStr);

    if (ShellCommandLineGetRawValue (Package, 5) != NULL) {
      CountStr = ShellCommandLineGetRawValue (Package, 5);
      RegCount = ShellHexStrToUintn (CountStr);
    }
  }

  return DumpI2cRegisters (BusNum, DevAddr, StartReg, RegCount);
}

STATIC
SHELL_STATUS
HandleGetCommand (
  IN LIST_ENTRY  *Package
  )
{
  CONST CHAR16  *BusNumStr;
  CONST CHAR16  *DevAddrStr;
  CONST CHAR16  *RegStr;
  UINTN         BusNum;
  UINT16        DevAddr;
  UINT8         RegAddr;

  if ((ShellCommandLineGetRawValue (Package, 2) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 3) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 4) == NULL))
  {
    Print (L"Missing parameters\n");
    PrintUsage ();
    return SHELL_INVALID_PARAMETER;
  }

  BusNumStr  = ShellCommandLineGetRawValue (Package, 2);
  DevAddrStr = ShellCommandLineGetRawValue (Package, 3);
  RegStr     = ShellCommandLineGetRawValue (Package, 4);

  if (!ParseBusNumber (BusNumStr, &BusNum)) {
    Print (L"Invalid bus number\n");
    return SHELL_INVALID_PARAMETER;
  }

  DevAddr = (UINT16)ShellHexStrToUintn (DevAddrStr);
  RegAddr = (UINT8)ShellHexStrToUintn (RegStr);

  return DumpI2cRegisters (BusNum, DevAddr, RegAddr, 1);
}

STATIC
SHELL_STATUS
HandleWriteCommand (
  IN LIST_ENTRY  *Package
  )
{
  CONST CHAR16  *BusNumStr;
  CONST CHAR16  *DevAddrStr;
  CONST CHAR16  *DataStr;
  UINTN         BusNum;
  UINT16        DevAddr;
  UINT8         StartReg;
  UINT8         Data[MAX_BUFFER_SIZE];
  UINTN         DataCount;
  UINTN         Index;
  UINTN         ParamIndex;
  EFI_STATUS    Status;

  if ((ShellCommandLineGetRawValue (Package, 2) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 3) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 4) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 5) == NULL))
  {
    Print (L"Missing parameters\n");
    PrintUsage ();
    return SHELL_INVALID_PARAMETER;
  }

  BusNumStr  = ShellCommandLineGetRawValue (Package, 2);
  DevAddrStr = ShellCommandLineGetRawValue (Package, 3);

  if (!ParseBusNumber (BusNumStr, &BusNum)) {
    Print (L"Invalid bus number\n");
    return SHELL_INVALID_PARAMETER;
  }

  DevAddr   = (UINT16)ShellHexStrToUintn (DevAddrStr);
  StartReg  = (UINT8)ShellHexStrToUintn (ShellCommandLineGetRawValue (Package, 4));
  DataCount = 0;

  for (ParamIndex = 5; ParamIndex < MAX_BUFFER_SIZE + 5; ParamIndex++) {
    DataStr = ShellCommandLineGetRawValue (Package, ParamIndex);
    if (DataStr == NULL) {
      break;
    }

    Data[DataCount] = (UINT8)ShellHexStrToUintn (DataStr);
    DataCount++;
  }

  if (DataCount == 0) {
    Print (L"No data bytes specified\n");
    return SHELL_INVALID_PARAMETER;
  }

  // Write each byte individually to avoid multi-byte write issues
  for (Index = 0; Index < DataCount; Index++) {
    Status = I2cWrite (BusNum, DevAddr, (UINT8)(StartReg + Index), &Data[Index], 1);
    if (EFI_ERROR (Status)) {
      Print (L"Write failed at byte %d: %r\n", Index, Status);
      return SHELL_DEVICE_ERROR;
    }

    // Add brief delay
    gBS->Stall (5000);  // 5ms delay
  }

  // Verify write
  UINT8  ReadBuffer[MAX_BUFFER_SIZE];
  Status = I2cRead (BusNum, DevAddr, StartReg, ReadBuffer, DataCount);
  if (!EFI_ERROR (Status)) {
    BOOLEAN  Match = TRUE;
    for (Index = 0; Index < DataCount; Index++) {
      if (ReadBuffer[Index] != Data[Index]) {
        Match = FALSE;
        break;
      }
    }

    if (!Match) {
      Print (L"Warning: Write verification failed. Data mismatch.\n");
      Print (L"Written: ");
      for (Index = 0; Index < DataCount; Index++) {
        Print (L"0x%02x ", Data[Index]);
      }

      Print (L"\nRead back: ");
      for (Index = 0; Index < DataCount; Index++) {
        Print (L"0x%02x ", ReadBuffer[Index]);
      }

      Print (L"\n");
    }
  }

  return SHELL_SUCCESS;
}

STATIC
SHELL_STATUS
HandleReadCommand (
  IN LIST_ENTRY  *Package
  )
{
  CONST CHAR16  *BusNumStr;
  CONST CHAR16  *DevAddrStr;
  CONST CHAR16  *RegStr;
  CONST CHAR16  *CountStr;
  UINTN         BusNum;
  UINT16        DevAddr;
  UINT8         StartReg;
  UINTN         ReadCount;

  if ((ShellCommandLineGetRawValue (Package, 2) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 3) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 4) == NULL) ||
      (ShellCommandLineGetRawValue (Package, 5) == NULL))
  {
    Print (L"Missing parameters\n");
    PrintUsage ();
    return SHELL_INVALID_PARAMETER;
  }

  BusNumStr  = ShellCommandLineGetRawValue (Package, 2);
  DevAddrStr = ShellCommandLineGetRawValue (Package, 3);
  RegStr     = ShellCommandLineGetRawValue (Package, 4);
  CountStr   = ShellCommandLineGetRawValue (Package, 5);

  if (!ParseBusNumber (BusNumStr, &BusNum)) {
    Print (L"Invalid bus number\n");
    return SHELL_INVALID_PARAMETER;
  }

  DevAddr   = (UINT16)ShellHexStrToUintn (DevAddrStr);
  StartReg  = (UINT8)ShellHexStrToUintn (RegStr);
  ReadCount = ShellHexStrToUintn (CountStr);

  if ((ReadCount == 0) || (ReadCount > MAX_BUFFER_SIZE)) {
    Print (L"Invalid count. Must be between 1 and %u.\n", MAX_BUFFER_SIZE);
    return SHELL_INVALID_PARAMETER;
  }

  return DumpI2cRegisters (BusNum, DevAddr, StartReg, ReadCount);
}

/**
  Run the I2C command with the given arguments.
**/
SHELL_STATUS
EFIAPI
ShellCommandRunI2c (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS    Status;
  LIST_ENTRY    *Package;
  CHAR16        *ProblemParam;
  SHELL_STATUS  ShellStatus;
  CONST CHAR16  *Cmd;

  // Initialize
  Status = ShellCommandLineParse (EmptyParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    if ((Status == EFI_VOLUME_CORRUPTED) && (ProblemParam != NULL)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_PROBLEM), gShellI2cHiiHandle, L"i2ctool", ProblemParam);
      FreePool (ProblemParam);
      return SHELL_INVALID_PARAMETER;
    }

    ASSERT (FALSE);
    return SHELL_INVALID_PARAMETER;
  }

  // Get command name
  Cmd = ShellCommandLineGetRawValue (Package, 1);
  if (Cmd == NULL) {
    Print (L"Invalid or missing command\n");
    PrintUsage ();
    ShellCommandLineFreeVarList (Package);
    return SHELL_INVALID_PARAMETER;
  }

  // Dispatch to appropriate command handler
  if (StrCmp (Cmd, L"list") == 0) {
    ShellStatus = HandleListCommand (Package);
  } else if (StrCmp (Cmd, L"detect") == 0) {
    ShellStatus = HandleDetectCommand (Package);
  } else if (StrCmp (Cmd, L"dump") == 0) {
    ShellStatus = HandleDumpCommand (Package);
  } else if (StrCmp (Cmd, L"get") == 0) {
    ShellStatus = HandleGetCommand (Package);
  } else if (StrCmp (Cmd, L"set") == 0) {
    ShellStatus = HandleSetCommand (Package);
  } else if (StrCmp (Cmd, L"write") == 0) {
    ShellStatus = HandleWriteCommand (Package);
  } else if (StrCmp (Cmd, L"read") == 0) {
    ShellStatus = HandleReadCommand (Package);
  } else {
    Print (L"Invalid or missing command\n");
    PrintUsage ();
    ShellStatus = SHELL_INVALID_PARAMETER;
  }

  ShellCommandLineFreeVarList (Package);
  return ShellStatus;
}

/**
  Return the file name of the help text file if not using HII.

  @return The string pointer to the file name.
**/
CONST CHAR16 *
EFIAPI
ShellCommandGetManFileNameI2c (
  VOID
  )
{
  return gShellI2cFileName;
}

/**
  Destructor for the I2c Tool library.

  @param[in] ImageHandle  The image handle of the process.
  @param[in] SystemTable  The EFI System Table pointer.

  @retval EFI_SUCCESS  The shell command handlers were uninstalled successfully.
**/
EFI_STATUS
EFIAPI
ShellI2cLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gShellI2cHiiHandle != NULL) {
    HiiRemovePackages (gShellI2cHiiHandle);
  }

  return EFI_SUCCESS;
}

/**
  Constructor for the I2c Tool library.

  @param[in] ImageHandle  The image handle of the process.
  @param[in] SystemTable  The EFI System Table pointer.

  @retval EFI_SUCCESS  The shell command handlers were installed successfully.
**/
EFI_STATUS
EFIAPI
ShellI2cLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gShellI2cHiiHandle = HiiAddPackages (
                                       &gShellI2cHiiGuid,
                                       ImageHandle,
                                       UefiShellI2cLibStrings,
                                       NULL
                                       );
  if (gShellI2cHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  ShellCommandRegisterCommandName (
                                   L"i2ctool",
                                   ShellCommandRunI2c,
                                   ShellCommandGetManFileNameI2c,
                                   0,
                                   L"i2ctool",
                                   TRUE,
                                   gShellI2cHiiHandle,
                                   STRING_TOKEN (STR_GET_HELP_I2C)
                                   );

  return EFI_SUCCESS;
}
