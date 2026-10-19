/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-19
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _TRANSFER_NOTIFICATION_LAYOUT_H_
#define _TRANSFER_NOTIFICATION_LAYOUT_H_

#include <algorithm>
#include <cstddef>

namespace crossdesk::transfer_notification {

constexpr float kWidth = 360;
constexpr float kHeight = 132;
constexpr float kMargin = 16;
constexpr float kGap = 10;

struct Layout {
  float width, height;
  size_t rows, columns;
  size_t capacity() const { return rows * columns; }
};

inline Layout Fit(float width, float height) {
  const float card_width = std::min(kWidth, std::max(1.0f, width - 2 * kMargin));
  const float card_height = std::min(kHeight, std::max(1.0f, height - 2 * kMargin));
  const auto count = [](float available, float card) {
    return static_cast<size_t>(std::max(1.0f, (available - 2 * kMargin + kGap) / (card + kGap)));
  };
  return {card_width, card_height, count(height, card_height), count(width, card_width)};
}

struct Position { float x, y; };

inline Position Place(const Layout& layout, float x, float y,
                      float width, float height, size_t index) {
  // Fill upward from the bottom-right, then continue in a column to the left.
  return {x + std::max(0.0f, width - kMargin - layout.width) -
              (index / layout.rows) * (layout.width + kGap),
          y + std::max(0.0f, height - kMargin - layout.height) -
              (index % layout.rows) * (layout.height + kGap)};
}

}  // namespace crossdesk::transfer_notification

#endif
