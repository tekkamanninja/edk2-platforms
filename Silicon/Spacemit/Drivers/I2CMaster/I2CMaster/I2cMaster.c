/** @file
  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/UefiLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseLib.h>
#include <Base.h>
#include <Library/PcdLib.h>
#include <Pi/PiI2c.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Uefi/UefiBaseType.h>
#include <Library/MemoryManagementLib.h>

#include "I2cMaster.h"

GLOBAL_REMOVE_IF_UNREFERENCED STATIC SP_I2C_DEVICE_PATH  SpI2cDevicePathProtocol = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(OFFSET_OF (SP_I2C_DEVICE_PATH, End)),
        (UINT8)(OFFSET_OF (SP_I2C_DEVICE_PATH, End) >> 8),
      },
    },
    EFI_CALLER_ID_GUID
  },
  0,   // Instance
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      sizeof (EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};

// I2C Enumerate Protocol template
EFI_I2C_ENUMERATE_PROTOCOL  mI2cEnumerateProtocol = {
  I2cEnumerate
};

// I2C Bus Configuration Management Protocol template
EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL  mI2cBusConfigProtocol = {
  I2cBusConfiguration
};

STATIC
EFI_STATUS
I2cLocatePinCtrlProtocol (
  IN OUT I2C_MASTER_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;

  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Context->PinCtrlProtocol != NULL) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconPinCtrlProtocolGuid,
                                NULL,
                                (VOID **)&Context->PinCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    Context->PinCtrlProtocol = NULL;
  }

  return Status;
}

STATIC
VOID
I2cApplyPinState (
  IN I2C_MASTER_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;

  if (Context == NULL) {
    return;
  }

  if (Context->PinctrlStateUnavailable || Context->PinctrlStateApplied) {
    return;
  }

  Status = I2cLocatePinCtrlProtocol (Context);
  if (EFI_ERROR (Status) || (Context->PinCtrlProtocol == NULL)) {
    return;
  }

  if (!Context->PinctrlControllerRegistered) {
    if (Context->PinCtrlProtocol->RegisterController != NULL) {
      Status = Context->PinCtrlProtocol->RegisterController (
                                           Context->PinCtrlProtocol,
                                           Context->ControllerHandle,
                                           "i2c",
                                           Context->ControllerId
                                           );
      if (EFI_ERROR (Status)) {
        DEBUG (
               (DEBUG_WARN, "I2C[%u]: failed to register pinctrl controller (%r)\n",
                Context->ControllerId, Status)
               );
        return;
      }
    }

    Context->PinctrlControllerRegistered = TRUE;
  }

  if (Context->PinCtrlProtocol->ApplyStateById == NULL) {
    Context->PinctrlStateUnavailable = TRUE;
    DEBUG (
           (DEBUG_WARN, "I2C[%u]: pinctrl state API is unavailable\n",
            Context->ControllerId)
           );
    return;
  }

  Status = Context->PinCtrlProtocol->ApplyStateById (
                                        Context->PinCtrlProtocol,
                                        "i2c",
                                        Context->ControllerId,
                                        NULL,
                                        PINCTRL_STATE_DEFAULT
                                        );

  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      Context->PinctrlStateUnavailable = TRUE;
      DEBUG (
             (DEBUG_WARN,
              "I2C[%u]: pinctrl state map for default state is not available\n",
              Context->ControllerId)
             );
      return;
    }

    DEBUG (
           (DEBUG_WARN, "I2C[%u]: failed to apply pinctrl default state - %r\n",
            Context->ControllerId,
            Status)
           );
    return;
  }

  Context->PinctrlStateApplied = TRUE;
}

/**
  Execute a single I2C transfer.

  @param[in] Context    I2C controller context
  @param[in,out] Msg    I2C message structure

  @retval EFI_SUCCESS           Transfer successful
  @retval EFI_TIMEOUT           Transfer timeout
  @retval EFI_NO_RESPONSE       No device responded (NACK received)
  @retval EFI_DEVICE_ERROR      Device error
**/
STATIC
EFI_STATUS
I2cTransfer (
  IN I2C_MASTER_CONTEXT    *Context,
  IN OUT SPACEMIT_I2C_MSG  *Msg
  )
{
  UINT32  Value;
  UINT32  IcrValue;
  UINTN   Timeout;

  // Read the current ICR value
  IcrValue = MmioRead32 (Context->BaseAddress + ICR_OFFSET);

  // Clear all interrupt status bits
  MmioWrite32 (Context->BaseAddress + ISR_OFFSET, 0xFFFFFFFF);

  // Check if bus is busy
  Value = MmioRead32 (Context->BaseAddress + ISR_OFFSET);
  if (Value & ISR_IBB) {
    // Try to recover the bus
    IcrValue &= ~(ICR_START | ICR_STOP | ICR_ACKNAK | ICR_TB | ICR_MA);
    MmioWrite32 (Context->BaseAddress + ICR_OFFSET, IcrValue);

    // Send STOP condition to release the bus
    IcrValue |= ICR_STOP;
    MmioWrite32 (Context->BaseAddress + ICR_OFFSET, IcrValue);

    // Wait for bus to be released
    Timeout = I2C_TRANSFER_TIMEOUT;
    while (Timeout > 0) {
      Value = MmioRead32 (Context->BaseAddress + ISR_OFFSET);
      if (!(Value & ISR_IBB)) {
        break;
      }

      MicroSecondDelay (10);
      Timeout -= 10;
    }

    if (Timeout == 0) {
      return EFI_TIMEOUT;
    }
  }

  // Prepare control register for transfer
  IcrValue &= ~(ICR_START | ICR_STOP | ICR_ACKNAK);

  // Set START condition if needed
  if (Msg->Condition == I2C_COND_START) {
    IcrValue |= ICR_START;
  }

  // Set STOP condition if needed
  if (Msg->Condition == I2C_COND_STOP) {
    IcrValue |= ICR_STOP;
  }

  // Set ACKNAK if needed
  if (Msg->AckNack == I2C_ACKNAK_SENDNAK) {
    IcrValue |= ICR_ACKNAK;
  }

  // Write data to IDBR for transmission
  MmioWrite32 (Context->BaseAddress + IDBR_OFFSET, Msg->Data);

  // Start transfer
  IcrValue |= ICR_TB;
  MmioWrite32 (Context->BaseAddress + ICR_OFFSET, IcrValue);

  // Wait for transfer completion or timeout
  Timeout = I2C_TRANSFER_TIMEOUT;
  while (Timeout > 0) {
    Value = MmioRead32 (Context->BaseAddress + ISR_OFFSET);

    // Check for bus errors
    if (Value & (ISR_BED | ISR_ALD)) {
      // Clear error status
      MmioWrite32 (Context->BaseAddress + ISR_OFFSET, Value);
      return EFI_DEVICE_ERROR;
    }

    // Check for transfer completion
    if (Msg->Direction == I2C_WRITE) {
      if (Value & ISR_ITE) {
        // Clear ITE status
        MmioWrite32 (Context->BaseAddress + ISR_OFFSET, ISR_ITE);

        // For write operations, we need to check if ACK was received
        if ((Msg->AckNack == I2C_ACKNAK_WAITACK) && (Value & ISR_ACKNAK)) {
          return EFI_NO_RESPONSE;
        }

        return EFI_SUCCESS;
      }
    } else {
      // I2C_READ
      if ((Value & ISR_IRF) || (Value & ISR_ITE)) {
        // Read data from IDBR
        Msg->Data = MmioRead32 (Context->BaseAddress + IDBR_OFFSET);
        // Clear status flags
        MmioWrite32 (Context->BaseAddress + ISR_OFFSET, Value & (ISR_IRF | ISR_ITE));
        return EFI_SUCCESS;
      }
    }

    MicroSecondDelay (10);
    Timeout -= 10;
  }

  return EFI_TIMEOUT;
}

/**
  Set the frequency for the I2C clock line.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure
  @param[in] BusClockHertz  Pointer to the requested I2C bus clock frequency in Hertz

  @retval EFI_SUCCESS           The bus frequency was set successfully.
  @retval EFI_UNSUPPORTED      The controller does not support this frequency.
**/
EFI_STATUS
EFIAPI
I2cMasterSetBusFrequency (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This,
  IN OUT UINTN                      *BusClockHertz
  )
{
  I2C_MASTER_CONTEXT  *Context;

  Context = I2C_MASTER_FROM_THIS (This);
  return I2cSetSpeed (Context, *BusClockHertz);
}

/**
  Get the requested I2C bus frequency for the specified bus configuration.

  @param[in] This          Address of an EFI_I2C_ENUMERATE_PROTOCOL structure
  @param[in] BusConfiguration  I2C bus configuration to access the I2C device
  @param[out] BusClockHertz   Pointer to a buffer to receive the I2C bus clock frequency in Hertz

  @retval EFI_SUCCESS          The bus frequency was returned successfully
  @retval EFI_INVALID_PARAMETER BusClockHertz is NULL
**/
EFI_STATUS
EFIAPI
I2cGetBusFrequency (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL  *This,
  IN UINTN                             BusConfiguration,
  OUT UINTN                            *BusClockHertz
  )
{
  I2C_MASTER_CONTEXT  *I2cContext;

  if (BusClockHertz == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  I2cContext = I2C_MASTER_CONTEXT_FROM_ENUMERATE (This);

  *BusClockHertz = I2cContext->ClockRate;

  return EFI_SUCCESS;
}

/**
  Reset the I2C controller and configure it for use.

  @param[in] This     Pointer to an EFI_I2C_MASTER_PROTOCOL structure.

  @retval EFI_SUCCESS           The reset completed successfully.
  @retval EFI_DEVICE_ERROR     The reset operation failed.
**/
EFI_STATUS
EFIAPI
I2cMasterReset (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This
  )
{
  I2C_MASTER_CONTEXT  *Context;
  UINT32              IcrMode;
  UINT32              Value;

  Context = I2C_MASTER_FROM_THIS (This);

  // Read current ICR value and save mode
  Value   = MmioRead32 (Context->BaseAddress + ICR_OFFSET);
  IcrMode = Value & ICR_MODE_MASK;

  // 1. Disable unit
  MmioAnd32 (Context->BaseAddress + ICR_OFFSET, (UINT32) ~((UINT32)ICR_IUE));

  // 2. Reset unit
  MmioOr32 (Context->BaseAddress + ICR_OFFSET, ICR_UR);
  MicroSecondDelay (100);

  // 3. Initialize registers
  MmioWrite32 (Context->BaseAddress + ISAR_OFFSET, 0);
  MmioWrite32 (Context->BaseAddress + ICR_OFFSET, I2C_ICR_INIT | IcrMode);
  MmioWrite32 (Context->BaseAddress + ISR_OFFSET, I2C_ISR_INIT);

  // 4. Enable unit
  MmioOr32 (Context->BaseAddress + ICR_OFFSET, ICR_IUE);
  MicroSecondDelay (100);

  return EFI_SUCCESS;
}

/**
  Start an I2C transaction on the host controller.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure.
  @param[in] SlaveAddress   Address of the device on the I2C bus.
  @param[in] RequestPacket  Pointer to an EFI_I2C_REQUEST_PACKET structure.
  @param[in] Event          Event to signal for asynchronous transactions.
  @param[out] I2cStatus     Optional buffer to receive the I2C transaction completion status.

  @retval EFI_SUCCESS           The transaction completed successfully.
  @retval EFI_DEVICE_ERROR     There was an I2C error during the transaction.
**/
EFI_STATUS
EFIAPI
I2cMasterStartRequest (
  IN CONST EFI_I2C_MASTER_PROTOCOL  *This,
  IN UINTN                          SlaveAddress,
  IN EFI_I2C_REQUEST_PACKET         *RequestPacket,
  IN EFI_EVENT                      Event OPTIONAL,
  OUT EFI_STATUS                    *I2cStatus OPTIONAL
  )
{
  I2C_MASTER_CONTEXT  *Context;
  SPACEMIT_I2C_MSG    Msg;
  EFI_STATUS          Status;
  UINTN               Count;
  UINTN               ReadMode;
  EFI_I2C_OPERATION   *Operation;

  if (RequestPacket == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Context = I2C_MASTER_FROM_THIS (This);
  Status  = EFI_SUCCESS;

  // Keep pins in I2C mux mode.
  I2cApplyPinState (Context);

  for (Count = 0; Count < RequestPacket->OperationCount; Count++) {
    Operation = &RequestPacket->Operation[Count];
    ReadMode  = Operation->Flags & I2C_FLAG_READ;

    // Send START condition for first operation or when restart is needed
    if (Count == 0) {
      // Send START + slave address
      Msg.Condition = I2C_COND_START;
      Msg.Direction = I2C_WRITE;
      Msg.Data      = (UINT8)((SlaveAddress << 1) | (ReadMode ? 1 : 0));
      Msg.AckNack   = I2C_ACKNAK_WAITACK;

      Status = I2cTransfer (Context, &Msg);
      if (EFI_ERROR (Status)) {
        goto Exit;
      }
    } else if (!(Operation->Flags & I2C_FLAG_NORESTART)) {
      // Send repeated START only when needed
      if (ReadMode) {
        // Restart needed for read operations
        Msg.Condition = I2C_COND_START;
        Msg.Direction = I2C_READ;
        Msg.Data      = (UINT8)((SlaveAddress << 1) | 1);
        Msg.AckNack   = I2C_ACKNAK_WAITACK;

        Status = I2cTransfer (Context, &Msg);
        if (EFI_ERROR (Status)) {
          goto Exit;
        }
      }

      // Continue with current bus state for write operations
    }

    if (ReadMode) {
      // Read data
      for (UINTN i = 0; i < Operation->LengthInBytes; i++) {
        Msg.Condition = I2C_COND_NORMAL;
        Msg.Direction = I2C_READ;

        // Send NAK for last byte
        if (i == Operation->LengthInBytes - 1) {
          Msg.AckNack = I2C_ACKNAK_SENDNAK;
          // Add STOP condition if this is the last operation
          if (Count == RequestPacket->OperationCount - 1) {
            Msg.Condition = I2C_COND_STOP;
          }
        } else {
          Msg.AckNack = I2C_ACKNAK_WAITACK;
        }

        Status = I2cTransfer (Context, &Msg);
        if (EFI_ERROR (Status)) {
          if ((Status == EFI_NO_RESPONSE) &&
              (i == Operation->LengthInBytes - 1) &&
              (Count == RequestPacket->OperationCount - 1))
          {
            Status = EFI_SUCCESS;
          } else {
            goto Exit;
          }
        }

        Operation->Buffer[i] = Msg.Data;
      }
    } else {
      // Write data
      for (UINTN i = 0; i < Operation->LengthInBytes; i++) {
        Msg.Direction = I2C_WRITE;
        Msg.Data      = Operation->Buffer[i];

        // Send device address for first byte of first operation
        if ((Count == 0) && (i == 0)) {
          Msg.Condition = I2C_COND_START;
          Msg.Data      = (SlaveAddress << 1); // Write address
          Msg.AckNack   = I2C_ACKNAK_WAITACK;
          Status        = I2cTransfer (Context, &Msg);
          if (EFI_ERROR (Status)) {
            goto Exit;
          }

          // Send internal address
          Msg.Condition = I2C_COND_NORMAL;
          Msg.Data      = Operation->Buffer[i];
        }
        // Add STOP for last byte
        else if ((i == Operation->LengthInBytes - 1) && (Count == RequestPacket->OperationCount - 1)) {
          Msg.Condition = I2C_COND_STOP;
        } else {
          Msg.Condition = I2C_COND_NORMAL;
        }

        Msg.AckNack = I2C_ACKNAK_WAITACK;

        Status = I2cTransfer (Context, &Msg);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "I2C: Write transfer failed - %r\n", Status));
          goto Exit;
        }
      }
    }
  }

Exit:
  if (I2cStatus != NULL) {
    *I2cStatus = Status;
  }

  if (Event != NULL) {
    gBS->SignalEvent (Event);
  }

  return Status;
}

/**
  Map I2C controller MMIO regions to GCD space.

  @retval None
**/
STATIC
VOID
I2cMmioRemap (
  IN I2C_MASTER_CONTEXT  *Context
  )
{
  // Map I2C controller registers to MMIO space
  MapRegToGcdMmioSpace (Context->BaseAddress, SIZE_2KB);
}

/**
  Initialize a single I2C controller.

  @param[in] BaseAddress    I2C controller base address
  @param[in] ClockRate      I2C bus clock frequency
  @param[in] ControllerHandle Controller handle

  @retval EFI_SUCCESS      Controller initialization successful
  @retval Others           Initialization failed
**/
STATIC
EFI_STATUS
SpacemitI2cInitController (
  IN UINTN       BaseAddress,
  IN UINT32      ClockRate,
  IN EFI_HANDLE  ControllerHandle
  )
{
  I2C_MASTER_CONTEXT  *I2cContext;
  EFI_STATUS          Status;

  DEBUG ((DEBUG_INFO, "I2C: Initializing controller at 0x%x\n", BaseAddress));

  // Allocate controller context
  I2cContext = AllocateZeroPool (sizeof (I2C_MASTER_CONTEXT));
  if (I2cContext == NULL) {
    DEBUG ((DEBUG_ERROR, "I2C: Failed to allocate context\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Initialize context
  I2cContext->Signature            = I2C_MASTER_SIGNATURE;
  I2cContext->BaseAddress          = BaseAddress;
  I2cContext->ClockRate            = ClockRate;
  I2cContext->ControllerHandle     = ControllerHandle;
  I2cContext->CurrentConfiguration = 0; // Default to standard mode

  // Map I2C controller MMIO regions
  I2cMmioRemap (I2cContext);

  // Reset and initialize hardware
  Status = I2cMasterReset (&I2cContext->I2cMaster);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "I2C: Failed to reset controller - %r\n", Status));
    goto ErrorExit;
  }

  // Set bus frequency
  UINTN  BusSpeed = (UINTN)I2cContext->ClockRate;
  Status = I2cMasterSetBusFrequency (&I2cContext->I2cMaster, &BusSpeed);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "I2C: Failed to set bus frequency - %r\n", Status));
    goto ErrorExit;
  }

  return EFI_SUCCESS;

ErrorExit:
  DEBUG ((DEBUG_ERROR, "I2C: Controller initialization failed, cleaning up\n"));
  gBS->UninstallMultipleProtocolInterfaces (
                                            ControllerHandle,
                                            &gEfiI2cMasterProtocolGuid,
                                            &I2cContext->I2cMaster,
                                            NULL
                                            );
  FreePool (I2cContext);
  return Status;
}

/**
  Allocate an I2C device structure.

  @param[in] SlaveAddress   I2C slave address
  @param[in,out] Device     Pointer to receive allocated device structure

  @retval EFI_SUCCESS           Device allocated successfully
  @retval EFI_INVALID_PARAMETER Invalid Device pointer
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate resources
**/
EFI_STATUS
AllocateI2cDevice (
  IN UINT8                     SlaveAddress,
  IN OUT CONST EFI_I2C_DEVICE  **Device
  )
{
  EFI_I2C_DEVICE  *NewDevice;
  UINT32          *SlaveAddressArray;

  // Parameter check
  if (Device == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Allocate device structure
  NewDevice = AllocateZeroPool (sizeof (EFI_I2C_DEVICE));
  if (NewDevice == NULL) {
    DEBUG ((DEBUG_ERROR, "I2C: Failed to allocate device structure\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Allocate slave address array
  SlaveAddressArray = AllocatePool (sizeof (UINT32));
  if (SlaveAddressArray == NULL) {
    DEBUG ((DEBUG_ERROR, "I2C: Failed to allocate slave address array\n"));
    FreePool (NewDevice);
    return EFI_OUT_OF_RESOURCES;
  }

  // Initialize device structure
  SlaveAddressArray[0]           = SlaveAddress;
  NewDevice->DeviceGuid          = &gEfiI2cDeviceGuid;// Use generic I2C device GUID
  NewDevice->DeviceIndex         = SlaveAddress;      // Use slave address as device index
  NewDevice->HardwareRevision    = 0;                 // Default hardware revision
  NewDevice->I2cBusConfiguration = 0;                 // Default bus configuration
  NewDevice->SlaveAddressCount   = 1;                 // Single slave address
  NewDevice->SlaveAddressArray   = SlaveAddressArray;

  // Return new device
  *Device = NewDevice;
  return EFI_SUCCESS;
}

/**
  Enumerate I2C devices.

  @param[in] This      Pointer to I2C enumerate protocol
  @param[in,out] Device  Pointer to returned I2C device

  @retval EFI_SUCCESS           Device found successfully
  @retval EFI_ALREADY_STARTED   No more devices
**/
EFI_STATUS
EFIAPI
I2cEnumerate (
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL  *This,
  IN OUT CONST EFI_I2C_DEVICE          **Device
  )
{
  I2C_MASTER_CONTEXT  *I2cContext;
  UINTN               Index;
  UINTN               NextIndex;
  UINT8               NextBus;
  UINT8               NextDeviceAddress;
  UINTN               DevCount = 1; // Default to 1 device for testing

  // Parameter check
  if ((This == NULL) || (Device == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  I2cContext = I2C_MASTER_CONTEXT_FROM_ENUMERATE (This);
  if (I2cContext->Signature != I2C_MASTER_SIGNATURE) {
    DEBUG ((DEBUG_ERROR, "I2C: Invalid context signature\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Get slave configurations from PCD
  I2C_SLAVE_CONFIG_ARRAY  *SlaveConfigs;
  SlaveConfigs = (I2C_SLAVE_CONFIG_ARRAY *)PcdGetPtr (PcdI2cSlaveConfig);
  DevCount     = SlaveConfigs->Num;

  if (*Device == NULL) {
    // First enumeration - find first device
    for (Index = 0; Index < DevCount; Index++) {
      NextBus           = SlaveConfigs->Data[Index].BusNumber;
      NextDeviceAddress = SlaveConfigs->Data[Index].SlaveAddress;

      // Only process devices on current bus
      if (NextBus != I2cContext->ControllerId) {
        continue;
      }

      // Found first device on current bus
      return AllocateI2cDevice (NextDeviceAddress, Device);
    }
  } else {
    // Continue enumeration - find next device
    for (Index = 0; Index < DevCount; Index++) {
      NextBus           = SlaveConfigs->Data[Index].BusNumber;
      NextDeviceAddress = SlaveConfigs->Data[Index].SlaveAddress;

      if (NextBus != I2cContext->ControllerId) {
        continue;
      }

      // Found current device
      if (NextDeviceAddress == (*Device)->DeviceIndex) {
        // Find next device
        for (NextIndex = Index + 1; NextIndex < DevCount; NextIndex++) {
          NextBus = SlaveConfigs->Data[NextIndex].BusNumber;
          if (NextBus != I2cContext->ControllerId) {
            continue;
          }

          NextDeviceAddress = SlaveConfigs->Data[NextIndex].SlaveAddress;
          return AllocateI2cDevice (NextDeviceAddress, Device);
        }

        break;
      }
    }

    // No more devices
    *Device = NULL;
  }

  return EFI_SUCCESS;
}

/**
  Configure I2C bus.

  @param[in] This                Pointer to I2C bus configuration protocol
  @param[in] Configuration       Requested configuration
  @param[in] Event              Optional event to signal completion
  @param[out] ConfigurationStatus Status of configuration request

  @retval EFI_SUCCESS           Configuration successful
  @retval EFI_INVALID_PARAMETER Invalid parameter
  @retval EFI_UNSUPPORTED       Configuration not supported
**/
EFI_STATUS
EFIAPI
I2cBusConfiguration (
  IN CONST EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL  *This,
  IN UINTN                                                Configuration,
  IN EFI_EVENT                                            Event OPTIONAL,
  IN EFI_STATUS                                           *ConfigurationStatus
  )
{
  I2C_MASTER_CONTEXT  *Context;
  EFI_STATUS          Status;

  if (ConfigurationStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Context = I2C_MASTER_FROM_BUS_CONFIG (This);

  // Return success if already in requested configuration
  if (Context->CurrentConfiguration == Configuration) {
    *ConfigurationStatus = EFI_SUCCESS;
    Status               = EFI_SUCCESS;
    goto Exit;
  }

  // Set mode based on configuration number
  switch (Configuration) {
    case 0: // Standard Mode (100KHz)
      Status = I2cSetSpeed (Context, 100000);
      break;

    case 1: // Fast Mode (400KHz)
      Status = I2cSetSpeed (Context, 400000);
      break;

    case 2: // Fast Mode Plus (1MHz)
      Status = I2cSetSpeed (Context, 1000000);
      break;

    case 3: // High Speed Mode (3.4MHz)
      Status = I2cSetSpeed (Context, 3400000);
      break;

    default:
      Status = EFI_UNSUPPORTED;
      break;
  }

  *ConfigurationStatus = Status;

  if (!EFI_ERROR (Status)) {
    Context->CurrentConfiguration = Configuration;
  }

Exit:
  if (Event != NULL) {
    gBS->SignalEvent (Event);
  }

  return Status;
}

/**
  Test I2C protocols functionality.

  This function tests the I2C Enumerate and Bus Configuration protocols
  on all available handles.
**/
VOID
TestI2cProtocols (
  VOID
  )
{
  EFI_STATUS                                     Status;
  UINTN                                          HandleCount, Index;
  EFI_HANDLE                                     *HandleBuffer;
  EFI_I2C_ENUMERATE_PROTOCOL                     *I2cEnumerate;
  EFI_I2C_BUS_CONFIGURATION_MANAGEMENT_PROTOCOL  *I2cBusConfig;
  CONST EFI_I2C_DEVICE                           *Device = NULL;
  EFI_STATUS                                     ConfigStatus;

  // Locate all handles supporting Enumerate protocol
  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEfiI2cEnumerateProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &HandleBuffer
                                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "I2C: Unable to locate I2c Enumerate handles - %r\n", Status));
    return;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    // Open Enumerate protocol
    Status = gBS->OpenProtocol (
                                HandleBuffer[Index],
                                &gEfiI2cEnumerateProtocolGuid,
                                (VOID **)&I2cEnumerate,
                                gImageHandle,
                                HandleBuffer[Index],
                                EFI_OPEN_PROTOCOL_GET_PROTOCOL
                                );
    if (!EFI_ERROR (Status)) {
      Status = I2cEnumerate->Enumerate (I2cEnumerate, &Device);
      if (!EFI_ERROR (Status) && (Device != NULL)) {
        DEBUG (
               (DEBUG_INFO, "I2C: Found device with address 0x%x\n",
                Device->SlaveAddressArray[0])
               );
      }
    }

    // Open Bus Configuration protocol
    Status = gBS->OpenProtocol (
                                HandleBuffer[Index],
                                &gEfiI2cBusConfigurationManagementProtocolGuid,
                                (VOID **)&I2cBusConfig,
                                gImageHandle,
                                HandleBuffer[Index],
                                EFI_OPEN_PROTOCOL_GET_PROTOCOL
                                );
    if (!EFI_ERROR (Status)) {
      Status = I2cBusConfig->EnableI2cBusConfiguration (
                                                        I2cBusConfig,
                                                        0,
                                                        NULL,
                                                        &ConfigStatus
                                                        );
    }
  }

  FreePool (HandleBuffer);
}

/**
  Set I2C bus speed.

  @param[in] Context      I2C controller context
  @param[in] SpeedHz      Target speed (Hz)

  @retval EFI_SUCCESS     Speed set successfully
  @retval Others          Setting failed
**/
EFI_STATUS
I2cSetSpeed (
  IN I2C_MASTER_CONTEXT  *Context,
  IN UINTN               SpeedHz
  )
{
  UINT32  Value;

  // Disable I2C unit
  Value = MmioRead32 (Context->BaseAddress + ICR_OFFSET);
  MmioWrite32 (Context->BaseAddress + ICR_OFFSET, Value & ~ICR_IUE);
  MicroSecondDelay (100);

  // Set mode based on speed
  if (SpeedHz > 100000) {
    Value = (Value & ~ICR_MODE_MASK) | ICR_FM; // Fast Mode
  } else {
    Value = (Value & ~ICR_MODE_MASK) | ICR_SM; // Standard Mode
  }

  // Update ICR register
  MmioWrite32 (Context->BaseAddress + ICR_OFFSET, Value | ICR_IUE);
  MicroSecondDelay (100);

  // Update clock rate
  Context->ClockRate = SpeedHz;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
I2cMasterEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  UINTN               Index;
  UINTN               NumControllers;
  STATIC INTN         Bus = 0;
  I2C_MASTER_CONTEXT  *I2cContext;
  SP_I2C_DEVICE_PATH  *DevicePath;
  EFI_HANDLE          LastHandle = NULL;

  DEBUG ((DEBUG_INFO, "I2C: Driver starting\n"));

  // Get controller configurations from PCD
  I2C_CONTROLLER_CONFIG_ARRAY  *ControllerConfigs;
  ControllerConfigs = (I2C_CONTROLLER_CONFIG_ARRAY *)PcdGetPtr (PcdI2cControllerConfigs);
  NumControllers    = ControllerConfigs->Num;

  DEBUG ((DEBUG_INFO, "I2C: Found %d controllers in PCD\n", NumControllers));

  for (Index = 0; Index < NumControllers; Index++) {
    UINT8       ControllerId = ControllerConfigs->Data[Index].ControllerId;
    UINT64      BaseAddress  = ControllerConfigs->Data[Index].BaseAddress;
    UINT32      ClockRate    = ControllerConfigs->Data[Index].ClockRate;
    BOOLEAN     Enabled      = ControllerConfigs->Data[Index].Enable;
    EFI_HANDLE  NewHandle    = NULL;

    if (!Enabled) {
      continue;
    }

    DEBUG (
           (DEBUG_INFO, "I2C: Controller[%d]: ID=%d, Base=0x%x, Clock=%d\n",
            Index, ControllerId, BaseAddress, ClockRate)
           );

    // Allocate I2C master context
    I2cContext = AllocateZeroPool (sizeof (I2C_MASTER_CONTEXT));
    if (I2cContext == NULL) {
      DEBUG ((DEBUG_ERROR, "I2C: Failed to allocate I2C context\n"));
      continue;
    }

    // Allocate and initialize device path
    DevicePath = AllocateCopyPool (sizeof (SP_I2C_DEVICE_PATH), &SpI2cDevicePathProtocol);
    if (DevicePath == NULL) {
      DEBUG ((DEBUG_ERROR, "I2C: Failed to allocate device path\n"));
      FreePool (I2cContext);
      continue;
    }

    DevicePath->Instance = Bus;

    // Initialize context
    I2cContext->Signature            = I2C_MASTER_SIGNATURE;
    I2cContext->BaseAddress          = BaseAddress;
    I2cContext->ClockRate            = ClockRate;
    I2cContext->ControllerId         = ControllerId;
    I2cContext->CurrentConfiguration = 0; // Default to standard mode

    // Initialize I2C Master protocol
    I2cContext->I2cMaster.Reset           = I2cMasterReset;
    I2cContext->I2cMaster.StartRequest    = I2cMasterStartRequest;
    I2cContext->I2cMaster.SetBusFrequency = I2cMasterSetBusFrequency;

    // Initialize I2C Enumerate protocol
    I2cContext->I2cEnumerate.Enumerate       = I2cEnumerate;
    I2cContext->I2cEnumerate.GetBusFrequency = I2cGetBusFrequency;

    // Initialize I2C Bus Configuration protocol
    I2cContext->I2cBusConfig.EnableI2cBusConfiguration = I2cBusConfiguration;

    // Install protocols on new handle
    Status = gBS->InstallMultipleProtocolInterfaces (
                                                     &NewHandle,
                                                     &gEfiI2cMasterProtocolGuid,
                                                     &I2cContext->I2cMaster,
                                                     &gEfiI2cEnumerateProtocolGuid,
                                                     &I2cContext->I2cEnumerate,
                                                     &gEfiI2cBusConfigurationManagementProtocolGuid,
                                                     &I2cContext->I2cBusConfig,
                                                     &gEfiDevicePathProtocolGuid,
                                                     DevicePath,
                                                     NULL
                                                     );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "I2C: Failed to install protocols\n"));
      FreePool (DevicePath);
      FreePool (I2cContext);
      continue;
    }

    // Save handle
    I2cContext->ControllerHandle = NewHandle;
    LastHandle                   = NewHandle;

    // Initialize I2C controller
    Status = SpacemitI2cInitController (BaseAddress, ClockRate, NewHandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "I2C: Failed to init controller %d - %r\n", ControllerId, Status));
      gBS->UninstallMultipleProtocolInterfaces (
                                                NewHandle,
                                                &gEfiI2cMasterProtocolGuid,
                                                &I2cContext->I2cMaster,
                                                &gEfiI2cEnumerateProtocolGuid,
                                                &I2cContext->I2cEnumerate,
                                                &gEfiI2cBusConfigurationManagementProtocolGuid,
                                                &I2cContext->I2cBusConfig,
                                                &gEfiDevicePathProtocolGuid,
                                                DevicePath,
                                                NULL
                                                );
      FreePool (DevicePath);
      FreePool (I2cContext);
      continue;
    }

    I2cApplyPinState (I2cContext);

    DEBUG ((DEBUG_INFO, "I2C: Successfully initialized controller %d\n", ControllerId));
    Bus++;
  }

  return EFI_SUCCESS;
}
