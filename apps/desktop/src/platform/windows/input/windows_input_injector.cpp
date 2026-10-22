#include "windows_input_injector.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include "interactive_desktop.h"
#include "rd_log.h"

namespace crossdesk {
namespace {

struct InputResult {
  UINT sent;
  DWORD error;
  bool requires_worker = false;
};

InputResult InjectOnUserDesktop(INPUT* inputs, UINT count) {
  if (!IsCurrentSessionUnlocked()) {
    return {0, ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION};
  }
  ScopedInteractiveDesktop desktop;
  if (!desktop.Bind(L"")) {
    const DWORD error = GetLastError();
    // A library may have created a window/hook on an opted-in thread. This
    // failure occurs before SendInput, so retrying on the worker is safe.
    return {0, error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE,
            error == ERROR_BUSY};
  }
  if (!desktop.IsReceivingInput()) {
    return {0, ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION};
  }
  // Lock screens and UAC still require the session-checked service path.
  if (_wcsicmp(desktop.name().c_str(), L"Default") != 0) {
    return {0, ERROR_ACCESS_DENIED};
  }
  SetLastError(ERROR_SUCCESS);
  const UINT sent = SendInput(count, inputs, sizeof(INPUT));
  if (sent == count) return {sent, ERROR_SUCCESS};
  const DWORD error = GetLastError();
  return {sent, error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE};
}

class UserDesktopInputWorker {
 public:
  UserDesktopInputWorker() : thread_([this] { Run(); }) {}

  ~UserDesktopInputWorker() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    wake_.notify_one();
    thread_.join();
  }

  InputResult Send(INPUT* inputs, UINT count) {
    std::packaged_task<InputResult()> task(
        [inputs, count] { return InjectOnUserDesktop(inputs, count); });
    auto result = task.get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_.push(std::move(task));
    }
    wake_.notify_one();
    return result.get();
  }

 private:
  void Run() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
      wake_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
      if (pending_.empty()) return;
      auto task = std::move(pending_.front());
      pending_.pop();
      lock.unlock();
      task();
      lock.lock();
    }
  }

  std::mutex mutex_;
  std::condition_variable wake_;
  std::queue<std::packaged_task<InputResult()>> pending_;
  bool stopping_ = false;
  std::thread thread_;
};

}  // namespace

UINT SendInputOnUserDesktop(const INPUT& input) {
  INPUT copy = input;
  return SendInputOnUserDesktop(&copy, 1);
}

UINT SendInputOnUserDesktop(INPUT* inputs, UINT count) {
  if (!inputs || count == 0) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  InputResult result{0, ERROR_SUCCESS, true};
  if (ScopedUserDesktopInputThread::IsCurrent())
    result = InjectOnUserDesktop(inputs, count);
  if (result.requires_worker) {
    static UserDesktopInputWorker worker;
    result = worker.Send(inputs, count);
  }
  const DWORD error = result.error;
  if (error != ERROR_SUCCESS) {
    // Include session identity even when Windows reports no useful SendInput
    // error (for example UIPI). Keep repeated mouse-move failures bounded.
    static std::mutex log_mutex;
    static ULONGLONG last_log_tick = 0;
    std::lock_guard<std::mutex> lock(log_mutex);
    const ULONGLONG now = GetTickCount64();
    if (last_log_tick == 0 || now - last_log_tick >= 2000) {
      DWORD session_id = 0xFFFFFFFF;
      ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
      LOG_WARN("User desktop input unavailable: session_id={}, error={}",
               session_id, error);
      last_log_tick = now;
    }
  }
  SetLastError(error);
  return result.sent;
}

}  // namespace crossdesk
