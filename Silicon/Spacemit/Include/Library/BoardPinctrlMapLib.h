/** @file
  Board-level pinctrl state binding declarations.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _BOARD_PINCTRL_MAP_LIB_H_
#define _BOARD_PINCTRL_MAP_LIB_H_

#include <Base.h>
#include <Protocol/PinCtrl.h>

typedef struct {
  CONST CHAR8    *GroupName;
  UINT8          Order;
  BOOLEAN        HasVoltageOverride;
  UINT16         VoltageMv;
} PINCTRL_STATE_GROUP_REF;

typedef struct {
  CONST CHAR8                       *StateName;
  UINT8                             GroupCount;
  CONST PINCTRL_STATE_GROUP_REF     *Groups;
} PINCTRL_STATE_DESC;

typedef struct {
  CONST CHAR8                  *FunctionName;
  UINT8                        StateCount;
  CONST PINCTRL_STATE_DESC     *States;
} PINCTRL_FUNCTION_DESC;

typedef struct {
  CONST CHAR8                     *ControllerType;
  UINT32                          ControllerId;
  UINT8                           FunctionCount;
  CONST PINCTRL_FUNCTION_DESC     *Functions;
} PINCTRL_DEVICE_DESC;

typedef struct {
  UINT16                        DeviceCount;
  CONST PINCTRL_DEVICE_DESC     *Devices;
} PINCTRL_BOARD_MAP;

//
// PinCtrl board map DSL helpers.
// These macros eliminate manual count maintenance by deriving sizes from
// compound literals at compile time.
//
#define PC_GROUP(GroupName, Order, HasVoltageOverride, VoltageMv)  \
  {                                                                 \
    (GroupName),                                                    \
    (UINT8)(Order),                                                 \
    (HasVoltageOverride),                                           \
    (UINT16)(VoltageMv)                                             \
  }

#define PC_STATE(StateName, ...)                                                                    \
  {                                                                                                 \
    (StateName),                                                                                    \
    (UINT8)(sizeof ((PINCTRL_STATE_GROUP_REF[]){ __VA_ARGS__ }) / sizeof (PINCTRL_STATE_GROUP_REF)), \
    (CONST PINCTRL_STATE_GROUP_REF[]){ __VA_ARGS__ }                                               \
  }

#define PC_FUNCTION(FunctionName, ...)                                                            \
  {                                                                                               \
    (FunctionName),                                                                               \
    (UINT8)(sizeof ((PINCTRL_STATE_DESC[]){ __VA_ARGS__ }) / sizeof (PINCTRL_STATE_DESC)),      \
    (CONST PINCTRL_STATE_DESC[]){ __VA_ARGS__ }                                                   \
  }

#define PC_DEVICE(ControllerType, ControllerId, ...)                                             \
  {                                                                                               \
    (ControllerType),                                                                             \
    (UINT32)(ControllerId),                                                                       \
    (UINT8)(sizeof ((PINCTRL_FUNCTION_DESC[]){ __VA_ARGS__ }) / sizeof (PINCTRL_FUNCTION_DESC)), \
    (CONST PINCTRL_FUNCTION_DESC[]){ __VA_ARGS__ }                                                \
  }

#define PC_DEVICE_SIMPLE(ControllerType, ControllerId, GroupName)  \
  PC_DEVICE (                                                       \
    (ControllerType),                                               \
    (ControllerId),                                                 \
    PC_FUNCTION (                                                   \
      "default",                                                    \
      PC_STATE (                                                    \
        PINCTRL_STATE_DEFAULT,                                      \
        PC_GROUP ((GroupName), 0, FALSE, 0)                        \
        )                                                           \
      )                                                             \
    )

/**
  Return board-level pinctrl mapping table.

  @param[out]  BoardMap      Mapping table.

  @retval EFI_SUCCESS         Mapping table available.
  @retval EFI_NOT_FOUND       Board does not provide a map.
  @retval EFI_INVALID_PARAMETER  BoardMap is NULL.
**/
EFI_STATUS
EFIAPI
BoardPinctrlGetMap (
  OUT CONST PINCTRL_BOARD_MAP  **BoardMap
  );

#endif // _BOARD_PINCTRL_MAP_LIB_H_
