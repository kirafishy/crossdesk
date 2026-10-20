#include "updater/update_controller.h"
#include "updater/update_platform.h"
#include "common/filesystem_utf8.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace crossdesk::updater {
Controller::Controller(fs::path cache) : cache_(std::move(cache)) {
  // A previous result survives a restart. Incomplete jobs are never replayed.
  try {
    const auto previous = PathFromUtf8(ReadText(cache_ / "last-update", 32768));
    if (!fs::exists(previous / "result.json")) {
      state_ = State::Failed;
      error_ = "The previous update was interrupted. Download and retry.\n" + PathUtf8(previous);
      return;
    }
    const auto result = nlohmann::json::parse(ReadText(previous / "result.json"));
    if (!result.value("success", false)) {
      state_ = State::Failed;
      error_ = result.value("error", std::string("Update did not complete")) +
               "\n" + PathUtf8(previous);
    }
  } catch (const std::exception&) {}
}
void Controller::ConfirmStartup() {
  try {
    const auto previous = PathFromUtf8(ReadText(cache_ / "last-update", 32768));
    const auto result = nlohmann::json::parse(ReadText(previous / "result.json"));
    if (!result.value("success", false)) return;
    const auto request = nlohmann::json::parse(ReadText(previous / "request.json"));
    const auto artifact = VerifyEnvelope(request.at("envelope").get<std::string>(), PublicKey(),
                                         PlatformKey(ExecutablePath()), "0.0.0");
    auto built = BuildVersion();
    if (!built.empty() && built[0] == 'v') built.erase(0, 1);
    if (artifact.version == built) WriteText(previous / "started", "started\n");
  } catch (const std::exception&) {}
}
Controller::~Controller() {
  Cancel();
  if (pending_.valid()) {
    try {
      const auto prepared = pending_.get();
      WriteText(prepared / "cancel", "cancel\n");
    } catch (const std::exception&) {}
  }
}
bool Controller::Available(const std::string& envelope) const {
  if (checked_ && checked_envelope_ == envelope) return available_;
  checked_ = true;
  checked_envelope_ = envelope;
  available_ = false;
  try {
    const auto application = ExecutablePath();
    if (!SupportedInstallation(application)) return false;
    VerifyEnvelope(envelope, PublicKey(), PlatformKey(application), BuildVersion());
    available_ = true;
    return available_;
  } catch (const std::exception&) { return false; }
}
void Controller::Start(const std::string& envelope) {
  if (busy() || state_ == State::Ready) return;
  error_.clear();
  progress_ = 0;
  cancel_ = false;
  state_ = State::Downloading;
  try {
    pending_ = std::async(std::launch::async, [this, envelope] {
      const auto application = ExecutablePath();
      if (!SupportedInstallation(application)) throw std::runtime_error("Unsupported installation");
      const auto artifact = VerifyEnvelope(envelope, PublicKey(), PlatformKey(application), BuildVersion());
      const auto job = PrivateDirectory();
      try {
        WriteText(job / "request.json", nlohmann::json{
            {"application", PathUtf8(application)}, {"envelope", envelope}}.dump());
        DownloadPackage(artifact, job / artifact.filename, progress_, cancel_);
        if (cancel_) throw std::runtime_error("Update cancelled");
        // Relocate the coordinator so package installation can replace the
        // installed helper too. Its crypto/runtime dependencies are static.
        const auto helper = job / HelperPath(application).filename();
        fs::copy_file(HelperPath(application), helper);
        fs::permissions(helper, fs::perms::owner_all);
        Launch(helper, {"--coordinate", PathUtf8(job)});
        return job;
      } catch (...) {
        // Only this freshly created directory is owned by this worker.
        std::error_code ignored;
        fs::remove_all(job, ignored);
        throw;
      }
    });
  } catch (const std::exception& exception) {
    error_ = exception.what();
    state_ = State::Failed;
  }
}
bool Controller::Poll() {
  try {
    if (state_ == State::Downloading && pending_.valid() &&
        pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      job_ = pending_.get();
      deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      state_ = State::Preparing;
    }
    if (state_ != State::Preparing) return false;
    if (cancel_) throw std::runtime_error("Update cancelled");
    if (fs::exists(job_ / "result.json")) {
      const auto result = nlohmann::json::parse(ReadText(job_ / "result.json"));
      throw std::runtime_error(result.value("error", std::string("Update preparation failed")));
    }
    if (fs::exists(job_ / "ready")) {
      WriteText(cache_ / "last-update", PathUtf8(job_));
      WriteText(job_ / "commit", "install\n");
      state_ = State::Ready;
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline_)
      throw std::runtime_error("Update helper did not become ready");
  } catch (const std::exception& exception) {
    const bool cancelled = cancel_.load();
    if (state_ == State::Preparing) Cancel();
    error_ = exception.what();
    state_ = cancelled ? State::Idle : State::Failed;
    if (cancelled) error_.clear();
  }
  return false;
}
void Controller::Cancel() {
  cancel_ = true;
  if (state_ == State::Preparing && !job_.empty()) {
    try { WriteText(job_ / "cancel", "cancel\n"); } catch (const std::exception&) {}
  }
}
}  // namespace crossdesk::updater
