/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-14
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _TINYFILEDIALOGS_H_
#define _TINYFILEDIALOGS_H_

const char* tinyfd_selectFolderDialog(const char* title, const char* path);
const char* tinyfd_openFileDialog(const char* title, const char* path, int,
                                const char* const*, const char*, int);

#endif
