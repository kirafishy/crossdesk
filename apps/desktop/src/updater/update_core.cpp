#include "updater/update_core.h"

#include <app_version.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

#ifndef CROSSDESK_UPDATE_PUBLIC_KEY
#define CROSSDESK_UPDATE_PUBLIC_KEY ""
#endif
#ifndef CROSSDESK_VERSION
#define CROSSDESK_VERSION "0.0.0"
#endif

namespace crossdesk::updater {
namespace {
constexpr std::uint64_t kMaxPackageSize = 2ULL * 1024 * 1024 * 1024;
constexpr const char* kDownloadOrigin = "https://downloads.crossdesk.cn";

std::string Hex(const unsigned char* data, std::size_t length) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (std::size_t i = 0; i < length; ++i) {
    result += digits[data[i] >> 4];
    result += digits[data[i] & 15];
  }
  return result;
}

std::vector<unsigned char> Unhex(const std::string& value, std::size_t size) {
  if (value.size() != size * 2) throw std::runtime_error("Invalid signature/key size");
  std::vector<unsigned char> result(size);
  auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    throw std::runtime_error("Expected lowercase hexadecimal");
  };
  for (std::size_t i = 0; i < size; ++i)
    result[i] = static_cast<unsigned char>(digit(value[2 * i]) * 16 + digit(value[2 * i + 1]));
  return result;
}

std::string ExpectedFilename(const std::string& platform, const std::string& version) {
  if (version.empty() || version.find_first_not_of("0123456789.-") != std::string::npos)
    throw std::runtime_error("Invalid release filename version");
  if (platform == "windows-x64") return "crossdesk-win-x64-v" + version + ".exe";
  if (platform == "windows-x64-portable")
    return "crossdesk-win-x64-portable-v" + version + ".zip";
  if (platform == "macos-x64" || platform == "macos-arm64")
    return "crossdesk-" + platform + "-v" + version + ".pkg";
  for (const std::string arch : {"amd64", "arm64"}) {
    if (platform == "linux-" + arch)
      return "crossdesk-linux-" + arch + "-v" + version + ".deb";
    if (platform == "linux-" + arch + "-virtual-desktop-xfce")
      return "crossdesk-virtual-desktop-xfce-linux-" + arch + "-v" + version + ".deb";
  }
  throw std::runtime_error("Unsupported update platform");
}
}  // namespace

std::string PublicKey() { return CROSSDESK_UPDATE_PUBLIC_KEY; }
std::string BuildVersion() { return CROSSDESK_VERSION; }
std::string PathUtf8(const fs::path& path) { return path.u8string(); }

Artifact VerifyEnvelope(const std::string& envelope, const std::string& public_key,
                        const std::string& platform, const std::string& current_version) {
  if (envelope.size() > 65536) throw std::runtime_error("Update envelope is too large");
  const auto wrapper = nlohmann::json::parse(envelope);
  const auto payload = wrapper.at("payload").get<std::string>();
  const auto signature = Unhex(wrapper.at("signature").get<std::string>(), 64);
  const auto key_bytes = Unhex(public_key, 32);
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, key_bytes.data(), key_bytes.size()),
      EVP_PKEY_free);
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!key || !context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1 ||
      EVP_DigestVerify(context.get(), signature.data(), signature.size(),
          reinterpret_cast<const unsigned char*>(payload.data()), payload.size()) != 1)
    throw std::runtime_error("Update signature verification failed");
  const auto value = nlohmann::json::parse(payload);
  if (value.at("schema") != 1) throw std::runtime_error("Unsupported update schema");
  Artifact artifact;
  artifact.platform = value.at("platform").get<std::string>();
  artifact.version = value.at("version").get<std::string>();
  artifact.filename = value.at("filename").get<std::string>();
  artifact.url = value.at("url").get<std::string>();
  artifact.sha256 = value.at("sha256").get<std::string>();
  if (!value.at("size").is_number_unsigned()) throw std::runtime_error("Invalid package size");
  artifact.size = value.at("size").get<std::uint64_t>();
  if (artifact.platform != platform || platform.find("portable") != std::string::npos ||
      !IsValidAppVersion(current_version) || !IsValidAppVersion(artifact.version) ||
      !IsNewerVersion(current_version, artifact.version))
    throw std::runtime_error("Update platform/version does not match this installation");
  if (!artifact.size || artifact.size > kMaxPackageSize ||
      artifact.filename != ExpectedFilename(platform, artifact.version) ||
      artifact.url != std::string(kDownloadOrigin) + "/" + artifact.filename)
    throw std::runtime_error("Invalid update package metadata");
  Unhex(artifact.sha256, 32);
  return artifact;
}

std::string PlatformKey(const fs::path& executable, bool portable) {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
  (void)executable;
  return portable ? "windows-x64-portable" : "windows-x64";
#elif defined(__APPLE__)
  (void)executable; (void)portable;
#if defined(__aarch64__) || defined(__arm64__)
  return "macos-arm64";
#else
  return "macos-x64";
#endif
#elif defined(__linux__) && !defined(__OHOS__)
  (void)portable;
#if defined(__aarch64__)
  std::string key = "linux-arm64";
#elif defined(__x86_64__)
  std::string key = "linux-amd64";
#else
  return {};
#endif
  std::ifstream marker(executable.parent_path() / "package-name");
  std::string package;
  if (!std::getline(marker, package)) return {};
  if (package == "crossdesk-virtual-desktop-xfce") key += "-virtual-desktop-xfce";
  else if (package != "crossdesk") return {};
  return key;
#else
  (void)executable; (void)portable;
  return {};
#endif
}

std::string DownloadUrl(const std::string& manifest, const std::string& platform,
                        const std::string& version) {
  try {
    const auto download = nlohmann::json::parse(manifest).at("downloads").at(platform);
    const auto filename = ExpectedFilename(platform, version);
    const auto url = std::string(kDownloadOrigin) + "/" + filename;
    if (download.at("version") == version && download.at("filename") == filename &&
        download.at("url") == url) return url;
  } catch (const std::exception&) {}
  return "https://crossdesk.cn";
}

std::string SignedDownload(const std::string& manifest, const std::string& platform) {
  try {
    return nlohmann::json::parse(manifest).at("downloads").at(platform).at("update").dump();
  } catch (const std::exception&) { return {}; }
}

std::string Sha256(const std::string& value) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  if (EVP_Digest(value.data(), value.size(), digest, &size, EVP_sha256(), nullptr) != 1)
    throw std::runtime_error("SHA-256 failed");
  return Hex(digest, size);
}

void VerifyPackage(const fs::path& file, const Artifact& artifact) {
  std::ifstream input(file, std::ios::binary);
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!input || !context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    throw std::runtime_error("Cannot read update package");
  std::array<char, 65536> buffer{};
  std::uint64_t total = 0;
  while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
    const auto size = static_cast<std::size_t>(input.gcount());
    total += size;
    if (total > artifact.size || EVP_DigestUpdate(context.get(), buffer.data(), size) != 1)
      throw std::runtime_error("Update package size/hash failed");
  }
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  if (!input.eof() || total != artifact.size ||
      EVP_DigestFinal_ex(context.get(), digest, &size) != 1 || Hex(digest, size) != artifact.sha256)
    throw std::runtime_error("Update package verification failed");
}

void DownloadPackage(const Artifact& artifact, const fs::path& destination,
                     std::atomic<int>& progress, const std::atomic<bool>& cancel) {
  httplib::Client client(kDownloadOrigin);
  client.set_connection_timeout(10);
  client.set_read_timeout(30);
  client.set_write_timeout(10);
  client.set_max_timeout(30 * 60 * 1000);
  client.set_follow_location(false);
#if defined(__linux__)
  for (const char* bundle : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt"}) {
    if (fs::exists(bundle)) { client.set_ca_cert_path(bundle); break; }
  }
#endif
  std::ofstream file(destination, std::ios::binary | std::ios::trunc);
  if (!file) throw std::runtime_error("Cannot create update download");
  std::uint64_t received = 0;
  const auto response = client.Get(("/" + artifact.filename).c_str(),
      [&](const httplib::Response& headers) {
        return headers.status == 200 && !cancel.load();
      },
      [&](const char* data, std::size_t length) {
        if (cancel.load() || length > artifact.size - received) return false;
        file.write(data, static_cast<std::streamsize>(length));
        received += length;
        progress.store(static_cast<int>(received * 100 / artifact.size));
        return static_cast<bool>(file);
      });
  file.close();
  if (!response || response->status != 200 || !file || received != artifact.size || cancel.load())
    throw std::runtime_error(cancel.load() ? "Update cancelled" : "Update download failed");
  VerifyPackage(destination, artifact);
}

void CopyPackage(const fs::path& source, const fs::path& destination, const Artifact& artifact) {
  std::ifstream input(source, std::ios::binary);
  std::ofstream output(destination, std::ios::binary | std::ios::trunc);
  if (!input || !output) throw std::runtime_error("Cannot stage installation package");
  std::array<char, 65536> buffer{};
  std::uint64_t copied = 0;
  while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
    const auto length = static_cast<std::uint64_t>(input.gcount());
    if (length > artifact.size - copied) throw std::runtime_error("Installation package is too large");
    output.write(buffer.data(), input.gcount());
    if (!output) throw std::runtime_error("Cannot write installation package");
    copied += length;
  }
  output.close();
  if (!input.eof() || !output || copied != artifact.size)
    throw std::runtime_error("Installation package is incomplete");
  VerifyPackage(destination, artifact);
}

std::string ReadText(const fs::path& file, std::size_t maximum) {
  std::ifstream input(file, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot read update metadata");
  std::string value(maximum + 1, '\0');
  input.read(value.data(), static_cast<std::streamsize>(value.size()));
  value.resize(static_cast<std::size_t>(input.gcount()));
  if (value.size() > maximum || input.bad()) throw std::runtime_error("Invalid update metadata size");
  return value;
}

void WriteText(const fs::path& file, const std::string& text) {
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  output << text;
  output.close();
  if (!output) throw std::runtime_error("Cannot write update state");
}

std::string RandomId() {
  unsigned char bytes[16];
  if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("Cannot generate update ID");
  return Hex(bytes, sizeof(bytes));
}

void ExecuteInstall(const InstallActions& actions) {
  actions.verify();
  actions.wait_for_exit();
  actions.install();
}
}  // namespace crossdesk::updater
