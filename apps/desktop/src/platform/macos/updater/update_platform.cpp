#include "updater/update_platform.h"

#include <libproc.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace crossdesk::updater {
namespace {
std::string ShellQuote(const std::string& value) {
  std::string result = "'";
  for (char c : value) result += c == '\'' ? "'\\''" : std::string(1, c);
  return result + "'";
}
std::string AppleScriptString(const std::string& value) {
  std::string result = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"') result += '\\';
    result += c;
  }
  return result + "\"";
}
}  // namespace
fs::path ExecutablePath() {
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size);
  if (_NSGetExecutablePath(buffer.data(), &size)) throw std::runtime_error("Cannot locate executable");
  return fs::canonical(buffer.data());
}
bool SupportedInstallation(const fs::path& application) {
  return application == fs::path("/Applications/CrossDesk.app/Contents/MacOS/CrossDesk") &&
         fs::is_regular_file(HelperPath(application));
}
int RunElevated(const fs::path& executable, const fs::path& job) {
  const auto command = ShellQuote(executable.string()) + " --install " + ShellQuote(job.string());
  return Run("/usr/bin/osascript", {"-e", "do shell script " +
             AppleScriptString(command) + " with administrator privileges"});
}
void WaitForApplicationExit(const fs::path& application) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
  do {
    const int count = proc_listallpids(nullptr, 0);
    if (count <= 0) throw std::runtime_error("Cannot enumerate CrossDesk processes");
    std::vector<pid_t> pids(static_cast<std::size_t>(count) + 256);
    const int length = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    if (length <= 0 || length >= static_cast<int>(pids.size()))
      throw std::runtime_error("Cannot inspect all CrossDesk processes");
    bool running = false;
    for (int i = 0; i < length; ++i) {
      char path[PROC_PIDPATHINFO_MAXSIZE]{};
      if (proc_pidpath(pids[i], path, sizeof(path)) > 0) {
        if (fs::path(path) == application) running = true;
      } else {
        char name[1024]{};
        if (proc_name(pids[i], name, sizeof(name)) > 0 && std::string(name) == "CrossDesk")
          running = true;
      }
    }
    if (!running) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("CrossDesk is still running in another session");
}
void InstallPackage(const fs::path& package, const fs::path&) {
  if (Run("/usr/sbin/installer", {"-pkg", package.string(), "-target", "/"}) != 0)
    throw std::runtime_error("The macOS installer could not install the update");
}
std::vector<std::string> RestartArguments() { return {}; }
void Restart(const fs::path&, const std::vector<std::string>&) {
  Launch("/usr/bin/open", {"/Applications/CrossDesk.app"});
}
}  // namespace crossdesk::updater
