#include "updater/update_platform.h"

#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace crossdesk::updater {
fs::path ExecutablePath() { return fs::read_symlink("/proc/self/exe"); }
bool SupportedInstallation(const fs::path& application) {
  return application == fs::path("/usr/lib/crossdesk/crossdesk-bin") &&
         !PlatformKey(application).empty() && fs::is_regular_file(HelperPath(application)) &&
         access("/usr/bin/pkexec", X_OK) == 0 && access("/usr/bin/apt-get", X_OK) == 0;
}
int RunElevated(const fs::path& executable, const fs::path& job) {
  return Run("/usr/bin/pkexec", {executable.string(), "--install", job.string()});
}
void WaitForApplicationExit(const fs::path& application) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
  do {
    bool running = false;
    for (const auto& entry : fs::directory_iterator("/proc")) {
      const auto name = entry.path().filename().string();
      if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
      std::error_code error;
      const auto executable = fs::read_symlink(entry.path() / "exe", error);
      if (!error && executable == application) running = true;
      // Fail closed when a matching process cannot be inspected. A disappeared
      // process has no comm file and does not block installation.
      if (error) {
        std::ifstream comm(entry.path() / "comm");
        std::string process_name;
        std::getline(comm, process_name);
        if (process_name == "crossdesk-bin") running = true;
      }
    }
    if (!running) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("CrossDesk is still running; stop other sessions or its supervisor and retry");
}
void InstallPackage(const fs::path& package, const fs::path&) {
  // No shell and no automatic dependency removal. apt owns dpkg locking and
  // dependency resolution; a busy/broken package manager is reported as failure.
  setenv("DEBIAN_FRONTEND", "noninteractive", 1);
  if (Run("/usr/bin/apt-get", {"install", "--yes", "--no-remove",
                              "-o", "Dpkg::Options::=--force-confold", package.string()}) != 0)
    throw std::runtime_error("The package manager could not install the update");
}
void Restart(const fs::path&) { Launch("/usr/bin/crossdesk", {}); }
}  // namespace crossdesk::updater
