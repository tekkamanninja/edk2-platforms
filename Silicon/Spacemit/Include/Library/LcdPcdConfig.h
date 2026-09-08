/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_LCD_PCD_CONFIG_H_
#define _SPACEMIT_LCD_PCD_CONFIG_H_

#include "Library/SpacemitDpu.h"

#define VOP_OUTPUT_IF_NUMS  2

#define MAX_CONNECTOR_NAME_LEN  32

typedef struct {
  DPU_MODES    Mode;  // DPU_MODES
  DPU_ID       DpuId;
  UINT32       PinGroup;
} DISPLAY_ORDER;

typedef struct {
  UINT32           DisplayOrderCount;
  DISPLAY_ORDER    DisplayOrder[0];
} DISPLAY_CONNECTORS_PRIORITY_VARSTORE_DATA;

typedef struct {
  CHAR8    PanelName[MAX_CONNECTOR_NAME_LEN];
} DISPLAY_PANEL_NAME;

typedef struct {
  UINT32     GpioPin;
  BOOLEAN    ActiveState;
} LCD_GPIO_CONFIG;

typedef struct {
  UINT16             Num;
  LCD_GPIO_CONFIG    DcpGpio;
  LCD_GPIO_CONFIG    DcnGpio;
  LCD_GPIO_CONFIG    ResetGpio;
  LCD_GPIO_CONFIG    BlGpio;
  LCD_GPIO_CONFIG    EnableGpio;
  UINT32             PixClk;
  UINT32             EscClk;
  UINT32             DscClk;
  UINT32             AClk;
  UINT32             MClk;
} LCD_CONFIG_ARRAY;

#endif // _SPACEMIT_LED_PCD_CONFIG_H_
