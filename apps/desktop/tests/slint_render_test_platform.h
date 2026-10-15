/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SLINT_RENDER_TEST_PLATFORM_H_
#define _SLINT_RENDER_TEST_PLATFORM_H_

#include <slint-platform.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

// Retain pixels and honor redraw notifications, as the Harmony adapter does.
// take_snapshot() forces a repaint and can conceal a stalled popup/animation.
class RenderTestWindow : public slint::platform::WindowAdapter {
 public:
  slint::platform::SoftwareRenderer software{
      slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer};
  slint::PhysicalSize extent{{1280, 720}};
  std::vector<slint::Rgb8Pixel> pixels = std::vector<slint::Rgb8Pixel>(1280 * 720);
  bool dirty = true;
  slint::PhysicalSize size() override { return extent; }
  slint::platform::AbstractRenderer& renderer() override { return software; }
  void request_redraw() override { dirty = true; }
  void set_size(slint::PhysicalSize value) override {
    extent = value;
    pixels.resize(extent.width * extent.height);
    window().dispatch_resize_event(slint::LogicalSize(
        {float(extent.width), float(extent.height)}));
    dirty = true;
  }
  void Render() {
    if (!dirty && !window().has_active_animations()) return;
    dirty = false;
    software.render(pixels, extent.width);
  }
  void Settle(int frames = 15) {
    for (int i = 0; i < frames; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      slint::platform::update_timers_and_animations();
      Render();
    }
  }
  void Click(float x, float y, int hold_frames = 0) {
    const slint::LogicalPosition point({x, y});
    window().dispatch_pointer_move_event(point);
    window().dispatch_pointer_press_event(point, slint::PointerEventButton::Left);
    Settle(hold_frames);
    window().dispatch_pointer_release_event(point, slint::PointerEventButton::Left);
    Settle(); // Do not move the pointer to make a delayed repaint pass.
  }
  bool Light(int x, int y) const {
    const auto p = pixels.at(y * extent.width + x);
    return p.r >= 230 && p.g >= 230 && p.b >= 230;
  }
  bool Black(int x, int y) const {
    const auto p = pixels.at(y * extent.width + x);
    return p.r == 0 && p.g == 0 && p.b == 0;
  }
  std::uint64_t Region(int x, int y, int width, int height) const {
    std::uint64_t hash = 14695981039346656037ull;
    for (int row = y; row < y + height; ++row)
      for (int col = x; col < x + width; ++col) {
        const auto p = pixels.at(row * extent.width + col);
        for (auto channel : {p.r, p.g, p.b}) hash = (hash ^ channel) * 1099511628211ull;
      }
    return hash;
  }
  void Key(const slint::SharedString& key) {
    window().dispatch_key_press_event(key);
    window().dispatch_key_release_event(key);
    Settle();
  }
};

class RenderTestPlatform : public slint::platform::Platform {
 public:
  RenderTestWindow* adapter = nullptr;
  std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
    auto result = std::make_unique<RenderTestWindow>();
    adapter = result.get();
    return result;
  }
};

#endif
