/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-24
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _LAN_PASSWORD_H_
#define _LAN_PASSWORD_H_

#include <optional>
#include <string>

namespace crossdesk::lan {
// Keep the password record format aligned with crossdesk-server.
// Stored in identity.json. The hash identifies the algorithm,
// version and work factors; unrecognized formats never fall back to SHA-256.
struct DevicePasswordRecord {
  std::string salt;
  std::string hash;
};

namespace DevicePassword {
void CheckSupport();
std::optional<DevicePasswordRecord> Hash(const std::string& password);
// LAN accepts only current Argon2id records. Older LAN records are migrated
// locally from the encrypted password before connections are accepted.
bool Verify(const DevicePasswordRecord& record, const std::string& password);
bool IsCurrent(const DevicePasswordRecord& record);
}  // namespace DevicePassword

}  // namespace crossdesk::lan

#endif
