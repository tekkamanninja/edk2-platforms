/**
 *
 *  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __K3_GPIO_H__
#define __K3_GPIO_H__

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/EmbeddedGpio.h>
#include <Uefi/UefiBaseType.h>

#include <Protocol/ClockCtrl.h>

#define K3_GPIO_SIGNATURE  SIGNATURE_64('K', '3', '_', 'G', 'P', 'I', 'O', ' ')
#define GPIO_INSTANCE_FROM_THIS(a)                                             \
  CR(a, GPIO_INSTANCE, GpioProtocol, K3_GPIO_SIGNATURE)

#define K3_GPIO_REG_BASE  (FixedPcdGet64(PcdGpioControllerBase))
#define K3_MFPR_BASE      (FixedPcdGet64(PcdSpacemitMFPRRegBase))

#ifndef K3_MAX_GPIO
#define K3_MAX_GPIO  (FixedPcdGet32(PcdGpioPinCount))
#endif

#define K3_GPIO_PIN_CFG_REG(gp)  (K3_MFPR_BASE + (UINTN)(gp) * sizeof(UINT32))
#define K3_GPIO_PINMUX_MASK        0x7U
#define K3_GPIO_PINMUX_FUNC0       0U
#define K3_GPIO_PIN_DRIVE_SHIFT    9U
#define K3_GPIO_PIN_DRIVE_MASK     0xFU
#define K3_GPIO_PIN_DRIVE_DEFAULT  3U
#define K3_GPIO_PIN_EDGE_NONE      (1U << 6)
#define K3_GPIO_PIN_PULL_SHIFT     13U
#define K3_GPIO_PIN_PULL_MASK      (7U << K3_GPIO_PIN_PULL_SHIFT)
#define K3_GPIO_PIN_PULL_NONE      (0U << K3_GPIO_PIN_PULL_SHIFT)
#define K3_GPIO_PIN_PULL_DOWN      (5U << K3_GPIO_PIN_PULL_SHIFT)
#define K3_GPIO_PIN_PULL_UP        (6U << K3_GPIO_PIN_PULL_SHIFT)

/**
 * GPIO register structure
 */
typedef struct {
  UINT32    gplr;   // Pin Level Register - 0x00
  UINT32    gpdr;   // Pin Direction Register - 0x04
  UINT32    gpsr;   // Pin Output Set Register - 0x08
  UINT32    gpcr;   // Pin Output Clear Register - 0x0C
  UINT32    grer;   // Rising-Edge Detect Enable Register - 0x10
  UINT32    gfer;   // Falling-Edge Detect Enable Register - 0x14
  UINT32    gedr;   // Edge Detect Status Register - 0x18
  UINT32    gsdr;   // Bitwise Set of GPIO Direction Register - 0x1C
  UINT32    gcdr;   // Bitwise Clear of GPIO Direction Register - 0x20
  UINT32    gsrer;  // Bitwise Set of Rising-Edge Detect Enable Register - 0x24
  UINT32    gcrer;  // Bitwise Clear of Rising-Edge Detect Enable Register - 0x28
  UINT32    gsfer;  // Bitwise Set of Falling-Edge Detect Enable Register - 0x2C
  UINT32    gcfer;  // Bitwise Clear of Falling-Edge Detect Enable Register - 0x30
  UINT32    apmask; // Bitwise Mask of Edge Detect Register - 0x34
  UINT32    cpmask; // Bitwise Mask of Edge Detect Register - 0x38
} K3_GPIO_REGISTERS;

// GPIO group and bit calculation methods
#define GPIO_GROUP(gp)  ((gp) >> 5)// Divide by 32 to get group number
#define GPIO_BIT_OFFSET(gp)                                                    \
  (1 << ((gp) & 0x1f)) // Get low 5 bits for bit offset

// GPIO register group offset definitions
#define K3_GPIO_GROUP_COUNT    4
#define K3_GPIO_GROUP0_OFFSET  0x00
#define K3_GPIO_GROUP1_OFFSET  0x40
#define K3_GPIO_GROUP2_OFFSET  0x80
#define K3_GPIO_GROUP3_OFFSET  0x100

// GPIO controller configuration structure
typedef struct {
  UINTN    RegisterBase;      // Register base address
  UINTN    InternalGpioCount; // Number of GPIO pins
} K3_GPIO_CONTROLLER_CONFIG;

typedef struct {
  UINT64                        Signature;
  EFI_HANDLE                    Handle;
  EMBEDDED_GPIO                 GpioProtocol;
  K3_GPIO_CONTROLLER_CONFIG     *SoCGpio;
  UINTN                         GpioDeviceCount;

  SILICON_CLOCKCTRL_PROTOCOL    *ClockCtrlProtocol;
} GPIO_INSTANCE;

// K3 GPIO device GUID
#define K3_GPIO_DEVICE_GUID                                                    \
  {0x9A2E94BB, 0xFCA1, 0x4D42, {0x92, 0x22, 0x6D, 0xD3, 0xC4, 0xB4, 0x08, 0x80}}

/**
 * Function declarations
 */

/**
 * Get GPIO register base address
 *
 * @param[in]  Bank          GPIO bank number
 *
 * @retval     Register base address, or NULL if Bank is invalid
 */
STATIC
K3_GPIO_REGISTERS *
GetGpioBase (
  IN UINTN  Bank
  );

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
  );

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
  );

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
  );

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
  );

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
  );

#endif // __K3_GPIO_H__
