/**
 *
 *  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Uefi/UefiBaseType.h>

#include "GpioDxe.h"

// Device path template
STATIC CONST struct {
  VENDOR_DEVICE_PATH          Vendor;
  EFI_DEVICE_PATH_PROTOCOL    End;
} mDevicePathTemplate = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8),
      },
    },
    // Use driver GUID
    K3_GPIO_DEVICE_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL)),
      (UINT8)((sizeof (EFI_DEVICE_PATH_PROTOCOL)) >> 8),
    },
  }
};

STATIC CONST UINT32  GroupOffset[K3_GPIO_GROUP_COUNT] = {
  K3_GPIO_GROUP0_OFFSET,
  K3_GPIO_GROUP1_OFFSET,
  K3_GPIO_GROUP2_OFFSET,
  K3_GPIO_GROUP3_OFFSET
};

/**
 * Get GPIO register base address
 *
 * @param[in]  Group          GPIO group number
 *
 * @retval     Register base address, or NULL if Group is invalid
 */
STATIC
K3_GPIO_REGISTERS *
GetGpioBase (
  IN UINTN  Group
  )
{
  UINTN  BaseAddress;

  if (Group >= K3_GPIO_GROUP_COUNT) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Invalid GPIO group %d\n", Group));
    return NULL;
  }

  BaseAddress = K3_GPIO_REG_BASE;
  return (K3_GPIO_REGISTERS *)(BaseAddress + GroupOffset[Group]);
}

/**
  Map GPIO controller MMIO regions to GCD space.

  @retval None
**/
STATIC
VOID
GpioMmioRemap (
  VOID
  )
{
  // Map GPIO controller registers to MMIO space
  MapRegToGcdMmioSpace (K3_GPIO_REG_BASE, SIZE_4KB);
  // Map MFPR pinmux registers for GPIO pin function/pull programming.
  MapRegToGcdMmioSpace (K3_MFPR_BASE, SIZE_4KB);
}

STATIC
UINT32
BuildGpioPinConfig (
  IN UINT32  PullMode
  )
{
  return K3_GPIO_PINMUX_FUNC0 |
         (K3_GPIO_PIN_DRIVE_DEFAULT << K3_GPIO_PIN_DRIVE_SHIFT) |
         K3_GPIO_PIN_EDGE_NONE | PullMode;
}

STATIC
VOID
ApplyGpioPinConfig (
  IN UINT32  GpioPin,
  IN UINT32  PullMode
  )
{
  UINT32  MuxConfig;

  MuxConfig = BuildGpioPinConfig (PullMode & K3_GPIO_PIN_PULL_MASK);
  MmioWrite32 (K3_GPIO_PIN_CFG_REG (GpioPin), MuxConfig);
}

/**
 * Get GPIO state
 *
 * @param[in]  This          GPIO protocol instance
 * @param[in]  Gpio          GPIO pin number
 * @param[out] Value         GPIO state
 *
 * @retval EFI_SUCCESS       Operation successful
 * @retval EFI_INVALID_PARAMETER Invalid parameter
 */
STATIC
EFI_STATUS
EFIAPI
K3GpioGet (
  IN EMBEDDED_GPIO      *This,
  IN EMBEDDED_GPIO_PIN  Gpio,
  OUT UINTN             *Value
  )
{
  K3_GPIO_REGISTERS  *GpioReg;
  UINTN              GpioPin;
  UINT32             RegValue;

  if (Value == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GpioPin = (UINTN)Gpio;
  if (GpioPin >= K3_MAX_GPIO) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Invalid GPIO %d\n", GpioPin));
    return EFI_INVALID_PARAMETER;
  }

  GpioReg = GetGpioBase (GPIO_GROUP (GpioPin));
  if (GpioReg == NULL) {
    return EFI_DEVICE_ERROR;
  }

  // Read GPIO state
  RegValue = MmioRead32 ((UINTN)&GpioReg->gplr);

  // Return GPIO state
  *Value = (RegValue & GPIO_BIT_OFFSET (GpioPin)) ? 1 : 0;

  return EFI_SUCCESS;
}

/**
 * Set GPIO mode
 *
 * @param[in]  This          GPIO protocol instance
 * @param[in]  Gpio          GPIO pin number
 * @param[in]  Mode          GPIO mode
 *
 * @retval EFI_SUCCESS       Operation successful
 * @retval EFI_INVALID_PARAMETER Invalid parameter
 * @retval EFI_UNSUPPORTED   Unsupported mode
 */
STATIC
EFI_STATUS
EFIAPI
K3GpioSet (
  IN EMBEDDED_GPIO       *This,
  IN EMBEDDED_GPIO_PIN   Gpio,
  IN EMBEDDED_GPIO_MODE  Mode
  )
{
  K3_GPIO_REGISTERS  *GpioReg;
  UINT32             GpioPin;

  GpioPin = (UINT32)Gpio;
  (VOID)This;

  if (GpioPin >= K3_MAX_GPIO) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Invalid GPIO %d\n", GpioPin));
    return EFI_INVALID_PARAMETER;
  }

  GpioReg = GetGpioBase (GPIO_GROUP (GpioPin));
  if (GpioReg == NULL) {
    return EFI_DEVICE_ERROR;
  }

  ApplyGpioPinConfig (GpioPin, K3_GPIO_PIN_PULL_NONE);

  switch (Mode) {
    case GPIO_MODE_INPUT:
      // Set as input
      MmioWrite32 ((UINTN)&GpioReg->gcdr, GPIO_BIT_OFFSET (GpioPin));
      break;

    case GPIO_MODE_OUTPUT_0:
      // Set as output
      MmioWrite32 ((UINTN)&GpioReg->gsdr, GPIO_BIT_OFFSET (GpioPin));
      // Set output to low
      MmioWrite32 ((UINTN)&GpioReg->gpcr, GPIO_BIT_OFFSET (GpioPin));
      break;

    case GPIO_MODE_OUTPUT_1:
      // Set as output
      MmioWrite32 ((UINTN)&GpioReg->gsdr, GPIO_BIT_OFFSET (GpioPin));
      // Set output to high
      MmioWrite32 ((UINTN)&GpioReg->gpsr, GPIO_BIT_OFFSET (GpioPin));
      break;

    default:
      DEBUG (
             (DEBUG_ERROR, "K3 GPIO: Unsupported mode %d for GPIO %d\n", Mode,
              GpioPin)
             );
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
 * Get GPIO mode
 *
 * @param[in]  This          GPIO protocol instance
 * @param[in]  Gpio          GPIO pin number
 * @param[out] Mode          GPIO mode
 *
 * @retval EFI_SUCCESS       Operation successful
 * @retval EFI_INVALID_PARAMETER Invalid parameter
 */
STATIC
EFI_STATUS
EFIAPI
K3GpioGetMode (
  IN EMBEDDED_GPIO        *This,
  IN EMBEDDED_GPIO_PIN    Gpio,
  OUT EMBEDDED_GPIO_MODE  *Mode
  )
{
  K3_GPIO_REGISTERS  *GpioReg;
  UINTN              GpioPin;
  UINT32             DirValue;
  UINT32             LevelValue;

  if (Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GpioPin = (UINTN)Gpio;
  if (GpioPin >= K3_MAX_GPIO) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Invalid GPIO %d\n", GpioPin));
    return EFI_INVALID_PARAMETER;
  }

  GpioReg = GetGpioBase (GPIO_GROUP (GpioPin));
  if (GpioReg == NULL) {
    return EFI_DEVICE_ERROR;
  }

  // Read direction register
  DirValue = MmioRead32 ((UINTN)&GpioReg->gpdr);

  if (!(DirValue & GPIO_BIT_OFFSET (GpioPin))) {
    // Input mode
    *Mode = GPIO_MODE_INPUT;
  } else {
    // Output mode, read level
    LevelValue = MmioRead32 ((UINTN)&GpioReg->gplr);

    if (LevelValue & GPIO_BIT_OFFSET (GpioPin)) {
      // Output high
      *Mode = GPIO_MODE_OUTPUT_1;
    } else {
      // Output low
      *Mode = GPIO_MODE_OUTPUT_0;
    }
  }

  return EFI_SUCCESS;
}

/**
 * Set GPIO pull-up/pull-down
 *
 * @param[in]  This          GPIO protocol instance
 * @param[in]  Gpio          GPIO pin number
 * @param[in]  Pull          GPIO pull-up/pull-down
 *
 * @retval EFI_SUCCESS       Operation successful
 * @retval EFI_INVALID_PARAMETER Invalid parameter
 * @retval EFI_UNSUPPORTED   Unsupported on this hardware
 */
STATIC
EFI_STATUS
EFIAPI
K3GpioSetPull (
  IN EMBEDDED_GPIO       *This,
  IN EMBEDDED_GPIO_PIN   Gpio,
  IN EMBEDDED_GPIO_PULL  Pull
  )
{
  UINT32  GpioPin;
  UINT32  PullMode;

  GpioPin = (UINT32)Gpio;
  (VOID)This;
  if (GpioPin >= K3_MAX_GPIO) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Invalid GPIO %d\n", GpioPin));
    return EFI_INVALID_PARAMETER;
  }

  switch (Pull) {
    case GPIO_PULL_UP:
      PullMode = K3_GPIO_PIN_PULL_UP;
      break;

    case GPIO_PULL_DOWN:
      PullMode = K3_GPIO_PIN_PULL_DOWN;
      break;

    case GPIO_PULL_NONE:
      PullMode = K3_GPIO_PIN_PULL_NONE;
      break;

    default:
      DEBUG (
             (DEBUG_ERROR, "%a: Unsupported pull operation %d for GPIO %d\n",
              __func__, Pull, GpioPin)
             );
      return EFI_UNSUPPORTED;
  }

  ApplyGpioPinConfig (GpioPin, PullMode);

  return EFI_SUCCESS;
}

/**
 * Driver entry point
 *
 * @param[in]  ImageHandle   Image handle
 * @param[in]  SystemTable   Pointer to system table
 *
 * @retval EFI_SUCCESS       Driver initialized successfully
 * @retval Others            Failed to initialize driver
 */
EFI_STATUS
EFIAPI
K3GpioDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *GpioDevicePath;
  UINTN                     ControllerCount;
  GPIO_INSTANCE             *GpioInstance;

  DEBUG ((DEBUG_INFO, "K3 GPIO: Driver entry point called\n"));
  GpioMmioRemap ();

  // Set controller count
  ControllerCount = PcdGet32 (PcdGpioPinCount);

  DEBUG ((DEBUG_INFO, "K3 GPIO: Controller count: %d\n", ControllerCount));
  DEBUG ((DEBUG_INFO, "K3 GPIO: Pin count: %d\n", K3_MAX_GPIO));

  // Create device path
  GpioDevicePath =
    DuplicateDevicePath ((EFI_DEVICE_PATH_PROTOCOL *)&mDevicePathTemplate);
  if (GpioDevicePath == NULL) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Failed to create device path\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_INFO, "K3 GPIO: Device path created successfully\n"));

  // Allocate GPIO instance
  GpioInstance = AllocateZeroPool (sizeof (GPIO_INSTANCE));
  if (GpioInstance == NULL) {
    DEBUG ((DEBUG_ERROR, "K3 GPIO: Failed to allocate GPIO instance\n"));
    FreePool (GpioDevicePath);
    return EFI_OUT_OF_RESOURCES;
  }

  // Allocate SoC GPIO configuration
  GpioInstance->SoCGpio =
    AllocateZeroPool (sizeof (K3_GPIO_CONTROLLER_CONFIG) * ControllerCount);
  if (GpioInstance->SoCGpio == NULL) {
    DEBUG (
           (DEBUG_ERROR, "K3 GPIO: Failed to allocate SoC GPIO configuration\n")
           );
    FreePool (GpioInstance);
    GpioInstance = NULL;
    FreePool (GpioDevicePath);
    return EFI_OUT_OF_RESOURCES;
  }

  // Initialize GPIO instance
  GpioInstance->Signature       = K3_GPIO_SIGNATURE;
  GpioInstance->GpioDeviceCount = ControllerCount;

  // Initialize SoC GPIO configuration
  GpioInstance->SoCGpio[0].RegisterBase      = K3_GPIO_REG_BASE;
  GpioInstance->SoCGpio[0].InternalGpioCount = K3_MAX_GPIO;

  DEBUG (
         (DEBUG_INFO, "K3 GPIO: Controller[0] base: 0x%lx, pins: %d\n",
          GpioInstance->SoCGpio[0].RegisterBase,
          GpioInstance->SoCGpio[0].InternalGpioCount)
         );

  // Initialize GPIO protocol
  GpioInstance->GpioProtocol.Get     = K3GpioGet;
  GpioInstance->GpioProtocol.Set     = K3GpioSet;
  GpioInstance->GpioProtocol.GetMode = K3GpioGetMode;
  GpioInstance->GpioProtocol.SetPull = K3GpioSetPull;

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID *)&GpioInstance->ClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__,
            Status)
           );
  }

  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "initialize K3 GPIO clock \n"));
    // Init GPIO controller clock and release the reset.
    GpioInstance->ClockCtrlProtocol->SetClockState (
                                                    GpioInstance->ClockCtrlProtocol,
                                                    "GPIO",
                                                    ENABLE_CLOCK
                                                    );

    // Install protocols
    Status = gBS->InstallMultipleProtocolInterfaces (
                                                     &(GpioInstance->Handle),
                                                     &gEmbeddedGpioProtocolGuid,
                                                     &(GpioInstance->GpioProtocol),
                                                     &gEfiDevicePathProtocolGuid,
                                                     GpioDevicePath,
                                                     NULL
                                                     );
  }

  if (!EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_INFO,
            "K3 GPIO: Protocols installed successfully on handle 0x%p\n",
            GpioInstance->Handle)
           );
    DEBUG ((DEBUG_INFO, "K3 GPIO: Driver initialized successfully\n"));
    return EFI_SUCCESS;
  } else {
    FreePool (GpioInstance->SoCGpio);
    FreePool (GpioInstance);
    GpioInstance = NULL;
    FreePool (GpioDevicePath);

    DEBUG ((DEBUG_ERROR, "K3 GPIO: Failed to install protocols: %r\n", Status));
    return Status;
  }
}
