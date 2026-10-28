/*
 * @Author: DI JUNKUN
 * @Date: 2026-09-11
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#include "privacy_cover_renderer.h"

#include <algorithm>
#include <array>

namespace crossdesk {
namespace {

constexpr size_t kInstructionLines = 2;
constexpr size_t kBrandSegments = 2;
constexpr size_t kCrossLine = kInstructionLines;
constexpr size_t kDeskLine = kCrossLine + 1;

struct PaintResources {
  HDC dc;
  int saved;
  HICON logo = nullptr;
  std::array<HFONT, kInstructionLines + kBrandSegments> fonts{};
  ~PaintResources() {
    // Deselect every owned GDI object before releasing it.
    if (saved) RestoreDC(dc, saved);
    for (auto font : fonts)
      if (font) DeleteObject(font);
    if (logo) DestroyIcon(logo);
  }
};

bool Select(HDC dc, HGDIOBJ object) {
  const auto previous = SelectObject(dc, object);
  return previous && previous != HGDI_ERROR;
}

}  // namespace

bool DrawPrivacyCover(HDC dc, const RECT& rect, UINT dpi, HINSTANCE module,
                      const wchar_t* unlock_hint, bool allow_local_unlock) {
  if (!FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))))
    return false;
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) return false;
  // Scale with the monitor DPI, shrinking the whole group on small displays.
  // All coordinates remain in the owning window's physical-pixel context.
  const double scale = (std::min)({dpi / 96.0, width / 768.0, height / 200.0});
  const auto px = [scale](int value) {
    return (std::max)(1, static_cast<int>(value * scale + 0.5));
  };
  const int margin = px(32);
  const int text_width = px(398);
  PaintResources resources{dc, SaveDC(dc)};
  if (!resources.saved || !SetBkMode(dc, TRANSPARENT)) return false;

  struct Line {
    const wchar_t* text;
    int size;
    int weight;
    COLORREF color;
    RECT bounds{};
  };
  std::array<Line, kInstructionLines + kBrandSegments> lines{{
      {unlock_hint, 16, FW_NORMAL, RGB(181, 190, 204)},
      {L"Ctrl + Alt + Shift + F12", 24, FW_SEMIBOLD, RGB(245, 247, 250)},
      {L"Cross", 29, FW_BOLD, RGB(69, 135, 219)},
      {L"Desk", 29, FW_BOLD, RGB(37, 83, 151)},
  }};
  constexpr UINT flags = DT_LEFT | DT_WORDBREAK | DT_NOPREFIX;
  constexpr UINT brand_flags = DT_LEFT | DT_SINGLELINE | DT_NOPREFIX;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i < kInstructionLines && !allow_local_unlock) continue;
    auto& line = lines[i];
    const bool is_brand = i >= kInstructionLines;
    resources.fonts[i] =
        CreateFontW(-px(line.size), 0, 0, 0, line.weight, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    ANTIALIASED_QUALITY, DEFAULT_PITCH,
                    is_brand ? L"Arial" : L"Microsoft YaHei UI");
    if (!resources.fonts[i] || !Select(dc, resources.fonts[i])) return false;
    line.bounds = {0, 0, text_width, 0};
    if (!DrawTextW(dc, line.text, -1, &line.bounds,
                   (is_brand ? brand_flags : flags) | DT_CALCRECT) ||
        line.bounds.right > text_width)
      return false;
  }
  const int content_height =
      static_cast<int>(lines[0].bounds.bottom + lines[1].bounds.bottom) +
      (allow_local_unlock ? px(12) : 0);
  // Match the horizontal, two-tone wordmark in docs/images/crossdesk-banner.svg.
  const int logo_size = px(48);
  const int brand_gap = px(8);
  const int segment_gap = px(1);
  const int brand_width = logo_size + brand_gap + segment_gap +
      static_cast<int>(lines[kCrossLine].bounds.right + lines[kDeskLine].bounds.right);
  const int brand_height = (std::max)({logo_size,
      static_cast<int>(lines[kCrossLine].bounds.bottom),
      static_cast<int>(lines[kDeskLine].bounds.bottom)});
  if (logo_size <= 0 ||
      (std::max)(brand_height, content_height) + 2 * margin > height ||
      brand_width + (allow_local_unlock ? px(24) + text_width : 0) + 2 * margin > width)
    return false;
  resources.logo = static_cast<HICON>(LoadImageW(
      module, L"IDI_CROSSDESK_LOGO", IMAGE_ICON, logo_size, logo_size, 0));
  if (!resources.logo) return false;
  const int brand_left = rect.left + margin;
  const int left = brand_left + brand_width + px(24);
  const int brand_bottom = rect.bottom - margin;
  int top = rect.bottom - margin - content_height;
  if (!DrawIconEx(dc, brand_left, brand_bottom - logo_size,
                  resources.logo, logo_size,
                  logo_size, 0, nullptr, DI_NORMAL))
    return false;
  int brand_text_left = brand_left + logo_size + brand_gap;
  for (size_t i = kCrossLine; i <= kDeskLine; ++i) {
    const auto& line = lines[i];
    if (!Select(dc, resources.fonts[i])) return false;
    const int brand_text_top = brand_bottom - line.bounds.bottom;
    RECT bounds{brand_text_left, brand_text_top,
                brand_text_left + line.bounds.right,
                brand_text_top + line.bounds.bottom};
    SetTextColor(dc, line.color);
    if (!DrawTextW(dc, line.text, -1, &bounds, brand_flags)) return false;
    brand_text_left += line.bounds.right + segment_gap;
  }
  if (!allow_local_unlock) return true;
  for (size_t i = 0; i < kInstructionLines; ++i) {
    const auto& line = lines[i];
    if (!Select(dc, resources.fonts[i])) return false;
    RECT bounds{left, top, left + text_width, top + line.bounds.bottom};
    SetTextColor(dc, line.color);
    if (!DrawTextW(dc, line.text, -1, &bounds, flags)) return false;
    top += line.bounds.bottom + px(12);
  }
  return true;
}

}  // namespace crossdesk
