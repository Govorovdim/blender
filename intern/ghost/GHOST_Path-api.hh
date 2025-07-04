/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <optional>
#include <string>

#include "GHOST_Types.h"

GHOST_DECLARE_HANDLE(GHOST_SystemPathsHandle);

/**
 * Creates the one and only instance of the system path access.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_CreateSystemPaths();

/**
 * Disposes the one and only system.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeSystemPaths();

/**
 * Determine the base directory in which shared resources are located.
 * It will first try to use "unpack and run" path, then look for properly
 * installed path, including versioning.
 * \return Unsigned char string pointing to system directory (eg `/usr/share/blender/`).
 *
 * \note typically: `BKE_appdir_resource_path_id(BLENDER_RESOURCE_PATH_SYSTEM, false)` should be
 * used instead of this function directly as it ensures environment variable overrides are used.
 */
extern const char *GHOST_getSystemDir(int version, const char *versionstr);

/**
 * Determine the base directory in which user configuration is stored, including versioning.
 * \return Unsigned char string pointing to user directory (eg ~).
 *
 * \note typically: `BKE_appdir_resource_path_id(BLENDER_RESOURCE_PATH_USER, false)` should be
 * used instead of this function directly as it ensures environment variable overrides are used.
 */
extern const char *GHOST_getUserDir(int version, const char *versionstr);

/**
 * Determine a special ("well known") and easy to reach user directory.
 * \return If successfull, a string containing the user directory path (eg `~/Documents/`).
 */
extern std::optional<std::string> GHOST_getUserSpecialDir(GHOST_TUserSpecialDirTypes type);

/**
 * Determine the directory in which the binary file is found.
 * \return Unsigned char string pointing to binary directory (eg ~/usr/local/bin/).
 */
extern const char *GHOST_getBinaryDir();

/**
 * Add the file to the operating system most recently used files
 */
extern void GHOST_addToSystemRecentFiles(const char *filepath);
