/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _RELEASE_NOTES_TEXT_H_
#define _RELEASE_NOTES_TEXT_H_

#include <slint.h>

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>
#include <vector>

namespace crossdesk::release_notes_text {

struct Block {
  slint::StyledText content;
  bool section_gap = false;
};

inline bool IsThematicBreak(std::string_view line) {
  const auto start = line.find_first_not_of(' ');
  // Four leading spaces denote a code block, not a thematic break.
  if (start == std::string_view::npos || start > 3) return false;
  const char marker = line[start];
  if (marker != '-' && marker != '*' && marker != '_') return false;
  size_t count = 0;
  for (const char ch : line.substr(start)) {
    if (ch == marker)
      ++count;
    else if (ch != ' ' && ch != '\t')
      return false;
  }
  return count >= 3;
}

inline std::vector<Block> Parse(std::string_view markdown) {
  std::vector<Block> blocks;
  bool section_gap = false;
  size_t line_start = 0;
  while (line_start < markdown.size()) {
    const size_t newline = markdown.find('\n', line_start);
    const size_t line_end =
        newline == std::string_view::npos ? markdown.size() : newline;
    auto line = markdown.substr(line_start, line_end - line_start);
    line_start = newline == std::string_view::npos ? markdown.size() : newline + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    // StyledText does not support thematic breaks. Represent them with the
    // same spacing as blank lines instead of rendering raw Markdown markers.
    if (std::all_of(line.begin(), line.end(), [](unsigned char ch) {
          return std::isspace(ch) != 0;
        }) || IsThematicBreak(line)) {
      section_gap = !blocks.empty();
      continue;
    }

    size_t heading_end = 0;
    while (heading_end < line.size() && heading_end < 6 &&
           line[heading_end] == '#') {
      ++heading_end;
    }
    // The dialog already supplies the release title.
    if (heading_end > 0 && heading_end < line.size() &&
        std::isspace(static_cast<unsigned char>(line[heading_end])) != 0) {
      continue;
    }

    Block block;
    if (const auto parsed = slint::StyledText::from_markdown(line)) {
      block.content = *parsed;
    } else {
      // Unsupported syntax is valid release content, not a parsing failure.
      // Preserve it literally when it is outside Slint's Markdown subset.
      block.content = slint::StyledText::from_plain_text(line);
    }
    block.section_gap = section_gap;
    blocks.push_back(std::move(block));
    section_gap = false;
  }
  return blocks;
}

}  // namespace crossdesk::release_notes_text

#endif
