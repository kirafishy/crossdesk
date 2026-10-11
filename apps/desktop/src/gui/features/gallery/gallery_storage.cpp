#include "gallery_storage.h"

#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace crossdesk::gallery {
bool WriteAtomically(const std::filesystem::path &path,
                     const std::string &text) {
  auto temporary = path;
  temporary += ".tmp";
  try {
    {
      std::ofstream output;
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output.open(temporary, std::ios::binary | std::ios::trunc);
      output.write(text.data(), text.size());
      output.close();
    }
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return true;
#else
    std::filesystem::rename(temporary, path);
    return true;
#endif
  } catch (...) {
  }
  std::error_code ignored;
  std::filesystem::remove(temporary, ignored);
  return false;
}
} // namespace crossdesk::gallery
