/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-26
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _WINDOWS_TEXT_INPUT_H_
#define _WINDOWS_TEXT_INPUT_H_

#include <Windows.h>

#include <string>
#include <vector>

#include "remote_action.h"
#include "windows_input_marker.h"

namespace crossdesk {

inline bool BuildWindowsTextInputs(const std::string& text,
                                   std::vector<INPUT>& inputs) {
  inputs.clear();
  if (text.empty() || text.size() > kMaxTextInputBytes ||
      text.find('\0') != std::string::npos) return false;
  // Validate the entire commit before injecting any characters.
  const int bytes = static_cast<int>(text.size());
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        text.data(), bytes, nullptr, 0);
  if (length == 0) return false;
  std::wstring characters(length, L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes,
                          characters.data(), length) != length) return false;

  inputs.reserve(characters.size() * 2);
  for (size_t index = 0; index < characters.size(); ++index) {
    const wchar_t character = characters[index];
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.dwExtraInfo = static_cast<ULONG_PTR>(kInjectedKeyboardInputMarker);
    if (character == L'\r' || character == L'\n' || character == L'\t') {
      input.ki.wVk = character == L'\t' ? VK_TAB : VK_RETURN;
      if (character == L'\r' && index + 1 < characters.size() &&
          characters[index + 1] == L'\n') ++index;
    } else {
      input.ki.wScan = static_cast<WORD>(character);
      input.ki.dwFlags = KEYEVENTF_UNICODE;
    }
    inputs.push_back(input);
    input.ki.dwFlags |= KEYEVENTF_KEYUP;
    inputs.push_back(input);
  }
  return true;
}

// A partial batch must never be replayed. Release its final key if necessary,
// retaining the original count and error for the desktop-routing decision.
template <typename Sender>
UINT SendWindowsTextInputs(std::vector<INPUT>& inputs, Sender send) {
  const UINT count = static_cast<UINT>(inputs.size());
  SetLastError(ERROR_SUCCESS);
  const UINT sent = send(inputs.data(), count);
  if (sent == count) return sent;
  const DWORD error = GetLastError();
  if (sent < count && sent % 2 != 0) send(&inputs[sent], 1);
  SetLastError(error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE);
  return sent;
}

}  // namespace crossdesk

#endif
