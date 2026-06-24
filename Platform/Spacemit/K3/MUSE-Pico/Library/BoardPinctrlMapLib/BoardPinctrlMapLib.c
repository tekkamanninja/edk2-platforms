/** @file
  MUSE-Pico board pinctrl state map.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BoardPinctrlMapLib.h>

STATIC CONST PINCTRL_DEVICE_DESC  mBoardDevices[] = {
  PC_DEVICE_SIMPLE ("i2c", 1, "i2c1_3_grp"),
  PC_DEVICE_SIMPLE ("i2c", 2, "i2c2_1_grp"),
  PC_DEVICE_SIMPLE ("i2c", 6, "i2c6_0_grp"),
  PC_DEVICE_SIMPLE ("qspi", 0, "qspi_grp"),
  PC_DEVICE_SIMPLE ("usb", 0, "usb30_drd_dir-0-cfg"),

  PC_DEVICE ("pcie", 0,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("pcie0_1_grp", 0, TRUE, 3300)),
      PC_STATE ("lowvoltage",
        PC_GROUP ("pcie0_1_grp", 0, TRUE, 1800)),
    )),

  PC_DEVICE ("pcie", 1,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("pcie1_1_grp", 0, TRUE, 3300)),
      PC_STATE ("lowvoltage",
        PC_GROUP ("pcie1_1_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE ("pcie", 2,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("pcie2_1_grp", 0, TRUE, 3300)),
      PC_STATE ("lowvoltage",
        PC_GROUP ("pcie2_1_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE ("pcie", 3,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("pcie3_0_grp", 0, TRUE, 3300)),
      PC_STATE ("lowvoltage",
        PC_GROUP ("pcie3_0_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE ("pcie", 4,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("pcie4_1_grp", 0, TRUE, 3300)),
      PC_STATE ("lowvoltage",
        PC_GROUP ("pcie4_1_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE ("sdhci", 0,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("mmc1_grp", 0, TRUE, 3300)),
      PC_STATE ("uhs",
        PC_GROUP ("mmc1_fast_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE ("gmac", 0,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("gmac0_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE ("gmac", 1,
    PC_FUNCTION ("default",
      PC_STATE (PINCTRL_STATE_DEFAULT,
        PC_GROUP ("gmac1_grp", 0, TRUE, 1800))
    )),

  PC_DEVICE_SIMPLE ("dp", 0, "dp0_1_grp"),
  PC_DEVICE_SIMPLE ("dp", 1, "dp1_1_grp"),
  PC_DEVICE_SIMPLE ("dp", 2, "dp1_3_grp"),
};

STATIC CONST PINCTRL_BOARD_MAP  mBoardMap = {
  (UINT16)(sizeof (mBoardDevices) / sizeof (mBoardDevices[0])),
  mBoardDevices,
};

EFI_STATUS
EFIAPI
BoardPinctrlGetMap (
  OUT CONST PINCTRL_BOARD_MAP  **BoardMap
  )
{
  if (BoardMap == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *BoardMap = &mBoardMap;
  return EFI_SUCCESS;
}
