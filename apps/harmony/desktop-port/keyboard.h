/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-14
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include <slint-platform.h>
#include <map>
#include <optional>
#include <string>

namespace crossdesk::harmony::desktop {
// Remember the symbol chosen on key-down. Releasing Shift before a letter must
// not turn the matching key-up into a different remote key.
class KeyboardState {
  std::map<int, std::string> pressed_;
  bool caps_ = false;
  bool num_ = true;
  static std::string Utf8(std::u8string_view key) { return {reinterpret_cast<const char*>(key.data()), key.size()}; }
 public:
  void Reset() { pressed_.clear(); }
  bool IsPressed(int code) const { return pressed_.count(code); }
  const std::map<int, std::string>& Pressed() const { return pressed_; }
  std::string Translate(int code, bool down, unsigned unicode = 0,
      std::optional<bool> caps = {}, std::optional<bool> num = {}) {
    // Lock keys may have changed in another application. Prefer the OS event
    // snapshot, while keeping a down/up pair's original symbol unchanged.
    if (caps) caps_ = *caps;
    if (num) num_ = *num;
    if (auto it = pressed_.find(code); it != pressed_.end()) {
      auto key = it->second;
      if (!down) pressed_.erase(it);
      return key;
    }
    using namespace slint::platform::key_codes;
    const bool shift = IsPressed(2047) || IsPressed(2048);
    std::string key;
    if (!num_ && code >= 2103 && code <= 2112) {
      const std::u8string_view navigation[] = {Insert, End, DownArrow, PageDown, LeftArrow, u8"", RightArrow, Home, UpArrow, PageUp};
      key = Utf8(navigation[code - 2103]);
    } else if (!num_ && code == 2117) key = Utf8(Delete);
    else if (unicode >= 0x21 && unicode <= 0x7e &&
        ((code >= 2000 && code <= 2009) || (code >= 2017 && code <= 2044) ||
         (code >= 2056 && code <= 2064) || (code >= 2103 && code <= 2122)))
      key = std::string(1, char(unicode));
    else if (code >= 2000 && code <= 2009) key = std::string(1, shift ? ")!@#$%^&*("[code - 2000] : char('0' + code - 2000));
    else if (code >= 2017 && code <= 2042) key = std::string(1, char(((shift != caps_) ? 'A' : 'a') + code - 2017));
    else if (code >= 2103 && code <= 2112) key = std::string(1, char('0' + code - 2103));
    else if (code >= 2090 && code <= 2101) {
      const std::u8string_view functions[] = {F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12};
      key = Utf8(functions[code - 2090]);
    } else if (code >= 2816 && code <= 2827) {
      const std::u8string_view functions[] = {F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24};
      key = Utf8(functions[code - 2816]);
    } else switch (code) {
      case 2055: key = Utf8(Backspace); break; case 2054: case 2119: key = Utf8(Return); break;
      case 2049: key = Utf8(Tab); break; case 2070: key = Utf8(Escape); break;
      case 2012: key = Utf8(UpArrow); break; case 2013: key = Utf8(DownArrow); break;
      case 2014: key = Utf8(LeftArrow); break; case 2015: key = Utf8(RightArrow); break;
      case 2072: key = Utf8(Control); break; case 2073: key = Utf8(ControlR); break;
      case 2047: key = Utf8(Shift); break; case 2048: key = Utf8(ShiftR); break;
      case 2045: key = Utf8(Alt); break; case 2046: key = Utf8(AltGr); break;
      case 2076: key = Utf8(Meta); break; case 2077: key = Utf8(MetaR); break;
      case 2067: key = Utf8(Menu); break;
      case 2050: key = " "; break; case 2071: key = Utf8(Delete); break;
      case 2081: key = Utf8(Home); break; case 2082: key = Utf8(End); break;
      case 2068: key = Utf8(PageUp); break; case 2069: key = Utf8(PageDown); break;
      case 2083: key = Utf8(Insert); break;
      case 2074: if (down && !caps) caps_ = !caps_; key = Utf8(CapsLock); break;
      case 2102: if (down && !num) num_ = !num_; break;
      case 2075: key = Utf8(ScrollLock); break; case 2080: key = Utf8(Pause); break;
      case 2079: key = Utf8(SysReq); break; case 2113: key = "/"; break;
      case 2114: key = "*"; break; case 2115: key = "-"; break;
      case 2116: key = "+"; break; case 2117: key = "."; break;
      case 2118: key = ","; break; case 2120: key = "="; break;
      case 2121: key = "("; break; case 2122: key = ")"; break;
      case 2043: key = shift ? "<" : ","; break; case 2044: key = shift ? ">" : "."; break;
      case 2056: key = shift ? "~" : "`"; break; case 2057: key = shift ? "_" : "-"; break;
      case 2058: key = shift ? "+" : "="; break; case 2059: key = shift ? "{" : "["; break;
      case 2060: key = shift ? "}" : "]"; break; case 2061: key = shift ? "|" : "\\"; break;
      case 2062: key = shift ? ":" : ";"; break; case 2063: key = shift ? "\"" : "'"; break;
      case 2064: key = shift ? "?" : "/"; break;
    }
    if (down && (!key.empty() || code == 2102)) pressed_[code] = key;
    return key;
  }
};
}

#endif
