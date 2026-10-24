#include "platform.h"

#include <Winsock2.h>
#include <Windows.h>
#include <iphlpapi.h>

#include <cstdio>

#include "rd_log.h"

namespace crossdesk {

std::string GetMac() {
  IP_ADAPTER_INFO adapters[16] = {};
  DWORD size = sizeof(adapters);
  if (GetAdaptersInfo(adapters, &size) != ERROR_SUCCESS) return {};

  char address[16] = {};
  int length = 0;
  const PIP_ADAPTER_INFO adapter = adapters;
  for (UINT index = 0; index < adapter->AddressLength; ++index) {
    length += sprintf_s(address + length, sizeof(address) - length, "%.2X",
                        adapter->Address[index]);
  }
  return address;
}

std::string GetHostName() {
  // Read Unicode directly; Winsock's narrow hostname is not guaranteed UTF-8.
  DWORD size = 0;
  if (!GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size) &&
      GetLastError() != ERROR_MORE_DATA) {
    LOG_ERROR("GetComputerNameExW size query failed: {}", GetLastError());
    return {};
  }
  if (size == 0) return {};

  std::wstring hostname(size, L'\0');
  if (!GetComputerNameExW(ComputerNameDnsHostname, hostname.data(), &size)) {
    LOG_ERROR("GetComputerNameExW failed: {}", GetLastError());
    return {};
  }
  if (size == 0) return {};

  const int length = static_cast<int>(size);
  const int utf8_size =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, hostname.data(),
                          length, nullptr, 0, nullptr, nullptr);
  if (utf8_size == 0) {
    LOG_ERROR("Hostname UTF-8 size query failed: {}", GetLastError());
    return {};
  }
  std::string utf8_name(utf8_size, '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, hostname.data(),
                          length, utf8_name.data(), utf8_size, nullptr,
                          nullptr) == 0) {
    LOG_ERROR("Hostname UTF-8 conversion failed: {}", GetLastError());
    return {};
  }
  return utf8_name;
}

bool IsWaylandSession() { return false; }

}  // namespace crossdesk
