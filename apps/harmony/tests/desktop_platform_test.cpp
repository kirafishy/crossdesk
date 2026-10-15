#include "displays.h"
#include "pointer.h"
#include "window_mode.h"
#include <cassert>
#include <tuple>

int main() {
  using namespace crossdesk::harmony::desktop;
  using Json = nlohmann::json;
  const auto displays = ReadDisplays(Json::parse(R"({"displays":[
    {"id":42,"name":"Left","x":-2560,"y":-300,"width":2560,"height":1440},
    {"id":0,"name":"Main","primary":true,"width":1920,"height":1080,"usableY":30,"usableHeight":990},
    {"id":42,"width":2560,"height":1440},
    {"id":7,"width":0,"height":1080},
    {"id":-1,"width":1920,"height":1080}
  ]})"));
  assert(displays.size() == 2 && displays[0].id == 0 && displays[1].id == 42);
  assert(displays[0].SdlId() == 1 && displays[1].SdlId() == 43);
  assert(displays[0].usable_y == 30 && displays[0].usable_height == 990);
  assert(displays[1].usable_x == -2560 && displays[1].usable_y == -300);
  assert(DisplayAt(displays, -1, 0)->id == 42);
  assert(DisplayAt(displays, 0, 0)->id == 0);
  assert(DisplayAt(displays, -1280, -800)->id == 42); // Outside monitors: nearest display.
  assert(DisplayAt(displays, 3000, 100)->id == 0);
  const auto removed = ReadDisplays(Json::parse(R"({"displays":[{"id":42,"primary":true,"width":2560,"height":1440}]})"));
  assert(removed[0].SdlId() == displays[1].SdlId()); // SDL identity survives primary changes.
  assert(ReadDisplays(Json::object()).empty());
  assert(DisplayAt({}, 0, 0) == nullptr);

  PointerState pointer;
  std::vector<std::tuple<int, float, float>> releases;
  const auto release = [&](int b, float x, float y) { releases.emplace_back(b, x, y); };
  pointer.Update(0, 0, 1, 2);
  pointer.Update(0, 1, 10, 20);
  pointer.Update(0, 2, 10, 20);
  assert(pointer.Buttons() == 7);
  pointer.Update(1, 1, 12, 22); // Right release does not clear left/middle drag.
  assert(pointer.Buttons() == 3);
  pointer.Update(2, 0, -15, 130); // Drag beyond the surface preserves held state.
  pointer.Cancel(release);
  assert(pointer.Buttons() == 0 && releases.size() == 2);
  assert((releases[0] == std::tuple{0, -15.f, 130.f}));
  assert((releases[1] == std::tuple{2, -15.f, 130.f}));
  pointer.Cancel(release);
  assert(releases.size() == 2); // Repeated focus/detach/cancel has no duplicate releases.

  WindowMode modes;
  Json slint{{"fullscreen", false}, {"maximized", false}, {"minimized", false}};
  assert(modes.Request(slint) == slint);
  assert(modes.Request(slint).empty());
  // Native titlebar maximize/restore: all three Slint setters synchronously
  // call back, including the first one before the other flags have changed.
  for (bool maximized : {true, false, true, false}) {
    Json actual = slint; actual["maximized"] = maximized;
    modes.Acknowledge(actual, [&] {
      for (auto& [name, value] : actual.items()) {
        slint[name] = value;
        assert(modes.Request(slint).empty());
      }
    });
    assert(modes.Request(slint).empty());
  }
  slint["fullscreen"] = true;
  assert(modes.Request(slint) == Json({{"fullscreen", true}}));
  modes.Acknowledge(Json{{"minimized", true}}, [&] { slint["minimized"] = true; });
  assert(modes.Request(slint).empty()); // Minimize preserves fullscreen.
  slint["minimized"] = false;
  assert(modes.Request(slint) == Json({{"minimized", false}}));
}
