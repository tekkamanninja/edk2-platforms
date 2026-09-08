/** @file

  Copyright (c) 2022 Rockchip Electronics Co. Ltd.
  Copyright (c) 2025, SpacemiT Co., Ltd.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SPACEMIT_CONNECTOR_PROTOCOL_H_
#define _SPACEMIT_CONNECTOR_PROTOCOL_H_
#include <Include/Library/SpacemitDisplayLib.h>

#define SPACEMIT_CONNECTOR_PROTOCOL_GUID   \
    {0x7BE180E9, 0x847D, 0x4CAC, {0xB3, 0x22, 0xA7, 0xE4, 0x3C, 0x76, 0x30, 0x66}}

typedef struct _SPACEMIT_CONNECTOR_PROTOCOL SPACEMIT_CONNECTOR_PROTOCOL;

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_PREINIT)(
                                     IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                     IN OUT DISPLAY_STATE                *DisplayState
                                     );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_INIT)(
                                  IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                  IN OUT DISPLAY_STATE                *DisplayState
                                  );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_DEINIT)(
                                    IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                    IN OUT DISPLAY_STATE                *DisplayState
                                    );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_DETECT)(
                                    IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                    IN OUT DISPLAY_STATE                *DisplayState
                                    );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_GET_TIMING)(
                                        IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                        IN OUT DISPLAY_STATE                *DisplayState
                                        );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_GET_EDID)(
                                      IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                      IN OUT DISPLAY_STATE                *DisplayState
                                      );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_PREPARE)(
                                     IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                     IN OUT DISPLAY_STATE                *DisplayState
                                     );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_ENABLE)(
                                    IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                    IN OUT DISPLAY_STATE                *DisplayState
                                    );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_DISABLE)(
                                     IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                     IN OUT DISPLAY_STATE                *DisplayState
                                     );

typedef
  EFI_STATUS
(EFIAPI *SPACEMIT_CONNECTOR_UNPREPARE)(
                                       IN SPACEMIT_CONNECTOR_PROTOCOL      *This,
                                       IN OUT DISPLAY_STATE                *DisplayState
                                       );

struct _SPACEMIT_CONNECTOR_PROTOCOL {
  UINT64                           Revision;
  VOID                             *Private;
  SPACEMIT_CONNECTOR_PREINIT       Preinit;
  SPACEMIT_CONNECTOR_INIT          Init;
  SPACEMIT_CONNECTOR_DEINIT        Deinit;
  SPACEMIT_CONNECTOR_DETECT        Detect;
  SPACEMIT_CONNECTOR_GET_TIMING    GetTiming;
  SPACEMIT_CONNECTOR_GET_EDID      GetEdid;
  SPACEMIT_CONNECTOR_PREPARE       Prepare;
  SPACEMIT_CONNECTOR_ENABLE        Enable;
  SPACEMIT_CONNECTOR_DISABLE       Disable;
  SPACEMIT_CONNECTOR_UNPREPARE     Unprepare;
};

extern EFI_GUID  gSpacemitConnectorProtocolGuid;

#endif
