/** @file
 *
 *  Provide API to access silicon clock controller.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __SPACEMIT_SILICON_CLOCK_CTRL_PROTOCOL_H__
#define __SPACEMIT_SILICON_CLOCK_CTRL_PROTOCOL_H__

#define SPACEMIT_SILICON_CLOCK_CTRL_PROTOCOL_GUID \
  { 0x4581D44A, 0x4DC4, 0x43C0, { 0x9A, 0x6E, 0x46, 0x27, 0xB7, 0x24, 0x3D, 0x6C } }

typedef struct _SILICON_CLOCKCTRL_PROTOCOL SILICON_CLOCKCTRL_PROTOCOL;

typedef enum {
  CLOCK_DISABLED = 0,
  CLOCK_ENABLED  = 1
} _SPACEMIT_SILICON_CLOCK_STATE;

typedef enum {
  DISABLE_CLOCK = 0,
  ENABLE_CLOCK  = 1
} _SPACEMIT_SILICON_CLOCK_OP;

typedef EFI_STATUS
(*GET_CLOCK_STATE) (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  OUT UINT32                     *ClockState
  );

typedef EFI_STATUS
(*SET_CLOCK_STATE) (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  IN  UINT32                     ClockState
  );

typedef EFI_STATUS
(*GET_CLOCK_RATE) (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  OUT UINT64                     *ClockRate
  );

typedef EFI_STATUS
(*SET_CLOCK_RATE) (
  IN SILICON_CLOCKCTRL_PROTOCOL  *This,
  IN  CONST CHAR8                *ClockName,
  OUT UINT64                     ClockRate
  );

struct _SILICON_CLOCKCTRL_PROTOCOL {
  GET_CLOCK_STATE    GetClockState;
  SET_CLOCK_STATE    SetClockState;
  GET_CLOCK_RATE     GetClockRate;
  GET_CLOCK_RATE     GetMaxClockRate;
  GET_CLOCK_RATE     GetMinClockRate;
  SET_CLOCK_RATE     SetClockRate;
};

extern EFI_GUID  gSpacemitSiliconClockCtrlProtocolGuid;

#endif /* __SPACEMIT_SILICON_CLOCK_CTRL_PROTOCOL_H__ */
