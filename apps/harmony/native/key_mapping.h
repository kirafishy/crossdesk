/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-14
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _KEY_MAPPING_H_
#define _KEY_MAPPING_H_

#include <utility>

namespace crossdesk::harmony {
// CrossDesk's keyboard wire format uses Windows virtual-key values. HarmonyOS
// key codes are shared by ArkUI KeyEvent and the native Input Kit.
inline constexpr std::pair<int, int> kSpecialKeys[] = {
    {2045, 0xA4}, {2046, 0xA5}, {2047, 0xA0}, {2048, 0xA1},
    {2049, 0x09}, {2050, 0x20}, {2054, 0x0D}, {2055, 0x08},
    {2056, 0xC0}, {2057, 0xBD}, {2058, 0xBB}, {2059, 0xDB},
    {2060, 0xDD}, {2061, 0xDC}, {2062, 0xBA}, {2063, 0xDE},
    {2064, 0xBF}, {2043, 0xBC}, {2044, 0xBE},
    {2070, 0x1B}, {2071, 0x2E}, {2072, 0xA2}, {2073, 0xA3},
    {2074, 0x14}, {2075, 0x91}, {2076, 0x5B}, {2077, 0x5C},
    {2079, 0x2C}, {2080, 0x13}, {2081, 0x24}, {2082, 0x23},
    {2083, 0x2D}, {2068, 0x21}, {2069, 0x22},
    {2012, 0x26}, {2013, 0x28}, {2014, 0x25}, {2015, 0x27},
    {2102, 0x90}, {2113, 0x6F}, {2114, 0x6A}, {2115, 0x6D},
    {2116, 0x6B}, {2117, 0x6E}, {2067, 0x5D}};

inline int ToVirtualKey(int key) {
  if (key >= 2017 && key <= 2042) return key - 2017 + 0x41;
  if (key >= 2000 && key <= 2009) return key - 2000 + 0x30;
  if (key >= 2090 && key <= 2101) return key - 2090 + 0x70;
  if (key >= 2816 && key <= 2827) return key - 2816 + 0x7C;
  if (key >= 2103 && key <= 2112) return key - 2103 + 0x60;
  if (key == 2119) return 0x0D;
  for (auto [native, wire] : kSpecialKeys) if (key == native) return wire;
  return 0;
}

inline int FromVirtualKey(int key, unsigned scan = 0, bool extended = false) {
  scan &= 0xFF;
  if (key == 0x10) return scan == 0x36 ? 2048 : 2047;
  if (key == 0x11) return extended ? 2073 : 2072;
  if (key == 0x12) return extended ? 2046 : 2045;
  if (key == 0x0D && extended) return 2119;
  // Windows reports navigation VKs for the number pad with NumLock off.
  // Preserve the physical key; the host's own NumLock state selects its action.
  if (!extended) {
    constexpr std::pair<int, unsigned> navigation[] = {
        {0x2D, 0x52}, {0x23, 0x4F}, {0x28, 0x50}, {0x22, 0x51}, {0x25, 0x4B},
        {0x0C, 0x4C}, {0x27, 0x4D}, {0x24, 0x47}, {0x26, 0x48}, {0x21, 0x49}};
    for (int i = 0; i < 10; ++i)
      if (key == navigation[i].first && scan == navigation[i].second) return 2103 + i;
    if (key == 0x2E && scan == 0x53) return 2117;
  }
  if (key >= 0x41 && key <= 0x5A) return key - 0x41 + 2017;
  if (key >= 0x30 && key <= 0x39) return key - 0x30 + 2000;
  if (key >= 0x70 && key <= 0x7B) return key - 0x70 + 2090;
  if (key >= 0x7C && key <= 0x87) return key - 0x7C + 2816;
  if (key >= 0x60 && key <= 0x69) return key - 0x60 + 2103;
  for (auto [native, wire] : kSpecialKeys) if (key == wire) return native;
  return 0;
}
}  // namespace crossdesk::harmony

#endif
