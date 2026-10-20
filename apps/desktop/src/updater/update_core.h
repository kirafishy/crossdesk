/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-20
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _UPDATE_CORE_H_
#define _UPDATE_CORE_H_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace crossdesk::updater {
namespace fs = std::filesystem;

struct Artifact {
  std::string platform;
  std::string version;
  std::string filename;
  std::string url;
  std::string sha256;
  std::uint64_t size = 0;
};

// The envelope contains the exact signed UTF-8 payload and a hex Ed25519 signature.
// Never reserialize the payload before verifying it.
Artifact VerifyEnvelope(const std::string& envelope, const std::string& public_key,
                        const std::string& platform,
                        const std::string& current_version);
std::string PublicKey();
std::string BuildVersion();
std::string PlatformKey(const fs::path& executable, bool portable = false);
std::string DownloadUrl(const std::string& manifest, const std::string& platform,
                        const std::string& version);
std::string SignedDownload(const std::string& manifest, const std::string& platform);
std::string Sha256(const std::string& value);
void VerifyPackage(const fs::path& file, const Artifact& artifact);
void CopyPackage(const fs::path& source, const fs::path& destination, const Artifact& artifact);
void DownloadPackage(const Artifact& artifact, const fs::path& destination,
                     std::atomic<int>& progress, const std::atomic<bool>& cancel);
std::string ReadText(const fs::path& file, std::size_t maximum = 65536);
void WriteText(const fs::path& file, const std::string& text);
fs::path PrivateDirectory();
std::string RandomId();
// ABI boundary between the C++17 core and C++20 GUI (char8_t).
std::string PathUtf8(const fs::path& path);

// Files and installation operations are injectable so failure ordering is tested
// without invoking an actual installer.
struct InstallActions {
  std::function<void()> verify;
  std::function<void()> wait_for_exit;
  std::function<void()> install;
};
void ExecuteInstall(const InstallActions& actions);
}  // namespace crossdesk::updater

#endif
