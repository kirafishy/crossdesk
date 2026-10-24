/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-24
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _LAN_IDENTITY_H_
#define _LAN_IDENTITY_H_

#include <openssl/x509.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace crossdesk::lan {
struct Interface {
  std::string address;
};
struct Target {
  std::string address;
  // Learned from the current connection, never pinned.
  std::string device_id;
  unsigned port = 52888;
};
unsigned ParsePort(const std::string& value);
std::pair<std::string, unsigned> ParseEndpoint(const std::string& value,
                                               unsigned default_port);
bool IsIpv4(const std::string& value);
std::vector<Interface> PhysicalInterfaces();
std::string RandomHex(size_t bytes);
bool ValidPassword(const std::string& password);
bool VerifyLanCertificate(X509* cert);

// Files are separate from cloud settings and thumbnail credentials. A corrupt
// store is an error; it must never be replaced with a newly generated identity.
class Identity {
 public:
  explicit Identity(std::filesystem::path directory);
  void Load();
  static std::pair<std::string, nlohmann::json> GeneratePassword();
  void SetPassword(const std::string& password, const nlohmann::json& verifier);
  std::string ReadPassword() const;
  static nlohmann::json RegisterPassword(const std::string& password);
  static bool CurrentVerifier(const nlohmann::json& verifier);
  static bool VerifyPassword(const nlohmann::json& verifier,
                             const std::string& password);
  void SaveSettings(const std::string& address, unsigned port);
  // An unconfigured address uses the default port; offline addresses keep
  // their saved port. Selection only controls the address shown in the UI.
  unsigned PortFor(const std::string& address) const;
  bool HasPortFor(const std::string& address) const;
  void SelectAddress(const std::string& address);
  const std::string& id() const { return id_; }
  std::filesystem::path certificate() const {
    return directory_ / "certificate.pem";
  }
  std::filesystem::path private_key() const {
    return directory_ / "private-key.pem";
  }
  const nlohmann::json& verifier() const { return state_.at("verifier"); }
  const nlohmann::json& settings() const { return state_.at("settings"); }

 private:
  void Save(const nlohmann::json& state);
  std::filesystem::path directory_;
  std::string id_;
  nlohmann::json state_;
};
}  // namespace crossdesk::lan

#endif
