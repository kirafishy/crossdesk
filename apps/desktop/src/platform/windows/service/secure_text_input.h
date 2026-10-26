/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-26
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SECURE_TEXT_INPUT_H_
#define _SECURE_TEXT_INPUT_H_

#include <nlohmann/json.hpp>
#include <string>

#include "remote_action.h"

namespace crossdesk {

inline constexpr char kSecureTextInputCommandPrefix[] = "text:";
// JSON can escape each text byte as six ASCII bytes. Leave room for metadata.
inline constexpr size_t kSecureInputCommandBufferBytes =
    6 * kMaxTextInputBytes + 1024;

struct SecureTextInputRequest {
  std::string text;
  std::string stage;
  std::string desktop;
};

inline std::string BuildSecureTextInputCommand(
    const std::string& text, const std::string& stage = {},
    const std::string& desktop = {}) {
  return kSecureTextInputCommandPrefix +
         nlohmann::json{{"text", text}, {"stage", stage},
                        {"desktop", desktop}}.dump();
}

inline bool ParseSecureTextInputCommand(const std::string& command,
                                        SecureTextInputRequest& request) {
  request = {};
  if (command.size() > kSecureInputCommandBufferBytes ||
      command.rfind(kSecureTextInputCommandPrefix, 0) != 0) return false;
  const auto json = nlohmann::json::parse(
      command.substr(sizeof(kSecureTextInputCommandPrefix) - 1), nullptr, false);
  if (!json.is_object() || !json.contains("text") ||
      !json["text"].is_string() || !json.contains("stage") ||
      !json["stage"].is_string() || !json.contains("desktop") ||
      !json["desktop"].is_string()) return false;
  const auto& text = json["text"].get_ref<const std::string&>();
  const auto& stage = json["stage"].get_ref<const std::string&>();
  const auto& desktop = json["desktop"].get_ref<const std::string&>();
  if (text.empty() || text.size() > kMaxTextInputBytes ||
      text.find('\0') != std::string::npos || stage.size() > 64 ||
      desktop.size() > 256 || stage.find('\0') != std::string::npos ||
      desktop.find('\0') != std::string::npos) return false;
  request = {text, stage, desktop};
  return true;
}

}  // namespace crossdesk

#endif
