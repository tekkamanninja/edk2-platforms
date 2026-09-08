/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _SPACEMIT_DP_H_
#define _SPACEMIT_DP_H_

#include <Include/Protocol/SpacemitConnectorProtocol.h>
#include <Uefi.h>
#include "InnoDp.h"

/* Display timing entry with min/typ/max values */
typedef struct {
  UINT32    Min;
  UINT32    Typ;
  UINT32    Max;
} TIMING_ENTRY;

/* Display flags */
#define DISPLAY_FLAGS_HSYNC_HIGH  0x01
#define DISPLAY_FLAGS_HSYNC_LOW   0x00
#define DISPLAY_FLAGS_VSYNC_HIGH  0x02
#define DISPLAY_FLAGS_VSYNC_LOW   0x00
#define DISPLAY_FLAGS_INTERLACED  0x04

/* Helper macros for bit operations */
#define BIT(x)                  (1U << (x))
#define GET_BIT(x, bit)         (((x) >> (bit)) & 1)
#define GET_BITS(x, high, low)  (((x) >> (low)) & ((1 << ((high) - (low) + 1)) - 1))

/* EDID detailed timing structure */
typedef struct {
  UINT8    PixelClock[2];
  UINT8    HorizontalActive;
  UINT8    HorizontalBlanking;
  UINT8    HorizontalActiveBlankingHi;
  UINT8    VerticalActive;
  UINT8    VerticalBlanking;
  UINT8    VerticalActiveBlankingHi;
  UINT8    HsyncOffset;
  UINT8    HsyncPulseWidth;
  UINT8    VsyncOffsetPulseWidth;
  UINT8    HsyncVsyncOffsetPulseWidthHi;
  UINT8    HimageSize;
  UINT8    VimageSize;
  UINT8    HimageVimageSize_hi;
  UINT8    Hborder;
  UINT8    Vborder;
  UINT8    Flags;
} EDID_DETAILED_TIMING;

#define EDID_DETAILED_TIMING_PIXEL_CLOCK(x) \
  (((((UINT32)(x).PixelClock[1]) << 8) + (x).PixelClock[0]) * 10000)
#define EDID_DETAILED_TIMING_HORIZONTAL_ACTIVE(x) \
  ((GET_BITS((x).HorizontalActiveBlankingHi, 7, 4) << 8) + (x).HorizontalActive)
#define EDID_DETAILED_TIMING_HORIZONTAL_BLANKING(x) \
  ((GET_BITS((x).HorizontalActiveBlankingHi, 3, 0) << 8) + (x).HorizontalBlanking)
#define EDID_DETAILED_TIMING_VERTICAL_ACTIVE(x) \
  ((GET_BITS((x).VerticalActiveBlankingHi, 7, 4) << 8) + (x).VerticalActive)
#define EDID_DETAILED_TIMING_VERTICAL_BLANKING(x) \
  ((GET_BITS((x).VerticalActiveBlankingHi, 3, 0) << 8) + (x).VerticalBlanking)
#define EDID_DETAILED_TIMING_HSYNC_OFFSET(x) \
  ((GET_BITS((x).HsyncVsyncOffsetPulseWidthHi, 7, 6) << 8) + (x).HsyncOffset)
#define EDID_DETAILED_TIMING_HSYNC_PULSE_WIDTH(x) \
  ((GET_BITS((x).HsyncVsyncOffsetPulseWidthHi, 5, 4) << 8) + (x).HsyncPulseWidth)
#define EDID_DETAILED_TIMING_VSYNC_OFFSET(x) \
  ((GET_BITS((x).HsyncVsyncOffsetPulseWidthHi, 3, 2) << 4) + GET_BITS((x).VsyncOffsetPulseWidth, 7, 4))
#define EDID_DETAILED_TIMING_VSYNC_PULSE_WIDTH(x) \
  ((GET_BITS((x).HsyncVsyncOffsetPulseWidthHi, 1, 0) << 4) + GET_BITS((x).VsyncOffsetPulseWidth, 3, 0))
#define EDID_DETAILED_TIMING_FLAG_INTERLACED(x) \
  GET_BIT((x).Flags, 7)
#define EDID_DETAILED_TIMING_FLAG_VSYNC_POLARITY(x) \
  GET_BIT((x).Flags, 2)
#define EDID_DETAILED_TIMING_FLAG_HSYNC_POLARITY(x) \
  GET_BIT((x).Flags, 1)

/* EDID constants */
#define EDID_SIZE                  128
#define EDID_EXT_SIZE              256
#define EDID_CEA861_EXTENSION_TAG  0x02
#define EDID_DETAILED_TIMING_COUNT 4

/* EDID structures */
typedef struct {
  UINT8                   Header[8];
  UINT8                   ManufacturerId[2];
  UINT8                   ProductCode[2];
  UINT8                   SerialNumber[4];
  UINT8                   WeekOfManufacture;
  UINT8                   YearOfManufacture;
  UINT8                   EdidVersion;
  UINT8                   EdidRevision;
  UINT8                   VideoInputDefinition;
  UINT8                   MaxHorizontalImageSize;
  UINT8                   MaxVerticalImageSize;
  UINT8                   DisplayTransferCharacteristic;
  UINT8                   FeatureSupport;
  UINT8                   ColorCharacteristics[10];
  UINT8                   EstablishedTimings[3];
  UINT8                   StandardTimings[16];
  EDID_DETAILED_TIMING    MonitorDetails[EDID_DETAILED_TIMING_COUNT];
  UINT8                   ExtensionFlag;
  UINT8                   Checksum;
} EDID_BASE;

typedef struct {
  UINT8    ExtensionTag;
  UINT8    Revision;
  UINT8    DtdOffset;
  UINT8    NativeFormats;
  UINT8    Data[124];
} EDID_CEA861_EXTENSION;

typedef union {
  EDID_BASE                Base;
  EDID_CEA861_EXTENSION    Cea861;
  UINT8                    Raw[EDID_EXT_SIZE];
} EDID_EXTENSION;

typedef struct {
  UINT8                   Header[8];
  UINT8                   ManufacturerId[2];
  UINT8                   ProductCode[2];
  UINT8                   SerialNumber[4];
  UINT8                   WeekOfManufacture;
  UINT8                   YearOfManufacture;
  UINT8                   EdidVersion;
  UINT8                   EdidRevision;
  UINT8                   VideoInputDefinition;
  UINT8                   MaxHorizontalImageSize;
  UINT8                   MaxVerticalImageSize;
  UINT8                   DisplayTransferCharacteristic;
  UINT8                   FeatureSupport;
  UINT8                   ColorCharacteristics[10];
  UINT8                   EstablishedTimings[3];
  UINT8                   StandardTimings[16];
  EDID_DETAILED_TIMING    MonitorDetails[EDID_DETAILED_TIMING_COUNT];
  UINT8                   ExtensionFlag;
  UINT8                   Checksum;
} EDID_MONITOR;

#define EDID_CEA861_DTD_COUNT(x) \
  ((x).NativeFormats & 0x0F)

#define EDID_CEA861_DB_TYPE(x, offset) \
  GET_BITS((x).Data[offset], 7, 5)
#define EDID_CEA861_DB_LEN(x, offset) \
  GET_BITS((x).Data[offset], 4, 0)

/* CEA861 data block types */
#define EDID_CEA861_DB_AUDIO        1
#define EDID_CEA861_DB_VIDEO        2
#define EDID_CEA861_DB_VENDOR       3
#define EDID_CEA861_DB_SPEAKER      4
#define EDID_CEA861_DB_COLORIMETRY  5

/* Register base addresses */
#define DPU0_REG_BASE     (FixedPcdGet64(PcdSpacemitDpu0RegBase))
#define DPU1_REG_BASE     (FixedPcdGet64(PcdSpacemitDpu1RegBase))
#define DP0_REG_BASE      (FixedPcdGet64(PcdSpacemitDp0RegBase))
#define DP1_REG_BASE      (FixedPcdGet64(PcdSpacemitDp1RegBase))
#define DP_REGISTER_SIZE  (0x4000)

#define K3_APMU_BASE  (FixedPcdGet64(PcdSpacemitAPMURegBase))

/* Display timing configuration */
typedef struct {
  TIMING_ENTRY    Pixelclock;
  TIMING_ENTRY    Hactive;
  TIMING_ENTRY    Hfront_porch;
  TIMING_ENTRY    Hback_porch;
  TIMING_ENTRY    Hsync_len;
  TIMING_ENTRY    Vactive;
  TIMING_ENTRY    Vfront_porch;
  TIMING_ENTRY    Vback_porch;
  TIMING_ENTRY    Vsync_len;
  UINT32          Flags;
  BOOLEAN         HdmiMonitor;
  UINT32          Htotal;
  UINT32          Vtotal;
  UINT32          Hsync_start;
  UINT32          Hsync_end;
  UINT32          Vsync_start;
  UINT32          Vsync_end;
} DISPLAY_TIMING;

typedef enum {
  INNO_DP = 0,
  INNO_EDP,
} SPACEMIT_INNO_DP_TYPES;

typedef struct {
  VOID                      *Base;
  VOID                      *DpConn;
  UINT64                    RegBase;
  UINT64                    RegSize;
  SOC_DP_DEV                DpDev;
  SPACEMIT_INNO_DP_TYPES    DpType;

  UINT32                    DpId;
  UINT32                    EdpId;

  BOOLEAN                   PowerValid;
  BOOLEAN                   EnableValid;
  BOOLEAN                   BlValid;

  UINT32                    PxClk;
  UINT32                    DpPxClk;
  UINT32                    DpMclk;
  UINT32                    DpHclk;
  UINT32                    DpEscclk;
  UINT32                    DpDscclk;
  UINT32                    DpAclk;
  UINT32                    DpDppxclk;
} SPACEMIT_INNO_DP_PRIV;

struct DpDevice {
  UINT32                         Signature;
  SPACEMIT_CONNECTOR_PROTOCOL    Connector;
  UINT32                         Id;
  UINT32                         OutputInterface;
};

#define DP_DEVICE_SIGNATURE  SIGNATURE_32('D', 'P', 'D', 'V')

#define DP_QP_FROM_CONNECTOR_PROTOCOL(a) \
  BASE_CR (a, struct DpDevice, Connector)

/* Function declarations */
EFI_STATUS
EdidGetTiming (
  IN  CONST UINT8     *Edid,
  IN  INT32           EdidLen,
  OUT DISPLAY_TIMING  *Timing,
  OUT INT32           *PanelBpp
  );

#endif /* _SPACEMIT_INNO_DP_H_ */
