/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>
  Copyright (c) 2025, Spacemit Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_MANAGER_VFR_H_
#define PLATFORM_MANAGER_VFR_H_

#define FORMSET_GUID \
  { \
  0x26741529, 0x7779, 0x4217, { 0x98, 0x53, 0xa9, 0x22, 0xcd, 0xac, 0xf9, 0x60 } \
  }

//
// These are defined as the same with vfr file
//
#define LABEL_FORM_ID_OFFSET                 0x0100
#define ENTRY_KEY_OFFSET                     0x4000

#define PLATFORM_MANAGER_FORM_ID             0x1000

#define LABEL_ENTRY_LIST                     0x1100
#define LABEL_END                            0xffff

#endif /* PLATFORM_MANAGER_VFR_H_ */
