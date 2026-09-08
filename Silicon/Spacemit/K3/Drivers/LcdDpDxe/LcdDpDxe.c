/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>
#include <Protocol/ClockCtrl.h>
#include <Protocol/PinCtrl.h>
#include <Protocol/EmbeddedGpio.h>
#include <Include/Library/SpacemitDisplayLib.h>
#include <Include/Library/SpacemitDpu.h>
#include <Include/Library/SpacemitDisplayLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/CacheMaintenanceLib.h>
#include "LcdDpDxe.h"
#include "InnoDpReg.h"

STATIC SPACEMIT_INNO_DP_PRIV  *SpacemitDp;
#define PMU_BASE_ADDRESS       (FixedPcdGet64(PcdSpacemitAPMURegBase))
#define K3_DP0_POWER_CTRL      0x380
#define K3_DP1_POWER_CTRL      0x3f4
#define APMU_POWER_STATUS_REG  0xf0
#define DP_CEA_1080P60_VIC     16

/**
 * Set timing entry with single value for min/typ/max
 */
STATIC
VOID
DpSetEntry (
  IN OUT TIMING_ENTRY  *Entry,
  IN UINT32            Value
  )
{
  Entry->Min = Value;
  Entry->Typ = Value;
  Entry->Max = Value;
}

/**
 * Set 1080p60 timing parameters
 */
STATIC
VOID
DpSet1080p60Timing (
  OUT DISPLAY_TIMING  *Timing
  )
{
  ZeroMem (Timing, sizeof (*Timing));

  DpSetEntry (&Timing->Pixelclock, 148500000);
  DpSetEntry (&Timing->Hactive, 1920);
  DpSetEntry (&Timing->Hfront_porch, 88);
  DpSetEntry (&Timing->Hback_porch, 148);
  DpSetEntry (&Timing->Hsync_len, 44);
  DpSetEntry (&Timing->Vactive, 1080);
  DpSetEntry (&Timing->Vfront_porch, 4);
  DpSetEntry (&Timing->Vback_porch, 36);
  DpSetEntry (&Timing->Vsync_len, 5);
  Timing->Flags = DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH;

  Timing->Hsync_start = 1920 + 88;
  Timing->Hsync_end   = Timing->Hsync_start + 44;
  Timing->Htotal      = 1920 + 88 + 44 + 148;
  Timing->Vsync_start = 1080 + 4;
  Timing->Vsync_end   = Timing->Vsync_start + 5;
  Timing->Vtotal      = 1080 + 4 + 5 + 36;
}

/**
 * Decode detailed timing from EDID
 */
STATIC
VOID
DpDecodeDetailedTiming (
  IN CONST EDID_DETAILED_TIMING  *Timing,
  OUT DISPLAY_TIMING             *DisplayTiming
  )
{
  UINT32  Hactive, Hblank, HsyncOffset, HsyncWidth;
  UINT32  Vactive, Vblank, VsyncOffset, VsyncWidth;

  ZeroMem (DisplayTiming, sizeof (*DisplayTiming));

  Hactive     = EDID_DETAILED_TIMING_HORIZONTAL_ACTIVE (*Timing);
  Hblank      = EDID_DETAILED_TIMING_HORIZONTAL_BLANKING (*Timing);
  HsyncOffset = EDID_DETAILED_TIMING_HSYNC_OFFSET (*Timing);
  HsyncWidth  = EDID_DETAILED_TIMING_HSYNC_PULSE_WIDTH (*Timing);
  Vactive     = EDID_DETAILED_TIMING_VERTICAL_ACTIVE (*Timing);
  Vblank      = EDID_DETAILED_TIMING_VERTICAL_BLANKING (*Timing);
  VsyncOffset = EDID_DETAILED_TIMING_VSYNC_OFFSET (*Timing);
  VsyncWidth  = EDID_DETAILED_TIMING_VSYNC_PULSE_WIDTH (*Timing);

  DpSetEntry (
              &DisplayTiming->Pixelclock,
              EDID_DETAILED_TIMING_PIXEL_CLOCK (*Timing)
              );
  DpSetEntry (&DisplayTiming->Hactive, Hactive);
  DpSetEntry (&DisplayTiming->Hfront_porch, HsyncOffset);
  DpSetEntry (
              &DisplayTiming->Hback_porch,
              Hblank - HsyncOffset - HsyncWidth
              );
  DpSetEntry (&DisplayTiming->Hsync_len, HsyncWidth);
  DpSetEntry (&DisplayTiming->Vactive, Vactive);
  DpSetEntry (&DisplayTiming->Vfront_porch, VsyncOffset);
  DpSetEntry (
              &DisplayTiming->Vback_porch,
              Vblank - VsyncOffset - VsyncWidth
              );
  DpSetEntry (&DisplayTiming->Vsync_len, VsyncWidth);

  DisplayTiming->Flags = 0;

  if (EDID_DETAILED_TIMING_FLAG_HSYNC_POLARITY (*Timing)) {
    DisplayTiming->Flags |= DISPLAY_FLAGS_HSYNC_HIGH;
  } else {
    DisplayTiming->Flags |= DISPLAY_FLAGS_HSYNC_LOW;
  }

  if (EDID_DETAILED_TIMING_FLAG_VSYNC_POLARITY (*Timing)) {
    DisplayTiming->Flags |= DISPLAY_FLAGS_VSYNC_HIGH;
  } else {
    DisplayTiming->Flags |= DISPLAY_FLAGS_VSYNC_LOW;
  }

  if (EDID_DETAILED_TIMING_FLAG_INTERLACED (*Timing)) {
    DisplayTiming->Flags |= DISPLAY_FLAGS_INTERLACED;
  }

  DisplayTiming->Hsync_start = Hactive + HsyncOffset;
  DisplayTiming->Hsync_end   = DisplayTiming->Hsync_start + HsyncWidth;
  DisplayTiming->Htotal      = Hactive + Hblank;
  DisplayTiming->Vsync_start = Vactive + VsyncOffset;
  DisplayTiming->Vsync_end   = DisplayTiming->Vsync_start + VsyncWidth;
  DisplayTiming->Vtotal      = Vactive + Vblank;

  DEBUG (
         (DEBUG_INFO, "DpDecodeDetailedTiming: Hactive=%d, Hblank=%d, HsyncOffset=%d, HsyncWidth=%d\n",
          Hactive, Hblank, HsyncOffset, HsyncWidth)
         );
  DEBUG (
         (DEBUG_INFO, "DpDecodeDetailedTiming: Vactive=%d, Vblank=%d, VsyncOffset=%d, VsyncWidth=%d\n",
          Vactive, Vblank, VsyncOffset, VsyncWidth)
         );
  DEBUG (
         (DEBUG_INFO, "DpDecodeDetailedTiming: Pixelclock=%d, Flags=0x%x\n",
          DisplayTiming->Pixelclock.Typ, DisplayTiming->Flags)
         );
}

/**
 * Calculate vertical refresh rate from timing
 */
STATIC
UINT32
DpCalcVrefresh (
  IN CONST DISPLAY_TIMING  *Timing
  )
{
  UINT32  Htotal, Vtotal;
  UINT64  Refresh;

  Htotal = Timing->Hactive.Typ + Timing->Hfront_porch.Typ +
           Timing->Hback_porch.Typ + Timing->Hsync_len.Typ;
  Vtotal = Timing->Vactive.Typ + Timing->Vfront_porch.Typ +
           Timing->Vback_porch.Typ + Timing->Vsync_len.Typ;

  if ((Htotal == 0) || (Vtotal == 0)) {
    return 0;
  }

  Refresh  = (UINT64)Timing->Pixelclock.Typ;
  Refresh += (UINT64)Htotal * Vtotal / 2;
  Refresh /= (UINT64)Htotal * Vtotal;

  return (UINT32)Refresh;
}

/**
 * Check if timing matches 1080p60
 */
STATIC
BOOLEAN
DpIs1080p60Timing (
  IN CONST DISPLAY_TIMING  *Timing
  )
{
  UINT32  Refresh = DpCalcVrefresh (Timing);

  return (Timing->Hactive.Typ == 1920) &&
         (Timing->Vactive.Typ == 1080) &&
         !(Timing->Flags & DISPLAY_FLAGS_INTERLACED) &&
         (Refresh >= 59) && (Refresh <= 61);
}

/**
 * Find 1080p60 DTD in detailed timing list
 */
STATIC
BOOLEAN
DpFind1080p60Dtd (
  IN CONST EDID_DETAILED_TIMING  *Dtd,
  IN INT32                       Count,
  OUT DISPLAY_TIMING             *Timing
  )
{
  INT32           i;
  DISPLAY_TIMING  Tmp;

  for (i = 0; i < Count; i++) {
    if (EDID_DETAILED_TIMING_PIXEL_CLOCK (Dtd[i]) == 0) {
      continue;
    }

    DpDecodeDetailedTiming (&Dtd[i], &Tmp);
    if (!DpIs1080p60Timing (&Tmp)) {
      continue;
    }

    CopyMem (Timing, &Tmp, sizeof (*Timing));
    return TRUE;
  }

  return FALSE;
}

/**
 * Find 1080p60 from CEA VDB
 */
STATIC
BOOLEAN
DpFind1080p60CeaVdb (
  IN CONST EDID_CEA861_EXTENSION  *Cea,
  OUT DISPLAY_TIMING              *Timing
  )
{
  INT32  Offset = Cea->DtdOffset;
  INT32  DataLen;
  INT32  i;

  if ((Offset < 4) || (Offset > EDID_SIZE)) {
    return FALSE;
  }

  DataLen = Offset - 4;
  for (i = 0; i < DataLen;) {
    INT32  Len  = EDID_CEA861_DB_LEN (*Cea, i);
    INT32  Type = EDID_CEA861_DB_TYPE (*Cea, i);
    INT32  j;

    if (i + Len >= DataLen) {
      break;
    }

    if (Type == EDID_CEA861_DB_VIDEO) {
      for (j = 0; j < Len; j++) {
        UINT8  Svd = Cea->Data[i + 1 + j];

        if ((Svd & 0x7F) != DP_CEA_1080P60_VIC) {
          continue;
        }

        DpSet1080p60Timing (Timing);
        return TRUE;
      }
    }

    i += Len + 1;
  }

  return FALSE;
}

/**
 * Find 1080p60 timing from EDID buffer
 */
STATIC
BOOLEAN
DpFind1080p60FromEdid (
  IN CONST UINT8      *Buf,
  IN INT32            BufSize,
  OUT DISPLAY_TIMING  *Timing
  )
{
  CONST EDID_MONITOR  *Edid = (CONST EDID_MONITOR *)Buf;

  if (BufSize < sizeof (*Edid)) {
    return FALSE;
  }

  // Check EDID validity (simplified check)
  if ((Edid->Header[0] != 0x00) || (Edid->Header[1] != 0xFF) ||
      (Edid->Header[2] != 0xFF) || (Edid->Header[3] != 0xFF) ||
      (Edid->Header[4] != 0xFF) || (Edid->Header[5] != 0xFF) ||
      (Edid->Header[6] != 0xFF) || (Edid->Header[7] != 0x00))
  {
    return FALSE;
  }

  if (DpFind1080p60Dtd (
                        Edid->MonitorDetails,
                        EDID_DETAILED_TIMING_COUNT,
                        Timing
                        ))
  {
    return TRUE;
  }

  if ((Edid->ExtensionFlag != 0) && (BufSize >= EDID_EXT_SIZE)) {
    CONST EDID_CEA861_EXTENSION  *Cea =
      (CONST EDID_CEA861_EXTENSION *)(Buf + sizeof (*Edid));

    if (Cea->ExtensionTag == EDID_CEA861_EXTENSION_TAG) {
      INT32  Count  = EDID_CEA861_DTD_COUNT (*Cea);
      INT32  Offset = Cea->DtdOffset;

      if (DpFind1080p60CeaVdb (Cea, Timing)) {
        return TRUE;
      }

      if ((Offset >= 4) &&
          (Offset + Count * sizeof (EDID_DETAILED_TIMING) < EDID_SIZE) &&
          DpFind1080p60Dtd (
                            (CONST EDID_DETAILED_TIMING *)((CONST UINT8 *)Cea + Offset),
                            Count,
                            Timing
                            ))
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

STATIC
BOOLEAN
DpEdidHeaderValid (
  IN CONST UINT8  *Edid
  )
{
  STATIC CONST UINT8  ExpectedHeader[8] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
  };

  return CompareMem (Edid, ExpectedHeader, sizeof (ExpectedHeader)) == 0;
}

STATIC
BOOLEAN
DpEdidBlockChecksumValid (
  IN CONST UINT8  *Edid
  )
{
  UINT32  Index;
  UINT8   Sum;

  Sum = 0;
  for (Index = 0; Index < EDID_LENGTH; Index++) {
    Sum = (UINT8)(Sum + Edid[Index]);
  }

  return Sum == 0;
}

STATIC
VOID
DpDumpEdidData (
  IN CONST UINT8  *Edid,
  IN UINT32       EdidLen
  )
{
  UINT32  Offset;

  for (Offset = 0; Offset < EdidLen; Offset += 16) {
    DEBUG (
           (DEBUG_INFO,
            "EDID[%03u]: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            Offset,
            Edid[Offset + 0], Edid[Offset + 1], Edid[Offset + 2], Edid[Offset + 3],
            Edid[Offset + 4], Edid[Offset + 5], Edid[Offset + 6], Edid[Offset + 7],
            Edid[Offset + 8], Edid[Offset + 9], Edid[Offset + 10], Edid[Offset + 11],
            Edid[Offset + 12], Edid[Offset + 13], Edid[Offset + 14], Edid[Offset + 15])
           );
  }
}

/**
 * Enable DP output with given timing
 */
EFI_STATUS
DpEnable (
  IN SPACEMIT_INNO_DP_PRIV  *Priv,
  IN CONNECTOR_STATE        *ConnectorState
  )
{
  SOC_DP_VIDEO_MODE           *Mode = &Priv->DpDev.VideoMode;
  UINT32                      Value;
  EFI_STATUS                  Status;
  UINTN                       PmuAddr = PMU_BASE_ADDRESS;
  SPACEMIT_MODE_INFO          *Info;
  IN UINT32                   Freq;
  SILICON_CLOCKCTRL_PROTOCOL  *ClockCtrlProtocol;

  DEBUG ((DEBUG_INFO, "%s\n", __FUNCTION__));

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID **)&ClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
  }

  Info = ConnectorState->SpacemitModeInfo;

  // Read sink capabilities
  if (SocDpHwReadSinkCaps (&Priv->DpDev) != 0) {
    DEBUG ((DEBUG_INFO, "Failed to read sink caps\n"));
    Priv->DpDev.Link.Revision        = 0x14;
    Priv->DpDev.Link.MaxRate         = SOC_DP_LINK_RATE_5_40;
    Priv->DpDev.Link.MaxNumLanes     = SOC_DP_LANE_2;
    Priv->DpDev.Link.EnhancedFraming = 1;
  }

  // Set pixel clock rate
  Freq =  Info->PixclockFreq;
  if (Freq == 0) {
    Freq = 148500000;   // Default to 1080p60 pixel clock
  }

  // Use DP pixel clock
  if ((Priv->DpId == 0)) {
    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "LCDPIX",
                                     (UINT64)Freq
                                     );

    Value  = MmioRead32 (PmuAddr + 0x23C);
    Value |= BIT (2);
    MmioWrite32 (PmuAddr + 0x23C, Value);
  } else if ((Priv->DpId == 1)) {
    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "DSI4LN2_PIX",
                                     (UINT64)Freq
                                     );

    Value  = MmioRead32 (PmuAddr + 0x23C);
    Value |= BIT (18);
    MmioWrite32 (PmuAddr + 0x23C, Value);
  }

  if (Info != NULL) {
    Mode->Clock = Info->PixclockFreq / 1000;

    Mode->Hdisplay   = (UINT16)Info->XRes;
    Mode->HsyncStart = (UINT16)(Info->XRes + Info->RightMargin);
    Mode->HsyncEnd   = (UINT16)(Mode->HsyncStart + Info->HsyncLen);
    Mode->Htotal     = (UINT16)(Mode->HsyncEnd + Info->LeftMargin);

    Mode->Vdisplay   = (UINT16)Info->YRes;
    Mode->VsyncStart = (UINT16)(Info->YRes + Info->LowerMargin);
    Mode->VsyncEnd   = (UINT16)(Mode->VsyncStart + Info->VsyncLen);
    Mode->Vtotal     = (UINT16)(Mode->VsyncEnd + Info->UpperMargin);

    Mode->Flags = 0;
    if (Info->Flags & DISPLAY_FLAGS_HSYNC_HIGH) {
      Mode->Flags |= SOC_DP_MODE_FLAG_PHSYNC;
    }

    if (Info->Flags & DISPLAY_FLAGS_VSYNC_HIGH) {
      Mode->Flags |= SOC_DP_MODE_FLAG_PVSYNC;
    }
  }

  Mode->Flags |= SOC_DP_MODE_FLAG_PHSYNC;
  Mode->Flags |= SOC_DP_MODE_FLAG_PVSYNC;

  if (SocDpModeSet (&Priv->DpDev, Mode) == 0) {
    SocDpHwEnable (&Priv->DpDev);
  }

  return EFI_SUCCESS;
}

/**
 * Read EDID from DP sink
 */
EFI_STATUS
DpReadEdid (
  IN  SPACEMIT_INNO_DP_PRIV  *Priv,
  OUT UINT8                  *Buf,
  IN  INT32                  BufSize
  )
{
  UINT32      EdidSize = EDID_LENGTH;
  INT32       i;
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "%s\n", __FUNCTION__));

  for (i = 0; i < 3; i++) {
    Status = SocDpConnGetEdidBlock (&Priv->DpDev, Buf, 0, EDID_LENGTH);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "EDID read failed\n"));
      continue;
    }

    // Check extension flag
    if (Buf[EDID_EXTENSION_FLAG] != 0) {
      EdidSize += EDID_LENGTH;
      if (EdidSize <= (UINT32)BufSize) {
        Status = SocDpConnGetEdidBlock (
                                        &Priv->DpDev,
                                        Buf + EDID_LENGTH,
                                        1,
                                        EDID_LENGTH
                                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_INFO, "additional EDID Read failed!\n"));
          continue;
        }
      }
    }

    DpDumpEdidData (Buf, EdidSize);
    return (EFI_STATUS)EdidSize;
  }

  return Status;
}

/**
 * Get display timing from EDID buffer
 */
EFI_STATUS
EdidGetTiming (
  IN  CONST UINT8     *Edid,
  IN  INT32           EdidLen,
  OUT DISPLAY_TIMING  *Timing,
  OUT INT32           *PanelBpp
  )
{
  CONST EDID_MONITOR  *EdidMon = (CONST EDID_MONITOR *)Edid;

  if ((EdidLen < sizeof (*EdidMon)) || (Edid == NULL) || (Timing == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!DpEdidHeaderValid (Edid)) {
    DEBUG ((DEBUG_ERROR, "EDID header invalid, reject timing parse\n"));
    return EFI_DEVICE_ERROR;
  }

  if (!DpEdidBlockChecksumValid (Edid)) {
    DEBUG ((DEBUG_ERROR, "EDID checksum invalid, reject timing parse\n"));
    return EFI_DEVICE_ERROR;
  }

  ZeroMem (Timing, sizeof (*Timing));

  DpDecodeDetailedTiming (&EdidMon->MonitorDetails[0], Timing);

  if (PanelBpp != NULL) {
    *PanelBpp = 24;
  }

  return EFI_SUCCESS;
}

/**
 * Read display timing from EDID
 */
EFI_STATUS
DpReadTiming (
  IN  SPACEMIT_INNO_DP_PRIV  *Priv,
  CONNECTOR_STATE            *ConnectorState
  )
{
  UINT8               Edid[EDID_EXT_LENGTH];
  DISPLAY_TIMING      Timing;
  DISPLAY_TIMING      FallbackTiming;
  INT32               EdidLen;
  INT32               PanelBpp;
  EFI_STATUS          Status;
  SPACEMIT_MODE_INFO  *Info;

  DEBUG ((DEBUG_INFO, "%s\n", __FUNCTION__));

  Status = DpReadEdid (Priv, Edid, sizeof (Edid));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EdidLen = (INT32)Status;

  Status = EdidGetTiming (Edid, EdidLen, &Timing, &PanelBpp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!Priv->DpDev.EdpMode && (Timing.Hactive.Typ > 1920)) {
    if (DpFind1080p60FromEdid (Edid, EdidLen, &FallbackTiming)) {
      CopyMem (&Timing, &FallbackTiming, sizeof (Timing));
    }
  }

  Info = ConnectorState->SpacemitModeInfo;

  DEBUG ((DEBUG_INFO, "DpReadTiming: Info pointer = 0x%p\n", Info));

  if (Info != NULL) {
    Info->RealXRes       = Timing.Hactive.Typ;
    Info->RealYRes       = Timing.Vactive.Typ;
    Info->XRes           = Timing.Hactive.Typ;
    Info->YRes           = Timing.Vactive.Typ;
    Info->LeftMargin     = Timing.Htotal - Timing.Hsync_end;
    Info->RightMargin    = Timing.Hsync_start - Timing.Hactive.Typ;
    Info->UpperMargin    = Timing.Vtotal - Timing.Vsync_end;
    Info->LowerMargin    = Timing.Vsync_start - Timing.Vactive.Typ;
    Info->HsyncLen       = Timing.Hsync_end - Timing.Hsync_start;
    Info->VsyncLen       = Timing.Vsync_end - Timing.Vsync_start;
    Info->HsyncInvert    = 0;
    Info->VsyncInvert    = 0;
    Info->InvertPixclock = 0;
    Info->PixclockFreq   = Timing.Pixelclock.Typ;
    Info->PixFmtOut      = OUTFMT_RGB888;
    Info->Flags          = Timing.Flags;

    DEBUG (
           (DEBUG_INFO, "DpReadTiming: After assignment - XRes=%d, YRes=%d\n",
            Info->XRes, Info->YRes)
           );
    DEBUG (
           (DEBUG_INFO, "DpReadTiming: LeftMargin=%d, RightMargin=%d, UpperMargin=%d, LowerMargin=%d\n",
            Info->LeftMargin, Info->RightMargin, Info->UpperMargin, Info->LowerMargin)
           );
    DEBUG (
           (DEBUG_INFO, "DpReadTiming: Htotal=%d, Vtotal=%d, Hsync_start=%d, Hsync_end=%d\n",
            Timing.Htotal, Timing.Vtotal, Timing.Hsync_start, Timing.Hsync_end)
           );
    DEBUG (
           (DEBUG_INFO, "DpReadTiming: Vsync_start=%d, Vsync_end=%d\n",
            Timing.Vsync_start, Timing.Vsync_end)
           );
    DEBUG (
           (DEBUG_INFO, "DpReadTiming: %dx%d@%dHz\n",
            Info->XRes, Info->YRes, Timing.Pixelclock.Typ / (Timing.Htotal * Timing.Vtotal))
           );
  }

  return EFI_SUCCESS;
}

STATIC
VOID
DpuDpMmioRemap (
  IN UINT32  DpId
  )
{
  if (DpId == 0) {
    MapRegToGcdMmioSpace (DP0_REG_BASE, SIZE_4KB);
    MapRegToGcdMmioSpace (DPU0_REG_BASE, SIZE_256KB + SIZE_128KB);
  } else {
    MapRegToGcdMmioSpace (DP1_REG_BASE, SIZE_4KB);
    MapRegToGcdMmioSpace (DPU1_REG_BASE, SIZE_256KB + SIZE_128KB);
  }
}

STATIC EFI_STATUS
DpPowerOn (
  IN UINT32  DpId
  )
{
  INT32   loop;
  UINT32  Val;
  UINT64  DP_POWER_CTRL;
  UINT32  Status_Reg_Offset;

  if (DpId == 0) {
    DP_POWER_CTRL     = K3_APMU_BASE + K3_DP0_POWER_CTRL;
    Status_Reg_Offset = 12;
  } else {
    DP_POWER_CTRL     = K3_APMU_BASE + K3_DP1_POWER_CTRL;
    Status_Reg_Offset = 15;
  }

  /* LCD */
  Val  = MmioRead32 (DP_POWER_CTRL);
  Val |= (1 << 0);
  Val |= (1 << 4);
  MmioWrite32 (DP_POWER_CTRL, Val);

  gBS->Stall (310);

  for (loop = 10000; loop >= 0; --loop) {
    Val = MmioRead32 (K3_APMU_BASE + APMU_POWER_STATUS_REG);
    if (Val & (1 << Status_Reg_Offset)) {
      break;
    }

    gBS->Stall (6);
  }

  if (loop < 0) {
    DEBUG ((DEBUG_ERROR, "power-on DP domain error\n"));
    return EFI_NO_RESPONSE;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
DpApplyDefaultPinctrlState (
  IN UINT32  PinGroup
  )
{
  EFI_STATUS                Status;
  SILICON_PINCTRL_PROTOCOL  *PinCtrl;

  PinCtrl = NULL;
  Status  = gBS->LocateProtocol (
                                 &gSpacemitSiliconPinCtrlProtocolGuid,
                                 NULL,
                                 (VOID **)&PinCtrl
                                 );
  if (EFI_ERROR (Status) || (PinCtrl == NULL)) {
    DEBUG ((DEBUG_WARN, "DPHwInit: PinCtrl protocol not available, skipping pinctrl state\n"));
    return;
  }

  if (PinCtrl->ApplyStateById == NULL) {
    DEBUG ((DEBUG_WARN, "DPHwInit: pinctrl state API is unavailable\n"));
    return;
  }

  Status = PinCtrl->ApplyStateById (
                                    PinCtrl,
                                    "dp",
                                    PinGroup,
                                    NULL,
                                    PINCTRL_STATE_DEFAULT
                                    );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((DEBUG_WARN, "DPHwInit: pinctrl default state map is not available for pin group %u\n", PinGroup));
      return;
    }

    DEBUG ((DEBUG_WARN, "DPHwInit: failed to apply pinctrl default state for pin group %u: %r\n", PinGroup, Status));
  }
}

STATIC EFI_STATUS
DpClockInit (
  IN UINT32        DpId,
  CONNECTOR_STATE  *ConnectorState
  )
{
  EFI_STATUS                  Status;
  IN UINT32                   Freq;
  SILICON_CLOCKCTRL_PROTOCOL  *ClockCtrlProtocol;
  LCD_CONFIG_ARRAY            *LcdCfg = &ConnectorState->LcdConfigs;

  Status = gBS->LocateProtocol (
                                &gSpacemitSiliconClockCtrlProtocolGuid,
                                NULL,
                                (VOID **)&ClockCtrlProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Status=%r\n", __FUNCTION__, __LINE__, Status));
  }

  if (DpId == 0) {
    /* lcd reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "LCDCLK",
                                      ENABLE_CLOCK
                                      );
    /* aclk reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "LCDACLK",
                                      ENABLE_CLOCK
                                      );
    /* mclk reset */
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "LCDMCLK",
                                      ENABLE_CLOCK
                                      );
    /* esc reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "LCDDSIESC",
                                      ENABLE_CLOCK
                                      );
    /* dsc reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "LCDDSC",
                                      ENABLE_CLOCK
                                      );

    /* enable hclk */
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "LCDHCLK",
                                      ENABLE_CLOCK
                                      );
    /* enable pxclk */
    Freq = LcdCfg->PixClk;
    if (Freq == 0) {
      Freq = 150000000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "LCDPIX",
                                     (UINT64)Freq
                                     );
    /* enable mclk */
    Freq = LcdCfg->MClk;
    if (Freq == 0) {
      Freq = 307200000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "LCDMCLK",
                                     (UINT64)Freq
                                     );
    /* enable escclk */
    Freq = LcdCfg->EscClk;
    if (Freq == 0) {
      Freq = 51200000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "LCDDSIESC",
                                     (UINT64)Freq
                                     );
    /* enable aclk */
    Freq = LcdCfg->AClk;
    if (Freq == 0) {
      Freq = 409600000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "LCDACLK",
                                     (UINT64)Freq
                                     );

    /* enable dscclk */
    Freq = LcdCfg->DscClk;
    if (Freq == 0) {
      Freq = 614400000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "LCDDSC",
                                     (UINT64)Freq
                                     );

    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "EDP0CLK",
                                      ENABLE_CLOCK
                                      );
  } else {
    /* lcd reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "DSI4LN2_LCDCLK",
                                      ENABLE_CLOCK
                                      );
    /* aclk reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "DSI4LN2_ACLK",
                                      ENABLE_CLOCK
                                      );
    /* mclk reset */
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "DSI4LN2_MCLK",
                                      ENABLE_CLOCK
                                      );
    /* esc reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "DSI4LN2_ESCCLK",
                                      ENABLE_CLOCK
                                      );
    /* dsc reset*/
    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "DSI4LN2_DSCCLK",
                                      ENABLE_CLOCK
                                      );

    /* enable mclk */
    Freq = LcdCfg->MClk;
    if (Freq == 0) {
      Freq = 307200000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "DSI4LN2_MCLK",
                                     (UINT64)Freq
                                     );
    /* enable escclk */
    Freq = LcdCfg->EscClk;
    if (Freq == 0) {
      Freq = 51200000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "DSI4LN2_ESCCLK",
                                     (UINT64)Freq
                                     );

    /* enable aclk */
    Freq = LcdCfg->AClk;
    if (Freq == 0) {
      Freq = 409600000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "DSI4LN2_ACLK",
                                     (UINT64)Freq
                                     );

    /* enable dscclk */
    Freq = LcdCfg->DscClk;
    if (Freq == 0) {
      Freq = 614400000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "DSI4LN2_DSCCLK",
                                     (UINT64)Freq
                                     );

    /* enable pxclk */
    Freq = LcdCfg->PixClk;
    if (Freq == 0) {
      Freq = 150000000;
    }

    ClockCtrlProtocol->SetClockRate (
                                     ClockCtrlProtocol,
                                     "DSI4LN2_PIX",
                                     (UINT64)Freq
                                     );

    ClockCtrlProtocol->SetClockState (
                                      ClockCtrlProtocol,
                                      "EDP1CLK",
                                      ENABLE_CLOCK
                                      );
  }

  return EFI_SUCCESS;
}

EFI_STATUS
DpConnectorPreInit (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  CONNECTOR_STATE  *ConnectorState = &DisplayState->ConnectorState;
  CRTC_STATE       *CrtcState      = &DisplayState->CrtcState;
  struct DpDevice  *DpDev;

  DpDev                           = DP_QP_FROM_CONNECTOR_PROTOCOL (This);
  ConnectorState->OutputInterface = DpDev->OutputInterface;
  CrtcState->DpuMode              = DpDev->OutputInterface;

  if (!SpacemitDp) {
    SpacemitDp = AllocatePool (sizeof (*SpacemitDp));
  }

  ConnectorState->Private = SpacemitDp;

  // ensure mode info buffer exists to avoid NULL dereference later
  if (ConnectorState->SpacemitModeInfo == NULL) {
    ConnectorState->SpacemitModeInfo = AllocatePool (sizeof (SPACEMIT_MODE_INFO));
    if (ConnectorState->SpacemitModeInfo == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: failed to allocate SpacemitModeInfo\n", __func__));
      return EFI_OUT_OF_RESOURCES;
    }

    // clear initial contents
    ZeroMem (ConnectorState->SpacemitModeInfo, sizeof (SPACEMIT_MODE_INFO));
  }

  return EFI_SUCCESS;
}

/**
 * Initialize DP GPIO pins
 */
STATIC
EFI_STATUS
DpGpioInit (
  VOID
  )
{
  EFI_STATUS     Status;
  EMBEDDED_GPIO  *GpioProtocol;
  UINT32         GpioPowerPin;
  UINT32         GpioEnablePin;
  UINT32         GpioBlPin;

  GpioPowerPin  = FixedPcdGet32 (PcdDpGpioPowerPin);
  GpioEnablePin = FixedPcdGet32 (PcdDpGpioEnablePin);
  GpioBlPin     = FixedPcdGet32 (PcdDpGpioBlPin);

  if ((GpioPowerPin == 0) && (GpioEnablePin == 0) && (GpioBlPin == 0)) {
    DEBUG ((DEBUG_INFO, "[DpGpioInit] No GPIO pins configured\n"));
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (
                                &gEmbeddedGpioProtocolGuid,
                                NULL,
                                (VOID **)&GpioProtocol
                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[DpGpioInit] Failed to locate GPIO protocol: %r\n", Status));
    return Status;
  }

  if (GpioPowerPin != 0) {
    Status = GpioProtocol->Set (GpioProtocol, GpioPowerPin, GPIO_MODE_OUTPUT_1);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[DpGpioInit] Failed to set power GPIO: %r\n", Status));
      return Status;
    }

    DEBUG ((DEBUG_INFO, "[DpGpioInit] Power GPIO %d enabled\n", GpioPowerPin));
  }

  if (GpioEnablePin != 0) {
    Status = GpioProtocol->Set (GpioProtocol, GpioEnablePin, GPIO_MODE_OUTPUT_1);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[DpGpioInit] Failed to set enable GPIO: %r\n", Status));
      return Status;
    }

    DEBUG ((DEBUG_INFO, "[DpGpioInit] Enable GPIO %d enabled\n", GpioEnablePin));
  }

  if (GpioBlPin != 0) {
    Status = GpioProtocol->Set (GpioProtocol, GpioBlPin, GPIO_MODE_OUTPUT_1);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[DpGpioInit] Failed to set BL GPIO: %r\n", Status));
      return Status;
    }

    DEBUG ((DEBUG_INFO, "[DpGpioInit] BL GPIO %d enabled\n", GpioBlPin));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
DpConnectorInit (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  EFI_STATUS       Ret;
  CONNECTOR_STATE  *ConnectorState = &DisplayState->ConnectorState;
  CRTC_STATE       *CrtcState      = &DisplayState->CrtcState;
  BOOLEAN          IsVideoConnected;

  SPACEMIT_INNO_DP_PRIV  *Priv = (SPACEMIT_INNO_DP_PRIV *)ConnectorState->Private;

  Priv->DpId = CrtcState->DpuId;

  DpApplyDefaultPinctrlState (CrtcState->PinGroup);

  DpuDpMmioRemap (Priv->DpId);

  Ret = DpPowerOn (Priv->DpId);
  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "[DpConnectorInit] Dp power on failed: %r\n", Ret));
    return EFI_DEVICE_ERROR;
  }

  Ret = DpClockInit (Priv->DpId, ConnectorState);
  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "[DpConnectorInit] Dp clock enable failed: %r\n", Ret));
    return EFI_DEVICE_ERROR;
  }

  if (Priv->DpId == 0) {
    DEBUG ((DEBUG_INFO, "[DpConnectorInit] Using DP0, RegBase=0xcac84000\n"));
    Priv->RegBase = DP0_REG_BASE;
    Priv->RegSize = 0x4000;
    if (CrtcState->DpuMode == DpuModeEdp) {
      Priv->DpType        = INNO_EDP;
      Priv->DpDev.EdpMode = TRUE;
      Ret                 = DpGpioInit ();
      if (EFI_ERROR (Ret)) {
        DEBUG ((DEBUG_ERROR, "[DpConnectorInit] DP GPIO init failed: %r\n", Ret));
        return EFI_DEVICE_ERROR;
      }
    } else {
      Priv->DpType        = INNO_DP;
      Priv->DpDev.EdpMode = FALSE;
    }
  } else {
    DEBUG ((DEBUG_INFO, "[DpConnectorInit] Using DP1, RegBase=0xcac88000\n"));
    Priv->RegBase = DP1_REG_BASE;
    Priv->RegSize = 0x4000;
    if (CrtcState->DpuMode == DpuModeEdp) {
      Priv->DpType        = INNO_EDP;
      Priv->DpDev.EdpMode = TRUE;
    } else {
      Priv->DpType        = INNO_DP;
      Priv->DpDev.EdpMode = FALSE;
    }
  }

  DEBUG ((DEBUG_INFO, "[DpConnectorInit] DpType=%d (0=DP, 1=eDP)\n", Priv->DpType));

  // Initialize SOC DP
  SocDpInit (&Priv->DpDev, Priv->RegBase, SOC_DP_REF_CLK_24M, SOC_VIDEO_RGB_8BIT);
  MicroSecondDelay (5000);

  // Detect HPD
  if (SocDpHwDetectHpd (&Priv->DpDev) != ConnectorStatusConnected) {
    IsVideoConnected = FALSE;
    DEBUG ((DEBUG_INFO, "dp cannot get HPD signal\n"));
    return EFI_DEVICE_ERROR;
  }

  SocDpHwCleanHpd (&Priv->DpDev);

  IsVideoConnected = TRUE;
  Ret = DpReadTiming (Priv, ConnectorState);
  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "[DpConnectorInit] failed to read DP timing: %r\n", Ret));
    return Ret;
  }

  Ret = DpEnable (Priv, ConnectorState);
  if (EFI_ERROR (Ret)) {
    DEBUG ((DEBUG_ERROR, "[DpConnectorInit] failed to enable DP: %r\n", Ret));
    return Ret;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
DpConnectorDetect (
  OUT SPACEMIT_CONNECTOR_PROTOCOL  *This,
  OUT DISPLAY_STATE                *DisplayState
  )
{
  INTN                   Ret;
  BOOLEAN                IsVideoConnected;
  CONNECTOR_STATE        *ConnectorState = &DisplayState->ConnectorState;
  SPACEMIT_INNO_DP_PRIV  *Priv           = (SPACEMIT_INNO_DP_PRIV *)ConnectorState->Private;

  DEBUG ((DEBUG_INFO, "[DpConnectorDetect] Entry\n"));

  if (Priv == NULL) {
    DEBUG ((DEBUG_ERROR, "[DpConnectorDetect] Priv is NULL!\n"));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "[DpConnectorDetect] DpType=%d\n", Priv->DpType));

  if (Priv->DpType == INNO_EDP) {
    DEBUG ((DEBUG_INFO, "[DpConnectorDetect] Calling SocDpHwDetectHpd...\n"));
    Ret              = SocDpHwDetectHpd (&Priv->DpDev);
    IsVideoConnected = (Ret >= 0);
    SocDpHwCleanHpd (&Priv->DpDev);

    DEBUG (
           (DEBUG_INFO, "[DpConnectorDetect] SocDpHwDetectHpd returned: Ret=%d, IsVideoConnected=%d\n",
            Ret, IsVideoConnected)
           );
  } else if (Priv->DpType == INNO_DP) {
    DEBUG ((DEBUG_INFO, "[DpConnectorDetect] Calling SocDpHwDetectHpd...\n"));
    Ret              = SocDpHwDetectHpd (&Priv->DpDev);
    IsVideoConnected = (Ret >= 0);
    SocDpHwCleanHpd (&Priv->DpDev);

    DEBUG (
           (DEBUG_INFO, "[DpConnectorDetect] SocDpHwDetectHpd returned: Ret=%d, IsVideoConnected=%d\n",
            Ret, IsVideoConnected)
           );
  } else {
    IsVideoConnected = FALSE;
    DEBUG (
           (DEBUG_WARN, "[DpConnectorDetect] Unsupported DpType=%d (expected INNO_DP), setting IsVideoConnected=FALSE\n",
            Priv->DpType)
           );
  }

  if (!IsVideoConnected) {
    DEBUG (
           (DEBUG_ERROR, "[DpConnectorDetect] DP cannot get HPD signal! (IsVideoConnected=%d)\n",
            IsVideoConnected)
           );
    DEBUG ((DEBUG_ERROR, "[DpConnectorDetect] Returning EFI_DEVICE_ERROR\n"));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_INFO, "[DpConnectorDetect] HPD detected successfully! Returning EFI_SUCCESS\n"));
  return EFI_SUCCESS;
}

SPACEMIT_CONNECTOR_PROTOCOL  mDpConnectorOps = {
  0,
  NULL,
  DpConnectorPreInit,
  DpConnectorInit,
  NULL,
  DpConnectorDetect,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

STATIC struct DpDevice  SpacemitK3DpDevices[] = {
  {
    .Id              = 0,
    .OutputInterface = DpuModeEdp,
  },
  {
    .Id              = 1,
    .OutputInterface = DpuModeDp,
  },
};

EFI_STATUS
LcdInitDp (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT32      Index;
  EFI_HANDLE  Handle;

  for (Index = 0; Index < ARRAY_SIZE (SpacemitK3DpDevices); Index++) {
    struct DpDevice  *Dp = &SpacemitK3DpDevices[Index];

    Dp->Signature = DP_DEVICE_SIGNATURE;
    CopyMem (&Dp->Connector, &mDpConnectorOps, sizeof (SPACEMIT_CONNECTOR_PROTOCOL));

    Handle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces (
                                                     &Handle,
                                                     &gSpacemitLcdConnectorProtocolGuid,
                                                     &Dp->Connector,
                                                     NULL
                                                     );
    ASSERT_EFI_ERROR (Status);
  }

  return EFI_SUCCESS;
}
