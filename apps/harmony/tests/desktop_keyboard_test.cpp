#include "keyboard.h"
#include <cassert>
#include <iostream>
using crossdesk::harmony::desktop::KeyboardState;
int main() {
  KeyboardState keys;
  assert(keys.Translate(2017, true) == "a");
  assert(keys.Translate(2017, false) == "a");
  keys.Translate(2047, true);
  assert(keys.Translate(2017, true) == "A");
  assert(keys.Translate(2017, true) == "A"); // repeat retains the original symbol
  keys.Translate(2047, false);
  assert(keys.Translate(2017, false) == "A"); // Shift was released first
  keys.Translate(2047, true); keys.Translate(2048, true); keys.Translate(2047, false);
  assert(keys.Translate(2001, true) == "!"); // the other Shift is still pressed
  keys.Translate(2001, false); keys.Translate(2048, false);
  assert(keys.Translate(2057, true) == "-"); keys.Translate(2057, false);
  keys.Translate(2074, true); keys.Translate(2074, false);
  assert(keys.Translate(2042, true) == "Z"); keys.Translate(2042, false);
  keys.Translate(2047, true);
  assert(keys.Translate(2042, true) == "z"); keys.Translate(2042, false);
  keys.Translate(2072, true); keys.Reset();
  assert(keys.Pressed().empty());
  for (int code=2090; code<=2101; ++code) {
    auto press=keys.Translate(code,true); assert(!press.empty()); assert(keys.Translate(code,false)==press);
  }
  assert(keys.Translate(2117, true) == ".");
  assert(keys.Translate(99999, true).empty());
  keys.Reset();
  for (auto pair : {std::pair{2047, 2048}, std::pair{2072, 2073}, std::pair{2076, 2077}}) {
    const auto left = keys.Translate(pair.first, true);
    const auto right = keys.Translate(pair.second, true);
    assert(!left.empty() && !right.empty() && left != right);
    assert(keys.Translate(pair.first, false) == left && keys.IsPressed(pair.second));
    assert(keys.Translate(pair.second, false) == right);
  }
  for (int code = 2816; code <= 2827; ++code) {
    const auto press = keys.Translate(code, true);
    assert(!press.empty() && keys.Translate(code, false) == press);
  }
  assert(!keys.Translate(2067, true).empty());
  keys.Reset();
  // The lock state changed while another application had keyboard focus.
  assert(keys.Translate(2017, true, 0, true) == "A");
  assert(keys.Translate(2017, false, 0, false) == "A");
  assert(keys.Translate(2017, true, 0, false) == "a");
  keys.Reset();
  // Prefer the OS layout result to the US fallback, preserving paired releases.
  assert(keys.Translate(2042, true, 'y') == "y");
  assert(keys.Translate(2042, true, 'Y') == "y");
  assert(keys.Translate(2042, false, 'Y') == "y");
  assert(keys.Translate(2057, true, '?') == "?"); keys.Reset();
  using namespace slint::platform::key_codes;
  const auto utf8 = [](std::u8string_view key) { return std::string(reinterpret_cast<const char*>(key.data()), key.size()); };
  assert(keys.Translate(2104, true, '1', {}, false) == utf8(End));
  assert(keys.Translate(2104, false, '1', {}, true) == utf8(End));
  assert(keys.Translate(2104, true, '1', {}, true) == "1"); keys.Reset();
  assert(keys.Translate(2117, true, '.', {}, false) == utf8(Delete)); keys.Reset();
  assert(keys.Translate(2045, true) == utf8(Alt));
  assert(keys.Translate(2046, true) == utf8(AltGr));
  assert(keys.Translate(2045, false) == utf8(Alt) && keys.IsPressed(2046));
  assert(keys.Translate(2046, false) == utf8(AltGr));
  std::cout << "Slint keyboard chords, repeat, focus release, punctuation and function keys passed\n";
}
