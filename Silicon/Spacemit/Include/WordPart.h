/** @file
  Some helper macros for part of a word.

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SPACEMIT_WORD_PART_H__
#define __SPACEMIT_WORD_PART_H__

/**
  Return bits 32-63 of a number.

  This is a basic shift-right of a 64- or 32-bit quantity. Right shift 16-bit
  twice to suppress the "right shift count >= width of type" warning when the
  quantity is 32-bit.

  @param    n   The number being accessed.

  @return   Bits 32-63 of a number.

**/
#define UPPER_32_BITS(n) ((UINT32)(((n) >> 16) >> 16))

/**
  Return bits 0-31 of a number.

  @param    n   The number being accessed.

  @return   Bits 0-31 of a number.

**/
#define LOWER_32_BITS(n) ((UINT32)((n) & 0xffffffff))

/**
  Return bits 16-31 of a number.

  @param    n   The number being accessed.

  @return   Bits 16-31 of a number.

**/
#define UPPER_16_BITS(n) ((UINT16)((n) >> 16))

/**
  Return bits 0-15 of a number.

  @param    n   The number being accessed.

  @retrun   Bits 0-15 of a number.

**/
#define LOWER_16_BITS(n) ((UINT16)((n) & 0xffff))

/**
  Repeat a byte value x multiple times.

  NOTE:
  The value x is not checked for > 0xff; larger values produce odd results.

  @param    x   value to repeat.

  @return   An unsigned long value that repeat x multiple times.

**/
#define REPEAT_BYTE(x)  ((~0ul / 0xff) * (x))

/**
  Repeat a byte value x multiple times as a u32 value

  NOTE:
  The value x is not checked for > 0xff; larger values produce odd results.

  @param    x   value to repeat.

  @return   An u32 value that repeat x multiple times.

**/
#define REPEAT_BYTE_U32(x)  LOWER_32_BITS(REPEAT_BYTE(x))

#endif /* ifndef __SPACEMIT_WORD_PART_H__ */
