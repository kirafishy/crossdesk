#include "platform/instance_activation.h"
#include "platform/single_instance.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <utility>

namespace crossdesk::platform {
namespace {

constexpr std::string_view kRequest = "CrossDesk.Activate.v1";
constexpr std::size_t kMaxActivationToken = 4096;

#if defined(__linux__)
std::string Environment(const char* name) {
  const char* value = std::getenv(name);
  return value ? value : "";
}
#endif

std::string ActivationDirectory() {
  // Match the single-instance guard's user scope, independent of TMPDIR/HOME.
  return "/tmp/crossdesk-activation-" + std::to_string(geteuid());
}

bool PrivateDirectory(bool create, std::string& error) {
  const auto directory = ActivationDirectory();
  if (create && mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) {
    error = "Cannot create activation directory: " + std::string(strerror(errno));
    return false;
  }
  struct stat info{};
  if (lstat(directory.c_str(), &info) != 0) {
    if (errno != ENOENT)
      error = "Cannot inspect activation directory: " + std::string(strerror(errno));
    return false;
  }
  if (!S_ISDIR(info.st_mode) || info.st_uid != geteuid() ||
      (info.st_mode & 0777) != 0700) {
    error = "Activation directory must be private and owned by this user";
    return false;
  }
  return true;
}

std::string ActivationPath(const std::string& application, std::string& error) {
  if (!ValidInstanceApplication(application)) {
    error = "Invalid activation application name";
    return {};
  }
  std::string session = "macos";
#if defined(__linux__)
  if (!Environment("CROSSDESK_HEADLESS_ACTIVE").empty()) {
    error = "Window activation is unavailable in a headless session";
    return {};
  }
  const auto wayland = Environment("WAYLAND_DISPLAY");
  auto display = Environment("DISPLAY");
  if (!wayland.empty()) {
    session = "wayland:" + Environment("XDG_RUNTIME_DIR") + "/" + wayland;
  } else if (!display.empty()) {
    // :0 and :0.0 address the same server; a screen suffix is not a session.
    const auto colon = display.rfind(':');
    const auto screen = display.find('.', colon == std::string::npos ? 0 : colon);
    if (screen != std::string::npos) display.resize(screen);
    if (display.rfind("unix:", 0) == 0) display.erase(0, 4);
    session = "x11:" + display;
  } else {
    error = "No graphical session is available to activate the existing window";
    return {};
  }
#endif
  // A stable bounded name fits both Darwin and Linux sockaddr_un limits.
  std::uint64_t hash = 14695981039346656037ull;
  for (unsigned char byte : application + '\0' + session) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return ActivationDirectory() + "/" + std::to_string(hash) + ".sock";
}

int CreateSocket(std::string& error) {
  const int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0) {
    error = "Cannot create activation socket: " + std::string(strerror(errno));
    return -1;
  }
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
      fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
    error = "Cannot configure activation socket: " + std::string(strerror(errno));
    close(fd);
    return -1;
  }
  return fd;
}

sockaddr_un Address(const std::string& path) {
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  // ActivationPath always produces a name shorter than sun_path on both OSes.
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
  return address;
}

socklen_t AddressSize(const std::string& path) {
  return static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
}

}  // namespace

InstanceActivationReceiver::InstanceActivationReceiver(std::string application)
    : application_(std::move(application)) {}

InstanceActivationReceiver::~InstanceActivationReceiver() {
  if (socket_ < 0) return;
  struct stat info{};
  if (lstat(socket_path_.c_str(), &info) == 0 &&
      static_cast<std::uint64_t>(info.st_dev) == socket_device_ &&
      static_cast<std::uint64_t>(info.st_ino) == socket_inode_) {
    unlink(socket_path_.c_str());
  }
  close(socket_);
}

bool InstanceActivationReceiver::Start(std::string& error) {
  error.clear();
  if (socket_ >= 0) return true;
  const auto path = ActivationPath(application_, error);
  if (path.empty() || !PrivateDirectory(true, error)) return false;
  // The caller holds the client lease. Any socket left at this path is from a
  // crashed GUI, and may be removed without disrupting another live instance.
  struct stat info{};
  if (lstat(path.c_str(), &info) == 0) {
    if (!S_ISSOCK(info.st_mode) || info.st_uid != geteuid()) {
      error = "Activation endpoint is not a socket owned by this user";
      return false;
    }
    if (unlink(path.c_str()) != 0) {
      error = "Cannot remove stale activation socket: " + std::string(strerror(errno));
      return false;
    }
  } else if (errno != ENOENT) {
    error = "Cannot inspect activation socket: " + std::string(strerror(errno));
    return false;
  }
  const int fd = CreateSocket(error);
  if (fd < 0) return false;
  const auto address = Address(path);
  if (bind(fd, reinterpret_cast<const sockaddr*>(&address), AddressSize(path)) != 0) {
    error = "Cannot bind activation socket: " + std::string(strerror(errno));
    close(fd);
    return false;
  }
  if (lstat(path.c_str(), &info) != 0) {
    error = "Cannot inspect bound activation socket: " + std::string(strerror(errno));
    unlink(path.c_str());
    close(fd);
    return false;
  }
  socket_ = fd;
  socket_path_ = path;
  socket_device_ = info.st_dev;
  socket_inode_ = info.st_ino;
  return true;
}

bool InstanceActivationReceiver::TakeRequest(std::string* activation_token) {
  if (activation_token) activation_token->clear();
  if (socket_ < 0) return false;
  bool requested = false;
  // Bound each UI tick's work even if many duplicate launches arrive together.
  for (int i = 0; i < 32; ++i) {
    char buffer[kRequest.size() + 1 + kMaxActivationToken + 1];
    const auto size = recv(socket_, buffer, sizeof(buffer), 0);
    if (size < 0) {
      if (errno == EINTR) continue;
      break;
    }
    const std::string_view message(buffer, size);
    if (message == kRequest) {
      requested = true;
    } else if (message.size() > kRequest.size() &&
               message.size() <= kRequest.size() + 1 + kMaxActivationToken &&
               message.substr(0, kRequest.size()) == kRequest &&
               message[kRequest.size()] == '\0') {
      const auto token = message.substr(kRequest.size() + 1);
      if (token.find('\0') == std::string_view::npos) {
        requested = true;
        if (activation_token) *activation_token = token;
      }
    }
  }
  return requested;
}

bool RequestInstanceActivation(std::string& error, const std::string& application) {
  error.clear();
  const auto path = ActivationPath(application, error);
  if (path.empty()) return false;
  const int fd = CreateSocket(error);
  if (fd < 0) return false;
  const auto address = Address(path);
  std::string request(kRequest);
#if defined(__linux__)
  const auto token = Environment("XDG_ACTIVATION_TOKEN");
  if (!token.empty() && token.size() <= kMaxActivationToken) {
    request += '\0';
    request += token;
  }
#endif
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  int last_error = ENOENT;
  do {
    if (PrivateDirectory(false, error)) {
      if (sendto(fd, request.data(), request.size(), 0,
                 reinterpret_cast<const sockaddr*>(&address), AddressSize(path)) ==
          static_cast<ssize_t>(request.size())) {
        close(fd);
        return true;
      }
      last_error = errno;
      if (last_error != ENOENT && last_error != ECONNREFUSED &&
          last_error != EAGAIN && last_error != EINTR) break;
    } else if (!error.empty()) {
      close(fd);
      return false;
    }
    if (std::chrono::steady_clock::now() >= deadline) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  } while (true);
  close(fd);
  error = "Cannot notify the existing CrossDesk window in this graphical session: " +
          std::string(strerror(last_error));
  return false;
}

}  // namespace crossdesk::platform
