#/** @file
#
#  Component description file for LcdGraphicsOutputDxe module
#
#  Copyright (c) 2011 - 2020, Arm Limited. All rights reserved.<BR>
#  Copyright (c) 2022 Rockchip Electronics Co. Ltd.
#  Copyright (c) 2023-2025, Mario Bălănică <mariobalanica02@gmail.com>
#  Copyright (c) 2025, SpacemiT Co., Ltd.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Include/Library/SpacemitDpu.h>
#include <Guid/EventGroup.h>

#include <Guid/GlobalVariable.h>

#include "LcdGraphicsOutputDxe.h"
BOOLEAN  mDisplayInitialized = FALSE;

STATIC LCD_INSTANCE  mLcdTemplate = {
  LCD_INSTANCE_SIGNATURE,
  NULL,                              // Handle
  {
    // ModeInfo
    0,                               // Version
    0,                               // HorizontalResolution
    0,                               // VerticalResolution
    PixelBltOnly,                    // PixelFormat
    { 0 },                           // PixelInformation
    0,                               // PixelsPerScanLine
  },
  {
    0,    // MaxMode;
    0,    // Mode;
    NULL, // Info;
    0,    // SizeOfInfo;
    0,    // FrameBufferBase;
    0     // FrameBufferSize;
  },
  {
    // Gop
    LcdGraphicsQueryMode, // QueryMode
    LcdGraphicsSetMode,   // SetMode
    LcdGraphicsBlt,       // Blt
    NULL                  // *Mode
  },
  {
    // DevicePath
    {
      {
        HARDWARE_DEVICE_PATH,        HW_VENDOR_DP,
        { (UINT8)(sizeof (VENDOR_DEVICE_PATH)),(UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8) },
      },
      // Hardware Device Path
      EFI_CALLER_ID_GUID // Use the driver's GUID
    },
    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      { sizeof (EFI_DEVICE_PATH_PROTOCOL),0 }
    }
  },
  { 0 },                             // DisplayStates
  2,                                 // DisplayStatesCount
  NULL,                              // DisplayModes
};

STATIC
EFI_STATUS
PrepareDisplays (
  IN LCD_INSTANCE  *Instance
  )
{
  EFI_STATUS                                 Status;
  SPACEMIT_CRTC_PROTOCOL                     *Crtc;
  UINTN                                      ConnectorCount;
  EFI_HANDLE                                 *ConnectorHandles = NULL;
  DISPLAY_CONNECTORS_PRIORITY_VARSTORE_DATA  *ConnectorsPriority;
  SPACEMIT_CONNECTOR_PROTOCOL                *Connector;
  UINTN                                      ConnectorIndex;
  UINTN                                      Index, J;
  DISPLAY_STATE                              *DisplayState;
  CONNECTOR_STATE                            *ConnectorState;
  CRTC_STATE                                 *CrtcState;
  BOOLEAN                                    FoundConnector;

  Status = gBS->LocateProtocol (
                  &gSpacemitLcdCrtcProtocolGuid,
                  NULL,
                  (VOID **)&Crtc
                  );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "%a: Failed to locate gSpacemitLcdCrtcProtocolGuid. Status=%r\n",
       __func__,
       Status
      )
      );
    return Status;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gSpacemitLcdConnectorProtocolGuid,
                  NULL,
                  &ConnectorCount,
                  &ConnectorHandles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "%a: Failed to locate gSpacemitLcdConnectorProtocolGuid. Status=%r\n",
       __func__,
       Status
      )
      );
    return Status;
  }

  ConnectorsPriority = PcdGetPtr (PcdDisplayConnectorsPriority);

  if (ConnectorsPriority == NULL) {
    ASSERT (FALSE);
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  //   Instance->DisplayStatesCount = PcdGetSize (PcdDisplayConnectors) / sizeof (UINT32);

  for (ConnectorIndex = 0; ConnectorIndex < ConnectorCount; ConnectorIndex++) {
    Status = gBS->HandleProtocol (
                    ConnectorHandles[ConnectorIndex],
                    &gSpacemitLcdConnectorProtocolGuid,
                    (VOID **)&Connector
                    );
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    DisplayState = AllocateZeroPool (sizeof (DISPLAY_STATE));
    if (DisplayState == NULL) {
      ASSERT (FALSE);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    ConnectorState = &DisplayState->ConnectorState;
    CrtcState      = &DisplayState->CrtcState;

    ConnectorState->Connector = (VOID *)Connector;
    CrtcState->Crtc           = (VOID *)Crtc;

    if (Connector->Preinit != NULL) {
      Status = Connector->Preinit (Connector, DisplayState);
      if (EFI_ERROR (Status)) {
        ASSERT_EFI_ERROR (Status);
        goto DiscardState;
      }
    }

    if (Crtc->Preinit != NULL) {
      Status = Crtc->Preinit (Crtc, DisplayState);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    for (Index = 0, FoundConnector = FALSE;
         Index < Instance->DisplayStatesCount && !FoundConnector; Index++)
    {
      for (J = 0; J < ConnectorsPriority->DisplayOrderCount; J++) {
        if (ConnectorsPriority->DisplayOrder[J].Mode == ConnectorState->OutputInterface) {
          DEBUG ((
            DEBUG_INFO,
            "display mode: %d, dpu id: %d, pin group: %d\n",
            ConnectorsPriority->DisplayOrder[J].Mode,
            ConnectorsPriority->DisplayOrder[J].DpuId,
            ConnectorsPriority->DisplayOrder[J].PinGroup
            ));

          CrtcState->DpuId    = ConnectorsPriority->DisplayOrder[J].DpuId;
          CrtcState->PinGroup = ConnectorsPriority->DisplayOrder[J].PinGroup;

          if (Connector->Init != NULL) {
            Status = Connector->Init (Connector, DisplayState);
            if (EFI_ERROR (Status)) {
              goto DiscardState;
            }
          }

          Instance->DisplayStates[Index] = DisplayState;
          FoundConnector                 = TRUE;
          break;
        }
      }
    }

    if (Index == Instance->DisplayStatesCount) {
      goto DiscardState;
    }

    continue;

DiscardState:
    FreePool (DisplayState);
  }

  Status = EFI_SUCCESS;

Exit:
  if (EFI_ERROR (Status)) {
    for (Index = 0; Index < Instance->DisplayStatesCount; Index++) {
      if (Instance->DisplayStates[Index] != NULL) {
        FreePool (Instance->DisplayStates[Index]);
        Instance->DisplayStates[Index] = NULL;
      }
    }
  }

  if (ConnectorHandles != NULL) {
    FreePool (ConnectorHandles);
  }

  return Status;
}

STATIC
EFI_STATUS
DetectDisplays (
  IN  LCD_INSTANCE   *Instance,
  IN  BOOLEAN        ForceDetect,
  IN  BOOLEAN        DetectAll,
  OUT DISPLAY_STATE  **PrimaryDisplayState
  )
{
  EFI_STATUS                   Status;
  UINTN                        Index;
  UINTN                        NewCount;
  DISPLAY_STATE                *DisplayState;
  CONNECTOR_STATE              *ConnectorState;
  SPACEMIT_CONNECTOR_PROTOCOL  *Connector;

  for (Index = 0, NewCount = 0; Index < Instance->DisplayStatesCount; Index++) {
    DisplayState = Instance->DisplayStates[Index];
    if (DisplayState == NULL) {
      continue;
    }

    ConnectorState = &DisplayState->ConnectorState;
    Connector      = (SPACEMIT_CONNECTOR_PROTOCOL *)ConnectorState->Connector;

    if (Connector->Detect != NULL) {
      Status = Connector->Detect (Connector, DisplayState);
    } else {
      Status = EFI_SUCCESS;
    }

    if (!EFI_ERROR (Status)) {
      if (*PrimaryDisplayState == NULL) {
        *PrimaryDisplayState = DisplayState;
      }
    } else if (!ForceDetect) {
      FreePool (DisplayState);
      Instance->DisplayStates[Index] = NULL;
      continue;
    }

    Instance->DisplayStates[NewCount++] = DisplayState;

    if ((*PrimaryDisplayState != NULL) && !DetectAll) {
      break;
    }
  }

  Instance->DisplayStatesCount = NewCount;

  if (Instance->DisplayStatesCount == 0) {
    DEBUG ((DEBUG_ERROR, "%a: No displays found!\n", __func__));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SetupDisplay (
  IN  LCD_INSTANCE  *Instance,
  IN DISPLAY_STATE  *DisplayState
  )
{
  EFI_STATUS                   Status;
  CRTC_STATE                   *CrtcState;
  SPACEMIT_CRTC_PROTOCOL       *Crtc;
  CONNECTOR_STATE              *ConnectorState;
  SPACEMIT_CONNECTOR_PROTOCOL  *Connector;
  UINTN                        FbSize;
  EFI_PHYSICAL_ADDRESS         FbAddress;

  CrtcState      = &DisplayState->CrtcState;
  Crtc           = (SPACEMIT_CRTC_PROTOCOL *)CrtcState->Crtc;
  ConnectorState = &DisplayState->ConnectorState;
  Connector      = (SPACEMIT_CONNECTOR_PROTOCOL *)ConnectorState->Connector;

  if (Connector->Prepare != NULL) {
    Status = Connector->Prepare (Connector, DisplayState);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (Crtc->Init != NULL) {
    Status = Crtc->Init (Crtc, DisplayState, &FbAddress, &FbSize);

    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  DEBUG ((DEBUG_INFO, "FbSize = 0x%x (decimal: %u)\n", FbSize, FbSize));
  DEBUG ((DEBUG_INFO, "FbAddress = 0x%llx\n", FbAddress));

  Instance->Mode.FrameBufferBase = FbAddress;
  Instance->Mode.FrameBufferSize = FbSize;

  if (Connector->Enable != NULL) {
    Status = Connector->Enable (Connector, DisplayState);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  DisplayState->IsEnable = TRUE;

  return EFI_SUCCESS;
}

// STATIC
// EFI_STATUS
// SetupAllDisplays (
//   IN LCD_INSTANCE   *Instance,
//   IN DISPLAY_STATE  *PrimaryDisplayState
//   )
// {
//   EFI_STATUS     Status;
//   UINTN          Index;
//   DISPLAY_STATE  *DisplayState;

//   for (Index = 0; Index < Instance->DisplayStatesCount; Index++) {
//     DisplayState = Instance->DisplayStates[Index];
//     if ((DisplayState == NULL) || DisplayState->IsEnable) {
//       continue;
//     }

//     if (PrimaryDisplayState != NULL) {
//       //
//       // Clone primary display sink info.
//       // This is a best effort to support multiple outputs on
//       // a single CRTC port. All sinks are assumed to have more
//       // or less the same capabilities.
//       //
//     //   CopyMem (
//     //     &DisplayState->ConnectorState.SinkInfo,
//     //     &PrimaryDisplayState->ConnectorState.SinkInfo,
//     //     sizeof (DisplayState->ConnectorState.SinkInfo)
//     //     );
//     }

//     Status = SetupDisplay (DisplayState);
//     if (EFI_ERROR (Status)) {
//       continue;
//     }
//   }

//   return EFI_SUCCESS;
// }

STATIC
EFI_STATUS
GetSupportedDisplayModes (
  IN LCD_INSTANCE   *Instance,
  IN DISPLAY_STATE  *DisplayState
  )
{
  DISPLAY_MODE  *Mode;

  CRTC_STATE  *CrtcState;

  CONNECTOR_STATE  *ConnectorState;

  CrtcState      = &DisplayState->CrtcState;
  ConnectorState = &DisplayState->ConnectorState;

  //
  // Only expose a single mode to GOP.
  //
  Instance->Gop.Mode->MaxMode = 1;

  Instance->Gop.Mode->Mode = MAX_UINT32;

  Instance->DisplayModes = AllocateZeroPool (
                             sizeof (DISPLAY_MODE) *
                             Instance->Gop.Mode->MaxMode
                             );
  if (Instance->DisplayModes == NULL) {
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  Mode = &Instance->DisplayModes[0];

  Mode->HActive     = ConnectorState->SpacemitModeInfo->XRes;
  Mode->HFrontPorch = ConnectorState->SpacemitModeInfo->RightMargin;
  Mode->HSync       = ConnectorState->SpacemitModeInfo->HsyncLen;
  Mode->HBackPorch  = ConnectorState->SpacemitModeInfo->LeftMargin;
  Mode->HSyncActive = !ConnectorState->SpacemitModeInfo->HsyncInvert;

  Mode->VActive     = ConnectorState->SpacemitModeInfo->YRes;
  Mode->VFrontPorch = ConnectorState->SpacemitModeInfo->LowerMargin;
  Mode->VSync       = ConnectorState->SpacemitModeInfo->VsyncLen;
  Mode->VBackPorch  = ConnectorState->SpacemitModeInfo->UpperMargin;
  Mode->VSyncActive = !ConnectorState->SpacemitModeInfo->VsyncInvert;

  return EFI_SUCCESS;
}

STATIC
VOID
ClearScreen (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *This
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Fill;

  Fill.Red   = 0x00;
  Fill.Green = 0x00;
  Fill.Blue  = 0x00;
  This->Blt (
          This,
          &Fill,
          EfiBltVideoFill,
          0,
          0,
          0,
          0,
          This->Mode->Info->HorizontalResolution,
          This->Mode->Info->VerticalResolution,
          This->Mode->Info->HorizontalResolution *
          sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
          );
}

VOID
VideoSyncAll (
  UINTN  FbAddress,
  UINTN  FbSize
  )
{
  WriteBackDataCacheRange ((VOID *)FbAddress, FbSize);
}

EFI_STATUS EFIAPI
LcdGraphicsQueryMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL           *This,
  IN UINT32                                 ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **ModeInfo
  )
{
  EFI_STATUS    Status;
  LCD_INSTANCE  *Instance;
  DISPLAY_MODE  *Mode;

  if ((ModeInfo == NULL) || (SizeOfInfo == NULL) || (ModeNumber >= This->Mode->MaxMode)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                  (VOID **)ModeInfo
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Instance = LCD_INSTANCE_FROM_GOP_THIS (This);
  Mode     = &Instance->DisplayModes[ModeNumber];

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  CopyMem (*ModeInfo, This->Mode->Info, *SizeOfInfo);

  if ((0 == Mode->HActive) || (0 == Mode->VActive)) {
    Mode->HActive = 640;
    Mode->VActive = 480;
  }

  (*ModeInfo)->HorizontalResolution = Mode->HActive;
  (*ModeInfo)->VerticalResolution   = Mode->VActive;
  (*ModeInfo)->PixelsPerScanLine    = Mode->HActive;

  return EFI_SUCCESS;
}

/**
 * GopSetMode() - set graphical output mode
 *
 * This function implements the SetMode() service.
 *
 * See the Unified Extensible Firmware Interface (UEFI) specification for
 * details.
 *
 * @This:               the graphical output protocol
 * @ModeNumber:        the mode to be set
 * Return:              status code
 */
EFI_STATUS EFIAPI
LcdGraphicsSetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN UINT32                        ModeNumber
  )
{
  DISPLAY_MODE  *Mode;
  LCD_INSTANCE  *Instance;

  Instance = LCD_INSTANCE_FROM_GOP_THIS (This);

  if (ModeNumber >= This->Mode->MaxMode) {
    return EFI_UNSUPPORTED;
  }

  Mode = &Instance->DisplayModes[ModeNumber];
  DEBUG (
    (DEBUG_INFO, "Setting mode %u from %u: %u x %u\n",
     ModeNumber, This->Mode->Mode, Mode->HActive, Mode->VActive)
    );

  This->Mode->Mode                       = ModeNumber;
  This->Mode->Info->Version              = 0;
  This->Mode->Info->HorizontalResolution = Mode->HActive;
  This->Mode->Info->VerticalResolution   = Mode->VActive;

  /*
   * NOTE: Windows REQUIRES BGR in 32 or 24 bit format.
   */
  This->Mode->Info->PixelFormat       = PixelBlueGreenRedReserved8BitPerColor;
  This->Mode->Info->PixelsPerScanLine = Mode->HActive;
  This->Mode->SizeOfInfo              = sizeof (*This->Mode->Info);
  This->Mode->FrameBufferSize         = Mode->HActive * Mode->VActive * VIDEO_BPP32_BYTES_PER_PIXEL;
  DEBUG ((DEBUG_INFO, "Reported FrameBufferSize is %u\n", This->Mode->FrameBufferSize));

  ClearScreen (This);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LcdGraphicsBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer OPTIONAL,
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN  UINTN                              SourceX,
  IN  UINTN                              SourceY,
  IN  UINTN                              DestinationX,
  IN  UINTN                              DestinationY,
  IN  UINTN                              Width,
  IN  UINTN                              Height,
  IN  UINTN                              Delta OPTIONAL
  )
{
  UINT8  *VidBuf, *BltBuf, *VidBuf1;
  UINTN  I;

  if ((UINTN)BltOperation >= EfiGraphicsOutputBltOperationMax) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Width == 0) || (Height == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  switch (BltOperation) {
    case EfiBltVideoFill:
      BltBuf = (UINT8 *)BltBuffer;
      for (I = 0; I < Height; I++) {
        VidBuf = POS_TO_FB (DestinationX, DestinationY + I);

        SetMem32 (VidBuf, Width * VIDEO_BPP32_BYTES_PER_PIXEL, *(UINT32 *)BltBuf);
      }

      break;

    case EfiBltVideoToBltBuffer:
      if (Delta == 0) {
        Delta = Width * VIDEO_BPP32_BYTES_PER_PIXEL;
      }

      for (I = 0; I < Height; I++) {
        VidBuf = POS_TO_FB (SourceX, SourceY + I);

        BltBuf = (UINT8 *)((UINTN)BltBuffer + (DestinationY + I) * Delta +
                           DestinationX * VIDEO_BPP32_BYTES_PER_PIXEL);

        gBS->CopyMem ((VOID *)BltBuf, (VOID *)VidBuf, VIDEO_BPP32_BYTES_PER_PIXEL * Width);
      }

      break;

    case EfiBltBufferToVideo:
      if (Delta == 0) {
        Delta = Width * VIDEO_BPP32_BYTES_PER_PIXEL;
      }

      for (I = 0; I < Height; I++) {
        VidBuf = POS_TO_FB (DestinationX, DestinationY + I);
        BltBuf = (UINT8 *)((UINTN)BltBuffer + (SourceY + I) * Delta +
                           SourceX * VIDEO_BPP32_BYTES_PER_PIXEL);

        gBS->CopyMem ((VOID *)VidBuf, (VOID *)BltBuf, Width * VIDEO_BPP32_BYTES_PER_PIXEL);
      }

      break;

    case EfiBltVideoToVideo:
      for (I = 0; I < Height; I++) {
        VidBuf  = POS_TO_FB (SourceX, SourceY + I);
        VidBuf1 = POS_TO_FB (DestinationX, DestinationY + I);

        gBS->CopyMem ((VOID *)VidBuf1, (VOID *)VidBuf, Width * VIDEO_BPP32_BYTES_PER_PIXEL);
      }

      break;

    default:
      return EFI_INVALID_PARAMETER;
      break;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
LcdGraphicsOutputDestroy (
  IN LCD_INSTANCE  *Instance
  )
{
  UINTN  Index;

  if (Instance == NULL) {
    return;
  }

  if (Instance->Handle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           Instance->Handle,
           &gEfiGraphicsOutputProtocolGuid,
           &Instance->Gop,
           &gEfiDevicePathProtocolGuid,
           &Instance->DevicePath,
           NULL
           );
  }

  if (Instance->DisplayModes != NULL) {
    FreePool (Instance->DisplayModes);
  }

  for (Index = 0; Index < Instance->DisplayStatesCount; Index++) {
    if (Instance->DisplayStates[Index] != NULL) {
      FreePool (Instance->DisplayStates[Index]);
    }
  }

  FreePool (Instance);
}

STATIC
EFI_STATUS
EFIAPI
LcdGraphicsOutputInit (
  VOID
  )
{
  EFI_STATUS     Status;
  LCD_INSTANCE   *Instance = NULL;
  DISPLAY_STATE  *DisplayState;
  DISPLAY_STATE  *PrimaryDisplayState;
  BOOLEAN        ForceOutput;
  BOOLEAN        DuplicateOutput;

  Instance = AllocateCopyPool (sizeof (LCD_INSTANCE), &mLcdTemplate);
  if (Instance == NULL) {
    ASSERT (FALSE);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PrepareDisplays (Instance);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  ForceOutput     = FALSE;
  DuplicateOutput = FALSE;

  PrimaryDisplayState = NULL;

  Status = DetectDisplays (
             Instance,
             ForceOutput,
             DuplicateOutput,
             &PrimaryDisplayState
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  DisplayState = NULL;

  if (PrimaryDisplayState != NULL) {
    DisplayState = PrimaryDisplayState;
  } else if (ForceOutput) {
    DisplayState = Instance->DisplayStates[0];

    DuplicateOutput = TRUE;
  }

  if (DisplayState == NULL) {
    DEBUG (
      (
       DEBUG_INFO,
       ": DisplayState == null\n"
      )
      );
    ASSERT (FALSE);
    goto Exit;
  }

  Status = SetupDisplay (Instance, DisplayState);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  //   if (DuplicateOutput) {
  //     SetupAllDisplays (Instance, PrimaryDisplayState);
  //   }

  Instance->Gop.Mode  = &Instance->Mode;
  Instance->Mode.Info = &Instance->ModeInfo;

  Status = GetSupportedDisplayModes (Instance, PrimaryDisplayState);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Instance->Handle,
                  &gEfiGraphicsOutputProtocolGuid,
                  &Instance->Gop,
                  &gEfiDevicePathProtocolGuid,
                  &Instance->DevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG (
      (
       DEBUG_ERROR,
       "%a: Failed to install GOP. Status=%r\n",
       __func__,
       Status
      )
      );
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    LcdGraphicsOutputDestroy (Instance);
  }

  return Status;
}

VOID
EFIAPI
LcdGraphicsOutputEndOfDxeEventHandler (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  gBS->CloseEvent (Event);

  LcdGraphicsOutputInit ();
}

EFI_STATUS
EFIAPI
LcdGraphicsOutputDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   EndOfDxeEvent;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  LcdGraphicsOutputEndOfDxeEventHandler,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
