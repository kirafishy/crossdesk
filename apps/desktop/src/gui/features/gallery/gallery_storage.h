/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _GALLERY_STORAGE_H_
#define _GALLERY_STORAGE_H_

#include <filesystem>
#include <string>

namespace crossdesk::gallery {
bool WriteAtomically(const std::filesystem::path &path,
                     const std::string &text);
}  // namespace crossdesk::gallery

#endif
