#include "lan_identity.h"

#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "lan_password.h"
#ifdef _WIN32
// Winsock must precede windows.h; the networking/ACL headers need its types.
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <Aclapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <sddl.h>
// clang-format on
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#endif

namespace crossdesk::lan {
namespace {
void Require(bool ok, const char* error) {
  if (!ok) throw std::runtime_error(error);
}
std::string Hex(const unsigned char* data, size_t size) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result(size * 2, '0');
  for (size_t i = 0; i < size; ++i) {
    result[2 * i] = digits[data[i] >> 4];
    result[2 * i + 1] = digits[data[i] & 15];
  }
  return result;
}
bool IsHex(const std::string& text, size_t size) {
  return text.size() == size &&
         std::all_of(text.begin(), text.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
void PrivateWrite(const std::filesystem::path& path, const std::string& text) {
  const auto temporary = path.parent_path() / (".write-" + RandomHex(16));
#ifdef _WIN32
  HANDLE token = nullptr;
  Require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token),
          "Cannot open user token");
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::vector<unsigned char> data(size);
  bool ok = GetTokenInformation(token, TokenUser, data.data(), size, &size);
  CloseHandle(token);
  Require(ok, "Cannot read user token");
  LPWSTR sid = nullptr;
  Require(ConvertSidToStringSidW(
              reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid),
          "Cannot read user SID");
  std::wstring acl = L"D:P(A;;FA;;;" + std::wstring(sid) + L")";
  LocalFree(sid);
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  Require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
              acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr),
          "Cannot protect LAN file");
  SECURITY_ATTRIBUTES security{sizeof(security), descriptor, FALSE};
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, &security,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  LocalFree(descriptor);
  Require(file != INVALID_HANDLE_VALUE, "Cannot create LAN file");
  DWORD written = 0;
  ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written,
                 nullptr) &&
       written == text.size() && FlushFileBuffers(file);
  CloseHandle(file);
  if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(temporary.c_str());
    throw std::runtime_error("Cannot persist LAN file");
  }
#else
  int fd =
      open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  Require(fd >= 0, "Cannot create private LAN file");
  size_t offset = 0;
  while (offset < text.size()) {
    const auto count = write(fd, text.data() + offset, text.size() - offset);
    if (count <= 0) break;
    offset += static_cast<size_t>(count);
  }
  bool ok = offset == text.size() && fsync(fd) == 0;
  close(fd);
  if (!ok || rename(temporary.c_str(), path.c_str()) != 0) {
    unlink(temporary.c_str());
    throw std::runtime_error("Cannot persist LAN file");
  }
#endif
}
#ifdef _WIN32
std::vector<unsigned char> CurrentUser() {
  HANDLE token = nullptr;
  Require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token),
          "Cannot open user token");
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::vector<unsigned char> data(size);
  bool ok = GetTokenInformation(token, TokenUser, data.data(), size, &size);
  CloseHandle(token);
  Require(ok, "Cannot read user token");
  return data;
}
void CheckPrivateWindowsPath(const std::filesystem::path& path) {
  const auto attributes = GetFileAttributesW(path.c_str());
  Require(attributes != INVALID_FILE_ATTRIBUTES &&
              !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
          "LAN path cannot be a reparse point");
  auto user = CurrentUser();
  auto sid = reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid;
  PSID owner = nullptr;
  PACL acl = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  Require(GetNamedSecurityInfoW(
              const_cast<wchar_t*>(path.c_str()), SE_FILE_OBJECT,
              OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner,
              nullptr, &acl, nullptr, &descriptor) == ERROR_SUCCESS,
          "Cannot read LAN file permissions");
  bool ok = owner && EqualSid(owner, sid) && acl && acl->AceCount > 0;
  for (DWORD i = 0; ok && i < acl->AceCount; ++i) {
    void* raw = nullptr;
    if (!GetAce(acl, i, &raw)) {
      ok = false;
      break;
    }
    const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw);
    ok = ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE &&
         EqualSid(const_cast<DWORD*>(&ace->SidStart), sid);
  }
  LocalFree(descriptor);
  Require(ok, "LAN files must be accessible only by the current user");
}
void CreatePrivateDirectory(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    auto user = CurrentUser();
    LPWSTR sid = nullptr;
    Require(ConvertSidToStringSidW(
                reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid),
            "Cannot read user SID");
    const auto acl = L"D:P(A;OICI;FA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    Require(ConvertStringSecurityDescriptorToSecurityDescriptorW(
                acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr),
            "Cannot protect LAN directory");
    SECURITY_ATTRIBUTES security{sizeof(security), descriptor, FALSE};
    bool ok = CreateDirectoryW(path.c_str(), &security);
    LocalFree(descriptor);
    Require(ok, "Cannot create LAN directory");
  }
  CheckPrivateWindowsPath(path);
}
#endif
std::string Read(const std::filesystem::path& path) {
#ifdef _WIN32
  CheckPrivateWindowsPath(path);
#else
  struct stat status{};
  Require(lstat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
              status.st_uid == getuid() && (status.st_mode & 0077) == 0,
          "LAN file must be owned by this user with mode 0600");
#endif
  Require(std::filesystem::file_size(path) <= 64 * 1024, "LAN file too large");
  std::ifstream input(path, std::ios::binary);
  Require(input.good(), "Cannot read LAN file");
  return {std::istreambuf_iterator<char>(input), {}};
}
std::string BioText(BIO* bio) {
  char* data = nullptr;
  long size = BIO_get_mem_data(bio, &data);
  Require(size > 0, "Cannot encode LAN identity");
  return {data, static_cast<size_t>(size)};
}
struct Secret {
  std::string value;
  ~Secret() { OPENSSL_cleanse(value.data(), value.size()); }
};
std::string Unhex(const std::string& value, size_t bytes) {
  Require(IsHex(value, bytes * 2), "Invalid saved LAN password");
  std::string result(bytes, '\0');
  auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  for (size_t i = 0; i < bytes; ++i)
    result[i] =
        static_cast<char>((digit(value[2 * i]) << 4) | digit(value[2 * i + 1]));
  return result;
}
std::string PasswordKey(const std::filesystem::path& directory, bool create) {
  const auto path = directory / "password-key";
  if (create && !std::filesystem::exists(path)) {
    Secret key{std::string(32, '\0')};
    Require(
        RAND_bytes(reinterpret_cast<unsigned char*>(key.value.data()), 32) == 1,
        "Cannot generate LAN password storage key");
    PrivateWrite(path, key.value);
  }
  auto key = Read(path);
  Require(key.size() == 32, "Invalid LAN password storage key");
  return key;
}
// The independent local key and credential files use owner-only permissions.
// Encryption avoids plaintext in identity.json; it does not protect against
// an attacker who can read both this user's key and credential files.
nlohmann::json EncryptPassword(const std::string& password,
                               const std::string& key,
                               const std::string& context) {
  const auto nonce = Unhex(RandomHex(12), 12);
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> cipher(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  std::array<unsigned char, 32> data{}, tag{};
  int size = 0, tail = 0;
  Require(
      cipher &&
          EVP_EncryptInit_ex(
              cipher.get(), EVP_aes_256_gcm(), nullptr,
              reinterpret_cast<const unsigned char*>(key.data()),
              reinterpret_cast<const unsigned char*>(nonce.data())) == 1 &&
          EVP_EncryptUpdate(
              cipher.get(), nullptr, &size,
              reinterpret_cast<const unsigned char*>(context.data()),
              static_cast<int>(context.size())) == 1 &&
          EVP_EncryptUpdate(
              cipher.get(), data.data(), &size,
              reinterpret_cast<const unsigned char*>(password.data()),
              static_cast<int>(password.size())) == 1 &&
          EVP_EncryptFinal_ex(cipher.get(), data.data() + size, &tail) == 1 &&
          EVP_CIPHER_CTX_ctrl(cipher.get(), EVP_CTRL_GCM_GET_TAG, 16,
                              tag.data()) == 1,
      "Cannot save LAN password");
  return {{"version", 1},
          {"algorithm", "aes-256-gcm"},
          {"nonce", Hex(reinterpret_cast<const unsigned char*>(nonce.data()),
                        nonce.size())},
          {"data", Hex(data.data(), size + tail)},
          {"tag", Hex(tag.data(), 16)}};
}
std::string DecryptPassword(const nlohmann::json& record,
                            const std::string& key,
                            const std::string& context) {
  Require(record.at("version") == 1 && record.at("algorithm") == "aes-256-gcm",
          "Invalid saved LAN password");
  const auto nonce = Unhex(record.at("nonce"), 12);
  const auto data = Unhex(record.at("data"), 6);
  auto tag = Unhex(record.at("tag"), 16);
  Secret plain{std::string(32, '\0')};
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> cipher(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  int size = 0, tail = 0;
  Require(cipher &&
              EVP_DecryptInit_ex(
                  cipher.get(), EVP_aes_256_gcm(), nullptr,
                  reinterpret_cast<const unsigned char*>(key.data()),
                  reinterpret_cast<const unsigned char*>(nonce.data())) == 1 &&
              EVP_DecryptUpdate(
                  cipher.get(), nullptr, &size,
                  reinterpret_cast<const unsigned char*>(context.data()),
                  static_cast<int>(context.size())) == 1 &&
              EVP_DecryptUpdate(
                  cipher.get(),
                  reinterpret_cast<unsigned char*>(plain.value.data()), &size,
                  reinterpret_cast<const unsigned char*>(data.data()),
                  static_cast<int>(data.size())) == 1 &&
              EVP_CIPHER_CTX_ctrl(cipher.get(), EVP_CTRL_GCM_SET_TAG, 16,
                                  tag.data()) == 1 &&
              EVP_DecryptFinal_ex(
                  cipher.get(),
                  reinterpret_cast<unsigned char*>(plain.value.data()) + size,
                  &tail) == 1 &&
              size + tail == 6,
          "Cannot read saved LAN password");
  auto password = plain.value.substr(0, 6);
  Require(ValidPassword(password), "Invalid saved LAN password");
  return password;
}
void CreateCertificate(const std::filesystem::path& key_path,
                       const std::filesystem::path& cert_path) {
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"), EVP_PKEY_free);
  std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
  Require(key && cert, "Cannot generate LAN identity");
  X509_set_version(cert.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
  X509_gmtime_adj(X509_getm_notBefore(cert.get()), -300);
  X509_gmtime_adj(X509_getm_notAfter(cert.get()), 10L * 365 * 86400);
  X509_set_pubkey(cert.get(), key.get());
  auto* name = X509_get_subject_name(cert.get());
  X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_ASC,
      reinterpret_cast<const unsigned char*>("CrossDesk LAN"), -1, -1, 0);
  X509_set_issuer_name(cert.get(), name);
  for (const auto& extension :
       {std::pair<int, const char*>{NID_basic_constraints, "critical,CA:FALSE"},
        {NID_key_usage, "critical,digitalSignature"},
        {NID_ext_key_usage, "serverAuth"}}) {
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(
        nullptr, nullptr, extension.first, const_cast<char*>(extension.second));
    Require(ext != nullptr, "Cannot create certificate extension");
    const int result = X509_add_ext(cert.get(), ext, -1);
    X509_EXTENSION_free(ext);
    Require(result == 1, "Cannot install certificate extension");
  }
  Require(X509_sign(cert.get(), key.get(), EVP_sha256()) > 0,
          "Cannot sign LAN certificate");
  std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), BIO_free);
  Require(PEM_write_bio_PrivateKey(bio.get(), key.get(), nullptr, nullptr, 0,
                                   nullptr, nullptr) == 1,
          "Cannot encode private key");
  PrivateWrite(key_path, BioText(bio.get()));
  (void)BIO_reset(bio.get());
  Require(PEM_write_bio_X509(bio.get(), cert.get()) == 1,
          "Cannot encode certificate");
  PrivateWrite(cert_path, BioText(bio.get()));
}
}  // namespace
unsigned ParsePort(const std::string& value) {
  unsigned port = 0;
  auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), port);
  Require(error == std::errc{} && end == value.data() + value.size() &&
              port > 0 && port <= 65535,
          "Invalid LAN port");
  return port;
}
bool IsIpv4(const std::string& value) {
  unsigned octets[4]{};
  size_t offset = 0;
  for (unsigned i = 0; i < 4; ++i) {
    size_t end = value.find('.', offset);
    if (i == 3) end = value.size();
    if (end == std::string::npos || end <= offset || end - offset > 3 ||
        (end - offset > 1 && value[offset] == '0'))
      return false;
    for (size_t j = offset; j < end; ++j) {
      if (value[j] < '0' || value[j] > '9') return false;
      octets[i] = octets[i] * 10 + value[j] - '0';
    }
    if (octets[i] > 255) return false;
    offset = end + 1;
  }
  return offset == value.size() + 1 && octets[0] > 0 && octets[0] != 127 &&
         octets[0] < 224;
}
std::pair<std::string, unsigned> ParseEndpoint(const std::string& value,
                                               unsigned default_port) {
  std::string address = value;
  const auto colon = address.find(':');
  unsigned port = default_port;
  if (colon != std::string::npos) {
    port = ParsePort(address.substr(colon + 1));
    address.resize(colon);
  }
  Require(IsIpv4(address) && port > 0 && port <= 65535,
          "Enter a numeric IPv4 address or IPv4:port");
  return {address, port};
}
std::vector<Interface> PhysicalInterfaces() {
  std::vector<Interface> result;
#ifdef _WIN32
  ULONG bytes = 16384;
  std::vector<unsigned char> buffer(bytes);
  auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
  ULONG code =
      GetAdaptersAddresses(AF_INET,
                           GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                               GAA_FLAG_SKIP_DNS_SERVER,
                           nullptr, adapters, &bytes);
  if (code == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(bytes);
    adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    code = GetAdaptersAddresses(AF_INET, 0, nullptr, adapters, &bytes);
  }
  if (code != NO_ERROR) return result;
  for (auto* a = adapters; a; a = a->Next) {
    if (a->OperStatus != IfOperStatusUp ||
        (a->IfType != IF_TYPE_ETHERNET_CSMACD &&
         a->IfType != IF_TYPE_IEEE80211) ||
        a->PhysicalAddressLength == 0)
      continue;
    // HardwareInterface rejects software adapters that imitate Ethernet.
    MIB_IF_ROW2 row{};
    row.InterfaceLuid = a->Luid;
    if (GetIfEntry2(&row) != NO_ERROR ||
        !row.InterfaceAndOperStatusFlags.HardwareInterface)
      continue;
    for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
      char address[INET_ADDRSTRLEN]{};
      inet_ntop(
          AF_INET,
          &reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr)->sin_addr,
          address, sizeof(address));
      if (IsIpv4(address)) result.push_back({address});
    }
  }
#else
  ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) return result;
  for (auto* a = list; a; a = a->ifa_next) {
    if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET ||
        !(a->ifa_flags & IFF_UP) ||
        (a->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)))
      continue;
    bool physical = false;
#ifdef __APPLE__
    for (auto* link = list; link; link = link->ifa_next)
      if (link->ifa_addr && link->ifa_addr->sa_family == AF_LINK &&
          std::string(link->ifa_name) == a->ifa_name) {
        const auto* dl = reinterpret_cast<sockaddr_dl*>(link->ifa_addr);
        physical = dl->sdl_type == IFT_ETHER && dl->sdl_alen == 6 &&
                   std::string(a->ifa_name).rfind("en", 0) == 0;
      }
#else
    physical = std::filesystem::exists(std::filesystem::path("/sys/class/net") /
                                       a->ifa_name / "device");
#endif
    if (!physical) continue;
    char address[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr,
              address, sizeof(address));
    if (IsIpv4(address)) result.push_back({address});
  }
  freeifaddrs(list);
#endif
  return result;
}
std::string RandomHex(size_t bytes) {
  std::vector<unsigned char> data(bytes);
  Require(RAND_bytes(data.data(), static_cast<int>(bytes)) == 1,
          "Random generator failed");
  return Hex(data.data(), bytes);
}
bool ValidPassword(const std::string& password) {
  return password.size() == 6 &&
         password.find_first_not_of(
             "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy"
             "z") == std::string::npos;
}
bool VerifyLanCertificate(X509* cert) {
  if (!cert || !X509_get0_pubkey(cert)) return false;
  return X509_cmp_current_time(X509_get0_notBefore(cert)) < 0 &&
         X509_cmp_current_time(X509_get0_notAfter(cert)) > 0 &&
         X509_verify(cert, X509_get0_pubkey(cert)) == 1 &&
         X509_check_purpose(cert, X509_PURPOSE_SSL_SERVER, 0) == 1 &&
         (X509_get_key_usage(cert) & KU_DIGITAL_SIGNATURE) != 0;
}
Identity::Identity(std::filesystem::path directory)
    : directory_(std::move(directory)) {}
void Identity::Load() {
  DevicePassword::CheckSupport();
  std::filesystem::create_directories(directory_.parent_path());
#ifdef _WIN32
  CreatePrivateDirectory(directory_);
#else
  std::filesystem::create_directories(directory_);
  struct stat directory_status{};
  Require(lstat(directory_.c_str(), &directory_status) == 0 &&
              S_ISDIR(directory_status.st_mode) &&
              directory_status.st_uid == getuid(),
          "Invalid LAN directory owner or type");
  Require(chmod(directory_.c_str(), 0700) == 0, "Cannot protect LAN directory");
#endif
  const auto store = directory_ / "identity.json";
  if (!std::filesystem::exists(store)) {
    Require(!std::filesystem::exists(private_key()) &&
                !std::filesystem::exists(certificate()),
            "Incomplete LAN identity; manual recovery required");
    CreateCertificate(private_key(), certificate());
    state_ = {
        {"version", 1}, {"device_id", RandomHex(16)}, {"verifier", nullptr}};
    Save(state_);
  } else
    state_ = nlohmann::json::parse(Read(store));
  Require(state_.at("version") == 1 &&
              IsHex(state_.at("device_id").get<std::string>(), 32),
          "Invalid LAN identity store");
  // Legacy trust records no longer affect connections. Drop them from memory
  // and from the next ordinary settings/credential save, without requiring a
  // migration write before an outgoing connection can authenticate.
  state_.erase("trust");
  // Migrate the single endpoint in memory; the next ordinary save persists it.
  // Ports belong to exact IP addresses, including addresses currently offline.
  auto& settings = state_["settings"];
  if (settings.is_null()) settings = nlohmann::json::object();
  Require(settings.is_object(), "Invalid LAN settings");
  const auto address = settings.value("address", std::string{});
  Require(address.empty() || IsIpv4(address), "Invalid LAN listener address");
  if (!settings.contains("ports")) {
    settings["ports"] = nlohmann::json::object();
    if (!address.empty())
      settings["ports"][address] = settings.contains("port")
                                       ? settings["port"]
                                       : nlohmann::json(52888u);
  }
  Require(settings["ports"].is_object(), "Invalid LAN port settings");
  for (const auto& [ip, port] : settings["ports"].items())
    Require(IsIpv4(ip) && port.is_number_integer() && port > 0 && port <= 65535,
            "Invalid LAN port settings");
  settings["address"] = address;
  settings["version"] = 2;
  settings.erase("enabled");
  settings.erase("interface");
  settings.erase("port");
  id_ = state_.at("device_id");
  auto pem = Read(certificate());
  std::unique_ptr<BIO, decltype(&BIO_free)> bio(
      BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
  std::unique_ptr<X509, decltype(&X509_free)> cert(
      PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free);
  Require(VerifyLanCertificate(cert.get()),
          "LAN certificate invalid or expired");
  auto secret = Read(private_key());
  std::unique_ptr<BIO, decltype(&BIO_free)> key_bio(
      BIO_new_mem_buf(secret.data(), static_cast<int>(secret.size())),
      BIO_free);
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr, nullptr),
      EVP_PKEY_free);
  Require(key && X509_check_private_key(cert.get(), key.get()) == 1,
          "LAN private key does not match certificate");
  OPENSSL_cleanse(secret.data(), secret.size());
  if (!state_["verifier"].is_null()) {
    const auto& v = state_["verifier"];
    if (v.at("version") == 1) {
      Require(v.at("algorithm") == "scrypt-32768-8-1" &&
                  IsHex(v.at("salt"), 32) && IsHex(v.at("hash"), 64),
              "Invalid LAN password verifier");
    } else if (v.at("version") == 2) {
      // Legacy PAKE records are read only to recover the locally encrypted
      // password for migration. They are never used by the network protocol.
      Require(v.at("algorithm") == "spake2p-p256-scrypt-32768-8-1" &&
                  IsHex(v.at("salt"), 32) && IsHex(v.at("w0"), 64) &&
                  IsHex(v.at("L"), 130),
              "Invalid LAN password verifier");
    } else {
      Require(CurrentVerifier(v), "Invalid LAN password verifier");
    }
  }
  // Lifetime lockouts are obsolete; retain only expiring in-memory cooldowns.
  state_.erase("password_attempts");
}
void Identity::Save(const nlohmann::json& state) {
  PrivateWrite(directory_ / "identity.json", state.dump(2));
}
std::pair<std::string, nlohmann::json> Identity::GeneratePassword() {
  constexpr std::string_view alphabet =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  constexpr auto unbiased_limit = 256 - 256 % alphabet.size();
  std::string password;
  // Match cloud password generation: unbiased ASCII alphanumeric characters,
  // regenerating all-numeric results. Existing numeric passwords remain valid.
  do {
    password.clear();
    while (password.size() < 6) {
      unsigned char byte = 0;
      Require(RAND_bytes(&byte, 1) == 1, "Cannot generate LAN password");
      if (byte < unbiased_limit) password += alphabet[byte % alphabet.size()];
    }
  } while (password.find_first_not_of("0123456789") == std::string::npos);
  return {password, RegisterPassword(password)};
}
void Identity::SetPassword(const std::string& password,
                           const nlohmann::json& verifier) {
  Require(ValidPassword(password),
          "LAN password must contain exactly 6 ASCII letters or digits");
  Require(CurrentVerifier(verifier), "Invalid LAN password verifier");
  Secret key{PasswordKey(directory_, !state_.contains("password"))};
  auto next = state_;
  next["verifier"] = verifier;
  next["password"] = EncryptPassword(
      password, key.value,
      "crossdesk.lan.password.v1:" + id_ + ":" + verifier.dump());
  Save(next);
  state_ = std::move(next);
}
std::string Identity::ReadPassword() const {
  // Verifier-only legacy records require a local password entry to migrate.
  if (!state_.contains("password")) return {};
  Secret key{PasswordKey(directory_, false)};
  return DecryptPassword(
      state_.at("password"), key.value,
      "crossdesk.lan.password.v1:" + id_ + ":" + verifier().dump());
}
nlohmann::json Identity::RegisterPassword(const std::string& password) {
  Require(ValidPassword(password),
          "LAN password must contain exactly 6 ASCII letters or digits");
  const auto record = DevicePassword::Hash(password);
  Require(record.has_value(), "Cannot derive LAN password verifier");
  return {{"version", 3}, {"salt", record->salt}, {"hash", record->hash}};
}
bool Identity::CurrentVerifier(const nlohmann::json& verifier) {
  return verifier.is_object() && verifier.value("version", 0) == 3 &&
         verifier.contains("salt") && verifier["salt"].is_string() &&
         verifier.contains("hash") && verifier["hash"].is_string() &&
         DevicePassword::IsCurrent({verifier["salt"], verifier["hash"]});
}
bool Identity::VerifyPassword(const nlohmann::json& verifier,
                              const std::string& password) {
  if (!ValidPassword(password) || !CurrentVerifier(verifier)) return false;
  return DevicePassword::Verify(
      DevicePasswordRecord{verifier["salt"], verifier["hash"]}, password);
}

void Identity::SaveSettings(const std::string& address, unsigned port) {
  Require(IsIpv4(address) && port > 0 && port <= 65535,
          "Invalid LAN listener address");
  auto next = state_;
  const auto interfaces = PhysicalInterfaces();
  const auto selected = std::find_if(
      interfaces.begin(), interfaces.end(),
      [&](const auto& i) { return i.address == address; });
  Require(selected != interfaces.end(), "Selected LAN interface is unavailable");
  next["settings"]["ports"][address] = port;
  Save(next);
  state_ = std::move(next);
}
unsigned Identity::PortFor(const std::string& address) const {
  return state_.at("settings").at("ports").value(address, 52888u);
}
bool Identity::HasPortFor(const std::string& address) const {
  return state_.at("settings").at("ports").contains(address);
}
void Identity::SelectAddress(const std::string& address) {
  const auto interfaces = PhysicalInterfaces();
  Require(std::any_of(interfaces.begin(), interfaces.end(),
                      [&](const auto& i) { return i.address == address; }),
          "Selected LAN interface is unavailable");
  if (state_["settings"].value("address", "") == address) return;
  auto next = state_;
  next["settings"]["address"] = address;
  Save(next);
  state_ = std::move(next);
}
}  // namespace crossdesk::lan
