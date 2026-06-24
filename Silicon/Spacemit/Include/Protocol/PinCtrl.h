/** @file
 *
 *  Provide API to access silicon pin controller.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __SPACEMIT_SILICON_PIN_CTRL_PROTOCOL_H__
#define __SPACEMIT_SILICON_PIN_CTRL_PROTOCOL_H__

#include <Uefi.h>

#define SPACEMIT_SILICON_PIN_CTRL_PROTOCOL_GUID \
  { 0xECB8597A, 0xC7A0, 0x4D7F, { 0x85, 0xCB, 0xF3, 0xF1, 0xD4, 0xEA, 0xF3, 0x20 }}

#define SILICON_PINCTRL_PROTOCOL_REVISION  0x00020000U

#define PINCTRL_NAME_MAX_LEN             32U
#define PINCTRL_CONTROLLER_TYPE_MAX_LEN  16U
#define PINCTRL_MAX_GROUPS_PER_STATE     8U

typedef struct _SILICON_PINCTRL_PROTOCOL SILICON_PINCTRL_PROTOCOL;
typedef struct _SILICON_PINCTRL_DEVICE_KEY SILICON_PINCTRL_DEVICE_KEY;

typedef struct {
  UINT8    FunctionSelect;
  UINT8    PadDrive;
  UINT8    EdgeDetect;
  UINT8    Pull;
} PIN_CONFIG;

typedef
EFI_STATUS
(EFIAPI *PINCTRL_SET_PIN_CONFIG) (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN UINT32                    PinId,
  IN CONST PIN_CONFIG          *PinConfig
  );

typedef
EFI_STATUS
(EFIAPI *PINCTRL_SET_PIN_GROUP_BY_NAME) (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN CONST CHAR8               *GroupName
  );

typedef
EFI_STATUS
(EFIAPI *PINCTRL_REGISTER_CONTROLLER) (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN EFI_HANDLE                ControllerHandle,
  IN CONST CHAR8               *ControllerType,
  IN UINT32                    ControllerId
  );

typedef
EFI_STATUS
(EFIAPI *PINCTRL_APPLY_STATE) (
  IN SILICON_PINCTRL_PROTOCOL         *This,
  IN CONST SILICON_PINCTRL_DEVICE_KEY *DeviceKey,
  IN CONST CHAR8                      *StateName OPTIONAL
  );

typedef
EFI_STATUS
(EFIAPI *PINCTRL_APPLY_STATE_BY_ID) (
  IN SILICON_PINCTRL_PROTOCOL  *This,
  IN CONST CHAR8               *ControllerType,
  IN UINT32                    ControllerId,
  IN CONST CHAR8               *FunctionName OPTIONAL,
  IN CONST CHAR8               *StateName OPTIONAL
  );

typedef
EFI_STATUS
(EFIAPI *PINCTRL_GET_ACTIVE_STATE) (
  IN SILICON_PINCTRL_PROTOCOL         *This,
  IN CONST SILICON_PINCTRL_DEVICE_KEY *DeviceKey,
  OUT CONST CHAR8                     **StateName
  );

//
// Generic pinctrl state names used by drivers.
//
#define PINCTRL_STATE_DEFAULT  "default"
#define PINCTRL_STATE_SLEEP    "sleep"

struct _SILICON_PINCTRL_DEVICE_KEY {
  UINT32        Size;
  EFI_HANDLE    ControllerHandle;
  CONST CHAR8   *ControllerType;
  UINT32        ControllerId;
  CONST CHAR8   *FunctionName;
  INT32         FdtNodeOffset;
};

struct _SILICON_PINCTRL_PROTOCOL {
  UINT32                         Revision;
  PINCTRL_SET_PIN_CONFIG         SetPinConfig;
  PINCTRL_SET_PIN_GROUP_BY_NAME  SetPinGroupByName;
  PINCTRL_REGISTER_CONTROLLER    RegisterController;
  PINCTRL_APPLY_STATE            ApplyState;
  PINCTRL_APPLY_STATE_BY_ID      ApplyStateById;
  PINCTRL_GET_ACTIVE_STATE       GetActiveState;
};

extern EFI_GUID  gSpacemitSiliconPinCtrlProtocolGuid;

#endif /* __SPACEMIT_SILICON_PIN_CTRL_PROTOCOL_H__ */
