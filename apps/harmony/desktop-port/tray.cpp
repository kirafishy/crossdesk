#include "platform/harmony/gui/tray/harmony_tray.h"
#include "bridge.h"
#include "localization.h"
#include "rd_log.h"

namespace crossdesk {
namespace { HarmonyTray* current = nullptr; }
HarmonyTray::HarmonyTray(std::function<void()> show, std::function<void()> hide,
                         std::function<void()> settings, std::function<void()> exit,
                         const std::string& tooltip, int language)
    : show_(std::move(show)), hide_(std::move(hide)),
      settings_(std::move(settings)), exit_(std::move(exit)) {
  current = this;
  language = localization::detail::ClampLanguageIndex(language);
  auto result = harmony::desktop::Request({{"op", "trayCreate"}, {"title", tooltip},
      {"show", localization::show_main_window[language]},
      {"settings", localization::settings[language]},
      {"exit", localization::exit_program[language]}});
  active_ = result.value("ok", false);
  if (!active_) LOG_WARN("Harmony tray unavailable: {}", result.value("message", "unsupported"));
}
HarmonyTray::~HarmonyTray() { RemoveTrayIcon(); if (current == this) current = nullptr; }
bool HarmonyTray::MinimizeToTray() {
  if (!active_) return false;
  hide_();
  return true;
}
void HarmonyTray::RemoveTrayIcon() {
  if (!active_) return;
  active_ = false;
  harmony::desktop::Emit({{"op", "trayRemove"}});
}
void HarmonyTray::Dispatch(const std::string& action) {
  if (!current) return;
  if (action == "show") current->show_();
  else if (action == "settings") current->settings_();
  else if (action == "exit") current->exit_();
}
}
