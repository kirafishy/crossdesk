#include "wayland_activation.h"

#include <dlfcn.h>

#include <cstdint>
#include <cstring>

namespace crossdesk::platform {
namespace {

// The public libwayland-client wire ABI. Load it at runtime so X11-only builds
// do not acquire a new Wayland build or runtime dependency.
struct Interface;
struct Message {
  const char* name;
  const char* signature;
  const Interface** types;
};
struct Interface {
  const char* name;
  int version;
  int method_count;
  const Message* methods;
  int event_count;
  const Message* events;
};

struct WaylandApi {
  void* library = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
  void* (*create_queue)(void*) = nullptr;
  void (*destroy_queue)(void*) = nullptr;
  void* (*create_wrapper)(void*) = nullptr;
  void (*destroy_wrapper)(void*) = nullptr;
  void (*set_queue)(void*, void*) = nullptr;
  void (*destroy_proxy)(void*) = nullptr;
  void* (*marshal)(void*, uint32_t, const Interface*, uint32_t, uint32_t, ...) = nullptr;
  int (*add_listener)(void*, void (**)(void), void*) = nullptr;
  int (*roundtrip)(void*, void*) = nullptr;
  int (*flush)(void*) = nullptr;
  const Interface* registry_interface = nullptr;
  const Interface* surface_interface = nullptr;
  bool available = false;

  template <class T>
  bool Load(T& target, const char* name) {
    target = reinterpret_cast<T>(dlsym(library, name));
    return target != nullptr;
  }

  WaylandApi() {
    available = library &&
        Load(create_queue, "wl_display_create_queue") &&
        Load(destroy_queue, "wl_event_queue_destroy") &&
        Load(create_wrapper, "wl_proxy_create_wrapper") &&
        Load(destroy_wrapper, "wl_proxy_wrapper_destroy") &&
        Load(set_queue, "wl_proxy_set_queue") &&
        Load(destroy_proxy, "wl_proxy_destroy") &&
        Load(marshal, "wl_proxy_marshal_flags") &&
        Load(add_listener, "wl_proxy_add_listener") &&
        Load(roundtrip, "wl_display_roundtrip_queue") &&
        Load(flush, "wl_display_flush") &&
        Load(registry_interface, "wl_registry_interface") &&
        Load(surface_interface, "wl_surface_interface");
  }
  ~WaylandApi() { if (library) dlclose(library); }
};

struct Activation {
  WaylandApi& api;
  void* queue = nullptr;
  void* wrapper = nullptr;
  void* registry = nullptr;
  void* activation = nullptr;
  void* token = nullptr;
  std::string token_value;

  const Interface* surface_types[1];
  const Interface* activate_types[2];
  const Message token_methods[5];
  const Message token_events[1] = {{"done", "s", nullptr}};
  const Interface token_interface;
  const Interface* new_token_types[1];
  const Message activation_methods[3];
  const Interface activation_interface;

  explicit Activation(WaylandApi& value)
      : api(value), surface_types{api.surface_interface},
        activate_types{nullptr, api.surface_interface},
        token_methods{{"set_serial", "uo", nullptr}, {"set_app_id", "s", nullptr},
                      {"set_surface", "o", surface_types}, {"commit", "", nullptr},
                      {"destroy", "", nullptr}},
        token_interface{"xdg_activation_token_v1", 1, 5, token_methods, 1, token_events},
        new_token_types{&token_interface},
        activation_methods{{"destroy", "", nullptr},
                           {"get_activation_token", "n", new_token_types},
                           {"activate", "so", activate_types}},
        activation_interface{"xdg_activation_v1", 1, 3, activation_methods, 0, nullptr} {}

  ~Activation() {
    // Destroy protocol objects as well as client-side proxies on every path.
    constexpr uint32_t destroy = 1;  // WL_MARSHAL_FLAG_DESTROY
    if (token) api.marshal(token, 4, nullptr, 1, destroy);
    if (activation) api.marshal(activation, 0, nullptr, 1, destroy);
    if (registry) api.destroy_proxy(registry);
    if (wrapper) api.destroy_wrapper(wrapper);
    if (queue) api.destroy_queue(queue);
  }

  static void Global(void* data, void* registry, uint32_t name,
                     const char* interface, uint32_t version) {
    auto& self = *static_cast<Activation*>(data);
    if (self.activation || version < 1 || std::strcmp(interface, "xdg_activation_v1")) return;
    self.activation = self.api.marshal(registry, 0, &self.activation_interface, 1, 0,
        name, self.activation_interface.name, uint32_t{1}, nullptr);
  }
  static void GlobalRemove(void*, void*, uint32_t) {}
  static void Done(void* data, void*, const char* value) {
    static_cast<Activation*>(data)->token_value = value;
  }
};

}  // namespace

bool ActivateWaylandWindow(void* display, void* surface,
                           const std::string& activation_token) {
  if (!display || !surface) return false;
  static WaylandApi api;
  if (!api.available) return false;
  Activation request(api);
  // Use a separate queue and a display wrapper: roundtrips must never dispatch
  // Slint/winit's own events from inside its timer callback.
  request.queue = api.create_queue(display);
  request.wrapper = api.create_wrapper(display);
  if (!request.queue || !request.wrapper) return false;
  api.set_queue(request.wrapper, request.queue);
  request.registry = api.marshal(request.wrapper, 1, api.registry_interface, 1, 0, nullptr);
  if (!request.registry) return false;
  void (*registry_listener[])(void) = {
      reinterpret_cast<void (*)(void)>(&Activation::Global),
      reinterpret_cast<void (*)(void)>(&Activation::GlobalRemove)};
  if (api.add_listener(request.registry, registry_listener, &request) != 0 ||
      api.roundtrip(display, request.queue) < 0 || !request.activation) return false;

  request.token_value = activation_token;
  if (request.token_value.empty()) {
    // Terminal/tray launches may carry no startup token. Ask the compositor
    // for one; it remains free to refuse focus without a recent input serial.
    request.token = api.marshal(request.activation, 1, &request.token_interface, 1, 0, nullptr);
    if (!request.token) return false;
    void (*token_listener[])(void) = {reinterpret_cast<void (*)(void)>(&Activation::Done)};
    if (api.add_listener(request.token, token_listener, &request) != 0) return false;
    api.marshal(request.token, 1, nullptr, 1, 0, "crossdesk");
    api.marshal(request.token, 2, nullptr, 1, 0, surface);
    api.marshal(request.token, 3, nullptr, 1, 0);
    if (api.roundtrip(display, request.queue) < 0 || request.token_value.empty()) return false;
  }
  api.marshal(request.activation, 2, nullptr, 1, 0, request.token_value.c_str(), surface);
  api.flush(display);
  return true;
}

}  // namespace crossdesk::platform
