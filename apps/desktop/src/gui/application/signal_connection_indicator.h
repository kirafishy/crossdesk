#ifndef CROSSDESK_SIGNAL_CONNECTION_INDICATOR_H_
#define CROSSDESK_SIGNAL_CONNECTION_INDICATOR_H_

#include <chrono>
#include <optional>

namespace crossdesk {

// Presentation state only. The actual connected flag remains authoritative
// for actions and other UI consumers, including the announcement inbox.
class SignalConnectionIndicator {
 public:
  using Clock = std::chrono::steady_clock;

  bool Update(bool connected, Clock::time_point now = Clock::now()) {
    if (connected) {
      disconnected_at_.reset();
    } else if (was_connected_) {
      disconnected_at_ = now;
    }
    was_connected_ = connected;
    return disconnected_at_ &&
           now - *disconnected_at_ < std::chrono::seconds(10);
  }

  void Reset() {
    was_connected_ = false;
    disconnected_at_.reset();
  }

 private:
  bool was_connected_ = false;
  std::optional<Clock::time_point> disconnected_at_;
};

}  // namespace crossdesk

#endif
