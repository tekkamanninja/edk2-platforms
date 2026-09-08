/** @file

  Copyright (c) 2022 Rockchip Electronics Co. Ltd.
  Copyright (c) 2025, SpacemiT Co., Ltd.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include <Include/Library/SpacemitDisplayLib.h>

#define SPACEMIT_CRTC_PROTOCOL_GUID   \
    {0xBBB5E664, 0x0DE7, 0x4D7E, {0x85, 0xA7, 0x86, 0x93, 0x6C, 0x9D, 0xAB, 0x22}}

typedef struct _SPACEMIT_CRTC_PROTOCOL SPACEMIT_CRTC_PROTOCOL;

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_PREINIT)(
                                IN SPACEMIT_CRTC_PROTOCOL      *This,
                                IN OUT DISPLAY_STATE           *DisplayState
                                );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_INIT)(
                             IN SPACEMIT_CRTC_PROTOCOL      *This,
                             IN OUT DISPLAY_STATE           *DisplayState,
                             IN EFI_PHYSICAL_ADDRESS  *FbAddress,
                             IN UINTN                 *FbSize
                             );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_DEINIT)(
                               IN SPACEMIT_CRTC_PROTOCOL      *This,
                               IN OUT DISPLAY_STATE           *DisplayState
                               );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_SET_PLANE)(
                                  IN SPACEMIT_CRTC_PROTOCOL      *This,
                                  IN OUT DISPLAY_STATE           *DisplayState
                                  );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_PREPARE)(
                                IN SPACEMIT_CRTC_PROTOCOL      *This,
                                IN OUT DISPLAY_STATE           *DisplayState
                                );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_ENABLE)(
                               IN SPACEMIT_CRTC_PROTOCOL      *This,
                               IN OUT DISPLAY_STATE           *DisplayState
                               );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_DISABLE)(
                                IN SPACEMIT_CRTC_PROTOCOL      *This,
                                IN OUT DISPLAY_STATE           *DisplayState
                                );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CRTC_UNPREPARE)(
                                  IN SPACEMIT_CRTC_PROTOCOL      *This,
                                  IN OUT DISPLAY_STATE           *DisplayState
                                  );

struct _SPACEMIT_CRTC_PROTOCOL {
  UINT64                     Revision;
  SPACEMIT_CRTC_PREINIT      Preinit;
  SPACEMIT_CRTC_INIT         Init;
  SPACEMIT_CRTC_DEINIT       Deinit;
  SPACEMIT_CRTC_SET_PLANE    SetPlane;
  SPACEMIT_CRTC_PREPARE      Prepare;
  SPACEMIT_CRTC_ENABLE       Enable;
  SPACEMIT_CRTC_DISABLE      Disable;
  SPACEMIT_CRTC_UNPREPARE    Unprepare;
};

extern EFI_GUID  gSpacemitLcdCrtcProtocolGuid;
