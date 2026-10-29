#include "linux_pointer_presence.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <dlfcn.h>
#include <poll.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace crossdesk {
namespace {

enum class PointerPresence { unknown, present, absent };

PointerPresence QueryX11PointerPresence(Display* display) {
  if (!display) return PointerPresence::unknown;
  int opcode = 0, event = 0, error = 0;
  int major = 2, minor = 0;
  if (!XQueryExtension(display, "XInputExtension", &opcode, &event, &error) ||
      XIQueryVersion(display, &major, &minor) != Success || major < 2) {
    return PointerPresence::unknown;
  }
  int count = 0;
  XIDeviceInfo* devices = XIQueryDevice(display, XIAllDevices, &count);
  if (!devices) return PointerPresence::unknown;
  auto result = count > 0 ? PointerPresence::absent : PointerPresence::unknown;
  for (int i = 0; i < count; ++i) {
    const auto& device = devices[i];
    if (!device.enabled || device.use != XISlavePointer) continue;
    // Xorg creates an XTEST slave for every master even without hardware.
    // Match its generated name to its master, not a broad "virtual" filter:
    // mice exposed by xrdp and other virtual desktops still count as pointers.
    bool xtest = false;
    for (int j = 0; j < count && device.name; ++j) {
      const auto& master = devices[j];
      if (master.deviceid != device.attachment ||
          master.use != XIMasterPointer || !master.name)
        continue;
      const std::string name = master.name;
      constexpr char suffix[] = " pointer";
      if (name.size() >= sizeof(suffix) - 1 &&
          name.compare(name.size() - (sizeof(suffix) - 1), sizeof(suffix) - 1,
                       suffix) == 0) {
        xtest = name.substr(0, name.size() - (sizeof(suffix) - 1)) +
                    " XTEST pointer" ==
                device.name;
      }
      break;
    }
    if (!xtest) {
      result = PointerPresence::present;
      break;
    }
  }
  XIFreeDeviceInfo(devices);
  return result;
}

// Load the public client ABI, as the Wayland activation helper does. This
// keeps X11-only installations free of a new build/runtime dependency. The
// protocol interfaces themselves come from libwayland-client.
struct WaylandInterface;
struct WaylandApi {
  void* library = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
  void* (*connect)(const char*) = nullptr;
  void (*disconnect)(void*) = nullptr;
  int (*get_fd)(void*) = nullptr;
  int (*dispatch_pending)(void*) = nullptr;
  int (*prepare_read)(void*) = nullptr;
  void (*cancel_read)(void*) = nullptr;
  int (*read_events)(void*) = nullptr;
  int (*flush)(void*) = nullptr;
  void (*destroy_proxy)(void*) = nullptr;
  void* (*construct)(void*, uint32_t, const WaylandInterface*, uint32_t,
                     ...) = nullptr;
  int (*add_listener)(void*, void (**)(void), void*) = nullptr;
  const WaylandInterface* registry_interface = nullptr;
  const WaylandInterface* seat_interface = nullptr;
  const WaylandInterface* callback_interface = nullptr;
  bool available = false;

  template <class T>
  bool Load(T& target, const char* name) {
    target = reinterpret_cast<T>(dlsym(library, name));
    return target != nullptr;
  }

  WaylandApi() {
    available = library && Load(connect, "wl_display_connect") &&
                Load(disconnect, "wl_display_disconnect") &&
                Load(get_fd, "wl_display_get_fd") &&
                Load(dispatch_pending, "wl_display_dispatch_pending") &&
                Load(prepare_read, "wl_display_prepare_read") &&
                Load(cancel_read, "wl_display_cancel_read") &&
                Load(read_events, "wl_display_read_events") &&
                Load(flush, "wl_display_flush") &&
                Load(destroy_proxy, "wl_proxy_destroy") &&
                Load(construct, "wl_proxy_marshal_constructor_versioned") &&
                Load(add_listener, "wl_proxy_add_listener") &&
                Load(registry_interface, "wl_registry_interface") &&
                Load(seat_interface, "wl_seat_interface") &&
                Load(callback_interface, "wl_callback_interface");
  }
  ~WaylandApi() {
    if (library) dlclose(library);
  }
};

class WaylandPointerPresence {
 public:
  ~WaylandPointerPresence() { Reset(); }

  PointerPresence Query() {
    if (!api_.available) return PointerPresence::unknown;
    if ((!display_ && !Connect()) || !Pump() || failed_) {
      Reset();
      return PointerPresence::unknown;
    }
    if (!registry_ready_) return PointerPresence::unknown;
    bool pending = false;
    for (const auto& [id, seat] : seats_) {
      if (seat.presence == PointerPresence::present)
        return PointerPresence::present;
      pending = pending || seat.presence == PointerPresence::unknown;
    }
    return pending ? PointerPresence::unknown : PointerPresence::absent;
  }

 private:
  struct Seat {
    void* proxy = nullptr;
    PointerPresence presence = PointerPresence::unknown;
  };

  bool Connect() {
    // A dedicated connection avoids consuming Slint's events or depending on
    // which toolkit/display backend the local window uses.
    display_ = api_.connect(nullptr);
    if (!display_) return false;
    registry_ =
        api_.construct(display_, 1, api_.registry_interface, 1, nullptr);
    sync_ = api_.construct(display_, 0, api_.callback_interface, 1, nullptr);
    static void (*registry_listener[])(void) = {
        reinterpret_cast<void (*)(void)>(&Global),
        reinterpret_cast<void (*)(void)>(&GlobalRemove)};
    static void (*sync_listener[])(void) = {
        reinterpret_cast<void (*)(void)>(&Ready)};
    return registry_ && sync_ &&
           api_.add_listener(registry_, registry_listener, this) == 0 &&
           api_.add_listener(sync_, sync_listener, this) == 0;
  }

  bool Pump() {
    if (api_.dispatch_pending(display_) < 0) return false;
    if (api_.prepare_read(display_) != 0) return false;
    const int flushed = api_.flush(display_);
    if (flushed < 0 && errno != EAGAIN) {
      api_.cancel_read(display_);
      return false;
    }
    pollfd fd{api_.get_fd(display_), POLLIN, 0};
    if (fd.fd < 0) {
      api_.cancel_read(display_);
      return false;
    }
    const int ready = poll(&fd, 1, 0);
    if (ready < 0 || (fd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
      api_.cancel_read(display_);
      return false;
    }
    if (!(fd.revents & POLLIN)) {
      api_.cancel_read(display_);
      return true;
    }
    // Never perform a blocking roundtrip on the UI's cursor-sampling tick.
    return api_.read_events(display_) >= 0 &&
           api_.dispatch_pending(display_) >= 0;
  }

  void Reset() {
    for (auto& [id, seat] : seats_) {
      if (seat.proxy) api_.destroy_proxy(seat.proxy);
    }
    seats_.clear();
    if (sync_) api_.destroy_proxy(sync_);
    if (registry_) api_.destroy_proxy(registry_);
    if (display_) api_.disconnect(display_);
    display_ = registry_ = sync_ = nullptr;
    registry_ready_ = failed_ = false;
  }

  static void Global(void* data, void* registry, uint32_t id,
                     const char* interface, uint32_t version) {
    auto& self = *static_cast<WaylandPointerPresence*>(data);
    if (std::strcmp(interface, "wl_seat") != 0) return;
    if (version < 1 || self.seats_.count(id)) {
      self.failed_ = true;
      return;
    }
    auto& seat = self.seats_[id];
    seat.proxy = self.api_.construct(registry, 0, self.api_.seat_interface, 1,
                                     id, "wl_seat", uint32_t{1}, nullptr);
    // Version 1 only emits capabilities; seat names are not needed.
    static void (*listener[])(void) = {
        reinterpret_cast<void (*)(void)>(&Capabilities)};
    if (!seat.proxy ||
        self.api_.add_listener(seat.proxy, listener, &seat) != 0) {
      self.failed_ = true;
    }
  }

  static void GlobalRemove(void* data, void*, uint32_t id) {
    auto& self = *static_cast<WaylandPointerPresence*>(data);
    const auto it = self.seats_.find(id);
    if (it == self.seats_.end()) return;
    if (it->second.proxy) self.api_.destroy_proxy(it->second.proxy);
    self.seats_.erase(it);
  }

  static void Capabilities(void* data, void*, uint32_t capabilities) {
    // WL_SEAT_CAPABILITY_POINTER; keyboard/touch alone are not pointers.
    static_cast<Seat*>(data)->presence = (capabilities & 1u)
                                             ? PointerPresence::present
                                             : PointerPresence::absent;
  }

  static void Ready(void* data, void* callback, uint32_t) {
    auto& self = *static_cast<WaylandPointerPresence*>(data);
    self.registry_ready_ = true;
    self.api_.destroy_proxy(callback);
    self.sync_ = nullptr;
  }

  WaylandApi api_;
  void* display_ = nullptr;
  void* registry_ = nullptr;
  void* sync_ = nullptr;
  std::map<uint32_t, Seat> seats_;
  bool registry_ready_ = false;
  bool failed_ = false;
};

}  // namespace

struct LinuxPointerPresence::Impl {
  std::unique_ptr<WaylandPointerPresence> wayland;
  std::chrono::steady_clock::time_point next_check{};
  void* source_display = nullptr;
  bool source_wayland = false;
  bool absent_before = false;
  bool confirmed = false;
};

LinuxPointerPresence::LinuxPointerPresence()
    : impl_(std::make_unique<Impl>()) {}
LinuxPointerPresence::~LinuxPointerPresence() = default;

bool LinuxPointerPresence::NoPointingDevice(void* x11_display, bool wayland) {
  if (impl_->source_wayland != wayland ||
      impl_->source_display != x11_display) {
    impl_->next_check = {};
    impl_->absent_before = impl_->confirmed = false;
    impl_->source_display = x11_display;
    impl_->source_wayland = wayland;
  }
  const auto now = std::chrono::steady_clock::now();
  if (now < impl_->next_check) return impl_->confirmed;
  // Do not combine absence samples from unrelated, widely separated periods
  // of cursor hiding. Device changes and failed queries break the sequence.
  if (now > impl_->next_check + std::chrono::seconds(1))
    impl_->absent_before = false;
  impl_->next_check = now + std::chrono::seconds(1);
  PointerPresence presence;
  if (wayland) {
    if (!impl_->wayland)
      impl_->wayland = std::make_unique<WaylandPointerPresence>();
    presence = impl_->wayland->Query();
  } else {
    presence = QueryX11PointerPresence(static_cast<Display*>(x11_display));
  }
  const bool absent = presence == PointerPresence::absent;
  impl_->confirmed = absent && impl_->absent_before;
  impl_->absent_before = absent;
  return impl_->confirmed;
}

}  // namespace crossdesk
