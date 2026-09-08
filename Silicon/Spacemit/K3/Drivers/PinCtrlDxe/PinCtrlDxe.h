/** @file
 *  Spacemit K3 silicon pin controller driver header.
 *
 *  Copyright (c) 2025, Spacemit Limited. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#ifndef __K3_PINCTRL_DXE_H__
#define __K3_PINCTRL_DXE_H__

  #include <Library/BaseLib.h>
  #include <Library/BoardPinctrlMapLib.h>
  #include <Protocol/PinCtrl.h>

#define K3_PIN_SIGNATURE  SIGNATURE_32('p', 'i', 'n', '3')
#define PINCTRL_INSTANCE_FROM_THIS(a)                                          \
  CR(a, PINCTRL_INSTANCE, PinCtrlProtocol, K3_PIN_SIGNATURE)

#define PINCTRL_MAX_REGISTERED_CONTROLLERS  32U
#define PINCTRL_MAX_ACTIVE_STATE_ENTRIES    64U
#define PINCTRL_MAX_STATE_INDEX_ENTRIES     256U

#define K3_MFPR_BASE  (FixedPcdGet64(PcdSpacemitMFPRRegBase))

typedef struct {
  BOOLEAN       Valid;
  EFI_HANDLE    ControllerHandle;
  CHAR8         ControllerType[PINCTRL_CONTROLLER_TYPE_MAX_LEN + 1];
  UINT32        ControllerId;
} PINCTRL_CONTROLLER_MAP_ENTRY;

typedef struct {
  BOOLEAN    Valid;
  CHAR8      ControllerType[PINCTRL_CONTROLLER_TYPE_MAX_LEN + 1];
  UINT32     ControllerId;
  CHAR8      FunctionName[PINCTRL_NAME_MAX_LEN + 1];
  CHAR8      StateName[PINCTRL_NAME_MAX_LEN + 1];
} PINCTRL_ACTIVE_STATE_ENTRY;

typedef struct {
  CONST PINCTRL_DEVICE_DESC      *Device;
  CONST PINCTRL_FUNCTION_DESC    *Function;
  CONST PINCTRL_STATE_DESC       *State;
} PINCTRL_STATE_INDEX_ENTRY;

typedef struct {
  UINTN                           Signature;
  EFI_HANDLE                      Handle;
  SILICON_PINCTRL_PROTOCOL        PinCtrlProtocol;
  CONST PINCTRL_BOARD_MAP         *BoardMap;
  UINTN                           StateIndexCount;
  PINCTRL_STATE_INDEX_ENTRY       StateIndex[PINCTRL_MAX_STATE_INDEX_ENTRIES];
  PINCTRL_CONTROLLER_MAP_ENTRY    ControllerMap[PINCTRL_MAX_REGISTERED_CONTROLLERS];
  PINCTRL_ACTIVE_STATE_ENTRY      ActiveStates[PINCTRL_MAX_ACTIVE_STATE_ENTRIES];
} PINCTRL_INSTANCE;

/* K3 pinctrl power domain registers */
#define K3_IOPWRDOM_BASE        (K3_MFPR_BASE + 0x800)
#define K3_APBC_ASFAR           (FixedPcdGet64(PcdSpacemitApbcAsfarRegBase))
#define K3_APBC_AKEY_ASFAR      0xBABAU
#define K3_APBC_AKEY_ASSAR      0xEB10U
#define K3_IO_PWR_DOMAIN_3V3EN  0U
#define K3_IO_PWR_DOMAIN_1V8EN  (1U << 2)

#define K3_AIB_GPIO1_IO_REG  0x4U
#define K3_AIB_GPIO2_IO_REG  0xCU
#define K3_AIB_GPIO4_IO_REG  0x20U
#define K3_AIB_GPIO5_IO_REG  0x10U
#define K3_AIB_SD_IO_REG     0x1CU
#define K3_AIB_QSPI_IO_REG   0x2CU

/* pin offset */
#define PIN_CONFIG_REG_OFFSET(x)  (K3_MFPR_BASE + (x) * 4)
#define MAX_PIN_NUMBER  (155)

/* pin mux */
#define PIN_MUX_SHIFT  0
#define PIN_MUX_MODE0  0
#define PIN_MUX_MODE1  1
#define PIN_MUX_MODE2  2
#define PIN_MUX_MODE3  3
#define PIN_MUX_MODE4  4
#define PIN_MUX_MODE5  5
#define PIN_MUX_MODE6  6
#define PIN_MUX_MODE7  7

/*
 * drive strength
 * DRIVE[3:0] -> bits[12:9]
 */
#define PIN_DRIVE_SHIFT  (9)
#define PIN_DS0          (0) /* bit[12:9] 0000 */
#define PIN_DS1          (1) /* bit[12:9] 0001 */
#define PIN_DS2          (2) /* bit[12:9] 0010 */
#define PIN_DS3          (3) /* bit[12:9] 0011 */
#define PIN_DS4          (4) /* bit[12:9] 0100 */
#define PIN_DS5          (5) /* bit[12:9] 0101 */
#define PIN_DS6          (6) /* bit[12:9] 0110 */
#define PIN_DS7          (7) /* bit[12:9] 0111 */
#define PIN_DS8          (8) /* bit[12:9] 1000 */
#define PIN_DS9          (9) /* bit[12:9] 1001 */
#define PIN_DS10         (10)/* bit[12:9] 1010 */
#define PIN_DS11         (11)/* bit[12:9] 1011 */
#define PIN_DS12         (12)/* bit[12:9] 1100 */
#define PIN_DS13         (13)/* bit[12:9] 1101 */
#define PIN_DS14         (14)/* bit[12:9] 1110 */
#define PIN_DS15         (15)/* bit[12:9] 1111 */

/* strong pull resistor */
#define SPU_EN  (1 << 3)

/* edge detect */
#define PIN_EDGE_DETECT_SHIFT  (4)
#define PIN_EDGE_NONE          (1 << 6)
#define PIN_EDGE_RISE          (1 << 4)
#define PIN_EDGE_FALL          (1 << 5)
#define PIN_EDGE_BOTH          (3 << 4)

/* slew rate output control */
#define PIN_SLE_EN  (1 << 7)

/* schmitter trigger input threshhold */
#define PIN_ST_EN  (1 << 8)

/* pull up/down */
#define PIN_PULL_SHIFT  (13)
#define PIN_PULL_DIS    (0 << 13)/* bit[15:13] 000 */
#define PIN_PULL_UP     (6 << 13)/* bit[15:13] 110 */
#define PIN_PULL_DOWN   (5 << 13)/* bit[15:13] 101 */

#endif /* __K3_PINCTRL_DXE_H__ */
