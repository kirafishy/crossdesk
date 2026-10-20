#include "updater/update_platform.h"

#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace crossdesk::updater {
namespace {
std::vector<std::string> HeadlessArguments(const std::vector<std::string>& arguments) {
  std::vector<std::string> result;
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    const auto& argument = arguments[i];
    if (argument == "--headless" || argument == "--no-headless") {
      result.push_back(argument);
    } else if (argument == "--headless-display" || argument == "--headless-size" ||
               argument == "--headless-session") {
      if (i + 1 == arguments.size() || arguments[i + 1].empty() ||
          arguments[i + 1].rfind("--", 0) == 0)
        throw std::runtime_error("Missing headless restart argument");
      result.push_back(argument);
      result.push_back(arguments[++i]);
    }
    // Internal modes such as --child must never replace a normal restart.
  }
  return result;
}
}  // namespace

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
std::vector<std::string> RestartArguments() {
  // The built-in headless supervisor forks the GUI without exec, so procfs
  // still contains its original display/session options in the GUI process.
  std::istringstream command_line(ReadText("/proc/self/cmdline", 32768));
  std::string argument;
  std::getline(command_line, argument, '\0');  // Executable name.
  std::vector<std::string> arguments;
  while (std::getline(command_line, argument, '\0')) arguments.push_back(argument);
  return HeadlessArguments(arguments);
}
void Restart(const fs::path&, const std::vector<std::string>& arguments) {
  Launch("/usr/bin/crossdesk", HeadlessArguments(arguments));
}
}  // namespace crossdesk::updater
