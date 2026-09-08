/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/
#include <Library/SpacemitDpu.h>
#include "SpacemitDsiCommon.h"

#define UNLOCK_DELAY  0

SPACEMIT_MODE_INFO  gx09inx101_spacemit_modelist[] = {
  {
    .Name           = "1200x1920-60",
    .Refresh        = 60,
    .XRes           = 1200,
    .YRes           = 1920,
    .RealXRes       = 1200,
    .RealYRes       = 1920,
    .LeftMargin     = 40,
    .RightMargin    = 80,
    .HsyncLen       = 10,
    .UpperMargin    = 16,
    .LowerMargin    = 20,
    .VsyncLen       = 4,
    .HsyncInvert    = 0,
    .VsyncInvert    = 0,
    .InvertPixclock = 0,
    .PixclockFreq   = 156*1000,
    .PixFmtOut      = OUTFMT_RGB888,
    .Width          = 142,
    .Height         = 228,
  }
};

SPACEMIT_MIPI_INFO  gx09inx101_mipi_info = {
  .Height = 1920,
  .Width  = 1200,
  .Hfp    = 80, /* unit: pixel */
  .Hbp    = 40,
  .Hsync  = 10,
  .Vfp    = 20, /* unit: line */
  .Vbp    = 16,
  .Vsync  = 4,
  .Fps    = 60,

  .WorkMode    = SPACEMIT_DSI_MODE_VIDEO, /*command_mode, video_mode*/
  .RgbMode     = DSI_INPUT_DATA_RGB_MODE_888,
  .LaneNumber  = 4,
  .PhyBitClock = 614400000,
  .PhyEscClock = 51200000,
  .SplitEnable = 0,
  .EotpEnable  = 0,

  .BurstMode   = DSI_BURST_MODE_BURST,
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_set_id_cmds[] = {
  {
    SPACEMIT_DSI_SET_MAX_PKT_SIZE, SPACEMIT_DSI_LP_MODE, UNLOCK_DELAY, 1,{
      0x01
    }
  },
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_read_id_cmds[] = {
  {
    SPACEMIT_DSI_GENERIC_READ1, SPACEMIT_DSI_LP_MODE, UNLOCK_DELAY, 1,{
      0xfb
    }
  },
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_set_power_cmds[] = {
  {
    SPACEMIT_DSI_SET_MAX_PKT_SIZE, SPACEMIT_DSI_HS_MODE, UNLOCK_DELAY, 1,{
      0x1
    }
  },
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_read_power_cmds[] = {
  {
    SPACEMIT_DSI_GENERIC_READ1, SPACEMIT_DSI_HS_MODE, UNLOCK_DELAY, 1,{
      0xA
    }
  },
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_init_cmds[] = {
  // 8279 + INX10.1
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xB0, 0x01
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC3, 0x4F
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC4, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC5, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC6, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC7, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC8, 0x4D
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC9, 0x52
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCA, 0x51
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCD, 0x5D
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCE, 0x5B
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCF, 0x4B
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD0, 0x49
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD1, 0x47
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD2, 0x45
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD3, 0x41
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD7, 0x50
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD8, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD9, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDA, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDB, 0x40
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDC, 0x4E
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDD, 0x52
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDE, 0x51
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE1, 0x5E
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE2, 0x5C
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE3, 0x4C
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE4, 0x4A
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE5, 0x48
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE6, 0x46
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE7, 0x42
    }
  },
  // Page0x03
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xB0, 0x03
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xBE, 0x03
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCC, 0x44
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC8, 0x07
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC9, 0x05
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCA, 0x42
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCD, 0x3E
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCF, 0x60
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD2, 0x04
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD3, 0x04
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD4, 0x01
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD5, 0x00
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD6, 0x03
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD7, 0x04
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD9, 0x01
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDB, 0x01
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE4, 0xF0
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE5, 0x0A
    }
  },
  // Page0x00
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xB0, 0x00
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xBD, 0x50
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC2, 0x08
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC4, 0x10
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCC, 0x00
    }
  },
  // Page0x02
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xB0, 0x02
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC0, 0x00
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC1, 0x0A
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC2, 0x20
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC3, 0x24
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC4, 0x23
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC5, 0x29
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC6, 0x23
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC7, 0x1C
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC8, 0x19
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xC9, 0x17
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCA, 0x17
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCB, 0x18
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCC, 0x1A
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCD, 0x1E
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCE, 0x20
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xCF, 0x23
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD0, 0x07
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD1, 0x00
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD2, 0x00
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD3, 0x0A
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD4, 0x13
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD5, 0x1C
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD6, 0x1A
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD7, 0x13
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD8, 0x17
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xD9, 0x1C
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDA, 0x19
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDB, 0x17
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDC, 0x17
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDD, 0x18
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDE, 0x1A
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xDF, 0x1E
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE0, 0x20
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE1, 0x23
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 0, 2,{
      0xE2, 0x07
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 200, 2,{
      0x11, 0x00
    }
  },
  {
    SPACEMIT_DSI_DCS_LWRITE, SPACEMIT_DSI_LP_MODE, 50, 2,{
      0x29, 0x00
    }
  },
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_sleep_out_cmds[] = {
  {
    SPACEMIT_DSI_DCS_SWRITE, SPACEMIT_DSI_LP_MODE, 200, 1,{
      0x11
    }
  },
  {
    SPACEMIT_DSI_DCS_SWRITE, SPACEMIT_DSI_LP_MODE, 50, 1,{
      0x29
    }
  },
};

static SPACEMIT_DSI_CMD_DESC  gx09inx101_sleep_in_cmds[] = {
  {
    SPACEMIT_DSI_DCS_SWRITE, SPACEMIT_DSI_LP_MODE, 50, 1,{
      0x28
    }
  },
  {
    SPACEMIT_DSI_DCS_SWRITE, SPACEMIT_DSI_LP_MODE, 200, 1,{
      0x10
    }
  },
};

LCD_MIPI_PANEL_INFO  lcd_gx09inx101 = {
  .LcdName          = "gx09inx101",
  .LcdId            = 0x8279,
  .PanelId0         = 0x1,
  .PowerValue       = 0x14,
  .PanelType        = LCD_MIPI,
  .WidthMm          = 142,
  .HeightMm         = 228,
  .DftPwmBl         = 128,
  .SetIdCmdsNum     = ARRAY_SIZE (gx09inx101_set_id_cmds),
  .ReadIdCmdsNum    = ARRAY_SIZE (gx09inx101_read_id_cmds),
  .InitCmdsNum      = ARRAY_SIZE (gx09inx101_init_cmds),
  .SetPowerCmdsNum  = ARRAY_SIZE (gx09inx101_set_power_cmds),
  .ReadPowerCmdsNum = ARRAY_SIZE (gx09inx101_read_power_cmds),
  .SleepOutCmdsNum  = ARRAY_SIZE (gx09inx101_sleep_out_cmds),
  .SleepInCmdsNum   = ARRAY_SIZE (gx09inx101_sleep_in_cmds),
  .SpacemitModeInfo = gx09inx101_spacemit_modelist,
  .MipiInfo         = &gx09inx101_mipi_info,
  .SetIdCmds        = gx09inx101_set_id_cmds,
  .ReadIdCmds       = gx09inx101_read_id_cmds,
  .SetPowerCmds     = gx09inx101_set_power_cmds,
  .ReadPowerCmds    = gx09inx101_read_power_cmds,
  .InitCmds         = gx09inx101_init_cmds,
  .SleepOutCmds     = gx09inx101_sleep_out_cmds,
  .SleepInCmds      = gx09inx101_sleep_in_cmds,
  .BitclkSel        = 3,
  .BitclkDiv        = 1,
  .PxclkSel         = 2,
  .PxclkDiv         = 6,
};

EFI_STATUS
LcdGx09inx101Init (
  void
  )
{
  EFI_STATUS  Status;

  Status = LcdMipiRegisterPanel (&lcd_gx09inx101);
  return Status;
}
