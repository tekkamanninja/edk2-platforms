/** @file
  USB controller resource configuration definitions.

  Copyright (c) 2025, Spacemit Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _USBHOST_PCD_CONFIG_H_
#define _USBHOST_PCD_CONFIG_H_

#include <Protocol/ClockCtrl.h>

#define USB_HOST_MAX_CONTROLLERS  5

typedef enum {
  USB_SPEED_HIGH  = 2,
  USB_SPEED_SUPER = 3,
} USB_MAX_SPEED;

//
// Fixed hardware description of a USB controller (addresses, PHY, clock).
// Shared across all boards using the same SoC.
// MaxSpeed is set to max capability (3=SS); runtime filtering
// is done via PcdUsbHostEnableMask / PcdUsbHostHsOnlyMask dynamic PCDs.
//
#pragma pack(1)
typedef struct {
  BOOLEAN    IsCombo;
  BOOLEAN    HsForceDisableU3Phy;
  UINT8      PhySelBit;
  UINT8      MaxSpeed;
  CHAR8      ClockResetName[20];
  UINT64     ControllerBase;
  UINT64     UtmiPhyBase;
  UINT64     PipePhyBase;
  UINT64     PipePhy1Base;
} USB_HOST_CONTROLLER_HW;
#pragma pack()

typedef struct {
  UINT16                    Num;
  USB_HOST_CONTROLLER_HW    Controller[USB_HOST_MAX_CONTROLLERS];
} USB_HOST_CONTROLLER_HW_ARRAY;

typedef struct {
  UINT32     Gpio;
  BOOLEAN    ActiveLow;
} USB_VBUS_PIN;

typedef struct {
  UINT16          Num;
  USB_VBUS_PIN    VbusOrHub[0];
} USB_HOST_VBUS_GPIO_CONFIG_ARRAY;

#endif // _USBHOST_PCD_CONFIG_H_
