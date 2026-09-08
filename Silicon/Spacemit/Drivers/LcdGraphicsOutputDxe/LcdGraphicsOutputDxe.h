/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Include/Library/SpacemitDisplayLib.h>

#include <Protocol/Cpu.h>
#include <Protocol/DevicePath.h>
#include <Include/Protocol/SpacemitCrtcProtocol.h>
#include <Include/Protocol/SpacemitConnectorProtocol.h>

#define LCD_INSTANCE_SIGNATURE  SIGNATURE_32('l', 'c', 'd', '0')
#define LCD_INSTANCE_FROM_GOP_THIS(a)  CR (a, LCD_INSTANCE, Gop, LCD_INSTANCE_SIGNATURE)

#define POS_TO_FB(posX, posY)  ((UINT8*)                                       \
                               ((UINTN)This->Mode->FrameBufferBase +           \
                                (posY) * This->Mode->Info->PixelsPerScanLine * \
                                VIDEO_BPP32_BYTES_PER_PIXEL +                  \
                                (posX) * VIDEO_BPP32_BYTES_PER_PIXEL))

typedef struct {
  UINT32    Vic;
  UINT32    OscFreq;
  UINT32    HActive;
  UINT32    HFrontPorch;
  UINT32    HSync;
  UINT32    HBackPorch;
  UINT32    HSyncActive;
  UINT32    VActive;
  UINT32    VFrontPorch;
  UINT32    VSync;
  UINT32    VBackPorch;
  UINT32    VSyncActive;
  UINT32    DenActive;
  UINT32    ClkActive;
} DISPLAY_MODE;

typedef struct {
  VENDOR_DEVICE_PATH          Guid;
  EFI_DEVICE_PATH_PROTOCOL    End;
} LCD_GRAPHICS_DEVICE_PATH;

typedef struct {
  UINT32                                  Signature;
  EFI_HANDLE                              Handle;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    ModeInfo;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE       Mode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL            Gop;
  LCD_GRAPHICS_DEVICE_PATH                DevicePath;
  DISPLAY_STATE                           *DisplayStates[VOP_OUTPUT_IF_NUMS];
  UINT32                                  DisplayStatesCount;
  DISPLAY_MODE                            *DisplayModes;
} LCD_INSTANCE;

EFI_STATUS
EFIAPI
LcdGraphicsQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  );

EFI_STATUS
EFIAPI
LcdGraphicsSetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN UINT32                        ModeNumber
  );

EFI_STATUS
EFIAPI
LcdGraphicsBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *BltBuffer  OPTIONAL,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN UINTN                              SourceX,
  IN UINTN                              SourceY,
  IN UINTN                              DestinationX,
  IN UINTN                              DestinationY,
  IN UINTN                              Width,
  IN UINTN                              Height,
  IN UINTN                              Delta       OPTIONAL
  );

VOID
VideoSyncAll (
  UINTN  FbAddress,
  UINTN  FbSize
  );
