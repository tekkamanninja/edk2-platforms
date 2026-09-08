/**
*
*  Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <Base.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryManagementLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/DxeServicesTableLib.h>
#include <Include/Library/SpacemitDisplayLib.h>
#include <Include/Protocol/SpacemitCrtcProtocol.h>
#include "Include/Library/SpacemitDpu.h"
#include "DpuDxe.h"

extern BOOLEAN  mIsVideoConnected;

STATIC
EFI_STATUS
AllocateFrameBuffer (
  VOID   **FbBase,
  UINTN  *FbSize
  )
{
  EFI_STATUS  Status;
  VOID        *Buffer;
  UINTN       Pages, BuffSize;

  Pages  = EFI_SIZE_TO_PAGES (*FbSize);
  Buffer = AllocateRuntimePages (Pages);
  if (NULL == Buffer) {
    DEBUG (
           (DEBUG_ERROR,
            "%a: failed to allocate %d pages memory for framebuffer\n",
            __func__, Pages)
           );
    return EFI_OUT_OF_RESOURCES;
  }

  BuffSize = EFI_PAGES_TO_SIZE (Pages);
  Status   = gDS->SetMemorySpaceAttributes ((UINTN)Buffer, BuffSize, EFI_MEMORY_WC);
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR,
            "%a: failed to set memory space attributes for region [0x%p+0x%Lx)\n",
            __func__, Buffer, BuffSize)
           );

    FreePages (Buffer, Pages);
    return EFI_DEVICE_ERROR;
  }

  *FbBase = Buffer;
  *FbSize = BuffSize;
  SetMem (Buffer, BuffSize, 0);

  return EFI_SUCCESS;
}

STATIC
VOID
DpuWrite (
  IN UINTN   DpuBaseAddr,
  IN UINTN   Address,
  IN UINT32  Value
  )
{
  DEBUG ((DEBUG_INFO, "DPU write [0x%lx] = 0x%x\n", DpuBaseAddr + Address, Value));
  MmioWrite32 (DpuBaseAddr + Address, Value);
}

STATIC
VOID
DpuDeviceInit (
  IN SPACEMIT_MODE_INFO    *ModeInfo,
  IN EFI_PHYSICAL_ADDRESS  FbBase,
  IN UINTN                 DpuBaseAddr,
  IN INTN                  DpuType
  )
{
  UINT32  Vsync = ModeInfo->VsyncLen & 0x3FF;
  UINT32  Hsync = ModeInfo->HsyncLen & 0x3FF;
  UINT32  Vbp   = ModeInfo->UpperMargin & 0xFFF;
  UINT32  Vfp   = ModeInfo->LowerMargin & 0xFFF;
  UINT32  Hbp   = ModeInfo->LeftMargin & 0xFFF;
  UINT32  Hfp   = ModeInfo->RightMargin & 0xFFF;
  UINT32  Vsp   = ModeInfo->HsyncInvert ? 0 : 1;
  UINT32  Hsp   = ModeInfo->HsyncInvert ? 0 : 1;
  UINTN   CiuAddr;
  UINT32  Value;

  DEBUG ((DEBUG_INFO, "DpuDeviceInit: Hbp %d, Hfp %d Hsync %d Vsp %d\n", Hbp, Hfp, Hsync, Vsp));
  DEBUG ((DEBUG_INFO, "DpuDeviceInit: Vbp %d, Vfp %d Vsync %d Hsp %d\n", Vbp, Vfp, Vsync, Hsp));

  if ((DpuType == DpuModeDp) || (DpuType == DpuModeEdp)) {
    CiuAddr = K3_CIU_BASE;
    MapRegToGcdMmioSpace (CiuAddr, SIZE_4KB);

    Value  = MmioRead32 (CiuAddr + 0x12C);
    Value |= BIT8;
    MmioWrite32 (CiuAddr + 0x12C, Value);
    Value = MmioRead32 (CiuAddr + 0x12C);
  }

  MapRegToGcdMmioSpace (DpuBaseAddr, SIZE_256KB + SIZE_128KB);
  DpuWrite (DpuBaseAddr, 0x30000, 0x1);

  DpuWrite (DpuBaseAddr, 0x5120C, Hfp << 16);
  DpuWrite (DpuBaseAddr, 0x51210, (Hbp << 16) | Hsync);
  DpuWrite (DpuBaseAddr, 0x51214, (Vsync << 16) | Vfp);
  DpuWrite (DpuBaseAddr, 0x51218, (ModeInfo->XRes << 16) | Vbp);
  DpuWrite (DpuBaseAddr, 0x5121C, ModeInfo->YRes);
  DpuWrite (DpuBaseAddr, 0x51200, 0x184);
  DpuWrite (DpuBaseAddr, 0x51208, ModeInfo->PixFmtOut << 12);
  DpuWrite (DpuBaseAddr, 0x5123C, 0x1);

  DpuWrite (DpuBaseAddr, 0x1284, 0x840);

  DpuWrite (DpuBaseAddr, 0x1000, 0xF00217C);
  DpuWrite (DpuBaseAddr, 0x1024, (UINT32)(FbBase & 0xFFFFFFFF));
  DpuWrite (DpuBaseAddr, 0x1028, (UINT32)(FbBase >> 32));
  DpuWrite (DpuBaseAddr, 0x103C, ModeInfo->XRes * 4);
  DpuWrite (DpuBaseAddr, 0x1040, ModeInfo->XRes | (ModeInfo->YRes << 16));
  DpuWrite (DpuBaseAddr, 0x1044, 0x0);
  DpuWrite (DpuBaseAddr, 0x1048, (ModeInfo->XRes - 1) | ((ModeInfo->YRes - 1) << 16));
  DpuWrite (DpuBaseAddr, 0x1074, 0x8);
  /* supports up to 3840x2160 */
  DpuWrite (DpuBaseAddr, 0x107C, 0x100001e0);

  DpuWrite (DpuBaseAddr, 0x30004, ModeInfo->XRes | (ModeInfo->YRes << 16));
  DpuWrite (DpuBaseAddr, 0x30020, ((ModeInfo->XRes - 1) << 16));
  DpuWrite (DpuBaseAddr, 0x30024, ((ModeInfo->YRes - 1) << 16));
  DpuWrite (DpuBaseAddr, 0x30038, 0xFF03);
  DpuWrite (DpuBaseAddr, 0x30300, 0x0);
  DpuWrite (DpuBaseAddr, 0x30334, ModeInfo->XRes | (ModeInfo->YRes << 16));

  DpuWrite (DpuBaseAddr, 0x340, 0x1040001);

  DpuWrite (DpuBaseAddr, 0x34C, 0x821);

  DpuWrite (DpuBaseAddr, 0x348, 0x1);
  DpuWrite (DpuBaseAddr, 0x350, 0x1);
}

EFI_STATUS
SpacemitDisplayInit (
  IN DISPLAY_STATE  *DisplayState,
  VOID              **FbBase,
  UINTN             *FbSize
  )
{
  UINT32              Xsize, Ysize, Bpix;
  SPACEMIT_MODE_INFO  *ModeInfo = NULL;
  UINTN               DpuBaseAddr;
  EFI_STATUS          Status;
  CONNECTOR_STATE     *ConnectorState = &DisplayState->ConnectorState;
  CRTC_STATE          *CrtcState      = &DisplayState->CrtcState;

  Bpix = VIDEO_BPP32_BITS_PER_PIXEL;
  UINT32  DpuType = CrtcState->DpuMode;
  UINT32  DpuId   = CrtcState->DpuId;

  DEBUG ((DEBUG_INFO, "SpacemitDisplayInit: Mode=%d, DpuId=%d, Bpix=%d\n", DpuType, DpuId, Bpix));

  if (DpuId == 0) {
    DpuBaseAddr = DPU0_REG_BASE;
  } else if (DpuId == 1) {
    DpuBaseAddr = DPU1_REG_BASE;
  } else {
    DEBUG ((DEBUG_ERROR, "%a(): Unsupported dpu_id %d\n", __func__, DpuId));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "DPU Base Address = 0x%lx\n", DpuBaseAddr));

  if (ConnectorState->SpacemitModeInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "SpacemitModeInfo is NULL!\n"));
    return EFI_INVALID_PARAMETER;
  }

  Xsize = ConnectorState->SpacemitModeInfo->XRes;
  Ysize = ConnectorState->SpacemitModeInfo->YRes;

  DEBUG ((DEBUG_INFO, "Resolution: X=%d, Y=%d\n", Xsize, Ysize));

  *FbSize = Xsize * Ysize * VIDEO_BPP32_BYTES_PER_PIXEL;

  // Validate framebuffer size (max 32MB to prevent overflow/excessive allocation)
  if ((*FbSize == 0) || (*FbSize > SIZE_32MB)) {
    DEBUG (
           (DEBUG_ERROR, "%a: Invalid framebuffer size: 0x%lx (X=%d, Y=%d)\n",
            __func__, *FbSize, Xsize, Ysize)
           );
    return EFI_INVALID_PARAMETER;
  }

  Status = AllocateFrameBuffer (FbBase, FbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AllocateFrameBuffer failed: Status=0x%x\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "Framebuffer allocated at %p\n", *FbBase));

  ModeInfo = ConnectorState->SpacemitModeInfo;

  DpuDeviceInit (ModeInfo, (UINTN)*FbBase, DpuBaseAddr, DpuType);

  return EFI_SUCCESS;
}

BOOLEAN
IsHotPlugDevices (
  IN  INT32  DpuMode
  )
{
  switch (DpuMode) {
    case DpuModeEdp:
      return TRUE;
    default:
      return FALSE;
  }
}

EFI_STATUS
DpuPreInit (
  IN  SPACEMIT_CRTC_PROTOCOL  *This,
  OUT DISPLAY_STATE           *DisplayState
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
DpuInit (
  IN  SPACEMIT_CRTC_PROTOCOL  *This,
  OUT DISPLAY_STATE           *DisplayState,
  IN EFI_PHYSICAL_ADDRESS     *FbAddress,
  IN UINTN                    *FbSize
  )
{
  EFI_STATUS  Status;

  Status = SpacemitDisplayInit (DisplayState, (VOID **)FbAddress, FbSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SpacemitDisplayInit failed with Status=0x%x\n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC SPACEMIT_CRTC_PROTOCOL  mDpu = {
  0,
  DpuPreInit,
  DpuInit,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

EFI_STATUS
DpuDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;

  Status = gBS->InstallMultipleProtocolInterfaces (
                                                   &Handle,
                                                   &gSpacemitLcdCrtcProtocolGuid,
                                                   &mDpu,
                                                   NULL
                                                   );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
