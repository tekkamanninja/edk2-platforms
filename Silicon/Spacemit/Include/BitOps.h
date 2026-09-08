/** @file
  Common bit operations

  Copyright (c) 2024, SpacemiT Co., Ltd. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SPACEMIT_BIT_OPS_H__
#define __SPACEMIT_BIT_OPS_H__

#define BITS_PER_LONG       (sizeof (long) * 8)
#define BITS_PER_LONG_LONG  (sizeof (long long) * 8)

#define BIT(nr)         (1UL << (nr))
#define BIT_ULL(nr)     (1ULL << (nr))

#define GENMASK(h, l) \
  (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l) \
  (((~0ULL) - (1ULL << (l)) + 1) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

/**
  Find first bit set in a 64-bit value.

  Note:
  The first bit is at position 1.
  e.g. Ffs64(0) = 0, Ffs64(1) = 1, Ffs64(0x8000000000000000) = 64.

  @param  Val       The value to search.

  @retval 0         If Val is 0.
  @retval Other     The position of the first set bit.

**/
STATIC
inline
INTN
EFIAPI
Ffs64 (
  IN UINT64   Val
  )
{
  INTN Ret = 1;

  if (Val == 0) {
    return 0;
  }
  if ((Val & 0xffffffff) == 0) {
    Val >>= 32;
    Ret += 32;
  }
  if ((Val & 0xffff) == 0) {
    Val >>= 16;
    Ret += 16;
  }
  if ((Val & 0xff) == 0) {
    Val >>= 8;
    Ret += 8;
  }
  if ((Val & 0xf) == 0) {
    Val >>= 4;
    Ret += 4;
  }
  if ((Val & 0x3) == 0) {
    Val >>= 2;
    Ret += 2;
  }
  if ((Val & 0x1) == 0) {
    Val >>= 1;
    Ret += 1;
  }
  return Ret;
}

/**
  Find first bit set.

  Note:
  The first bit is at position 1.
  e.g. Ffs(0) = 0, Ffs(1) = 1, Ffs(0x80000000) = 32.

  @param  Val       The value to search.

  @retval 0         If Val is 0.
  @retval Other     The position of the first set bit.

**/
STATIC
inline
INTN
EFIAPI
Ffs (
  IN UINT32   Val
  )
{
  return Ffs64 ((UINT64) Val);
}

/**
  Find last (most-significant) bit set.

  Note:
  The last (most significant) bit is at position 32.
  e.g. Fls(0) = 0, Fls(1) = 1, Fls(0x80000000) = 32.

  @param  Val       The value to search.

  @retval 0         If Val is 0.
  @retval Other     The position of the last set bit.

**/
STATIC
inline
INTN
EFIAPI
Fls (
  IN UINT32   Val
  )
{
  INTN Ret = 32;

  if (Val == 0) {
    return 0;
  }
  if ((Val & 0xffff0000u) == 0) {
    Val <<= 16;
    Ret -= 16;
  }
  if ((Val & 0xff000000u) == 0) {
    Val <<= 8;
    Ret -= 8;
  }
  if ((Val & 0xf0000000u) == 0) {
    Val <<= 4;
    Ret -= 4;
  }
  if ((Val & 0xc0000000u) == 0) {
    Val <<= 2;
    Ret -= 2;
  }
  if ((Val & 0x80000000u) == 0) {
    Val <<= 1;
    Ret -= 1;
  }
  return Ret;
}

/**
  Find last (most-significant) bit set in a 64-bit value.

  Note:
  The last (most significant) bit is at position 64.
  e.g. Fls64(0) = 0, Fls64(1) = 1, Fls(0x8000000000000000) = 64.

  @param  Val       The value to search.

  @retval 0         If Val is 0.
  @retval Other     The position of the last set bit.

**/
STATIC
inline
INTN
EFIAPI
Fls64 (
  IN UINT64   Val
  )
{
  UINT32 High = (UINT32) (Val >> 32);
  if (High != 0) {
    return Fls (High) + 32;
  }
  return Fls ((UINT32) Val);
}

#endif /* ifndef __SPACEMIT_BIT_OPS_H__ */
