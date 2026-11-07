/*
 * FindDirectory.h for Haiku compatibility
 * Matches Haiku API: status_t find_directory(directory_which, BPath*, bool, const char*)
 * Enum values match real Haiku API from <Directory.h>
 */

#ifndef _FIND_DIRECTORY_H
#define _FIND_DIRECTORY_H

#include <Path.h>
#include <sys/types.h>

#ifdef __cplusplus

/* Haiku directory types - use int32_t to match real API */
typedef int32_t directory_which;

/* Values match Haiku system headers */
static const directory_which B_SYSTEM_SETTINGS_DIRECTORY = 0;
static const directory_which B_USER_SETTINGS_DIRECTORY = 1;
static const directory_which B_USER_CONFIG_DIRECTORY = 2;
static const directory_which B_USER_DATA_DIRECTORY = 3;
static const directory_which B_USER_NONPACKAGED_DATA_DIRECTORY = 4;
static const directory_which B_COMMON_SETTINGS_DIRECTORY = 5;
static const directory_which B_COMMON_CONFIG_DIRECTORY = 6;
static const directory_which B_COMMON_DATA_DIRECTORY = 7;
static const directory_which B_COMMON_NONPACKAGED_DATA_DIRECTORY = 8;
static const directory_which B_USER_ADDONS_DIRECTORY = 9;
static const directory_which B_SYSTEM_ADDONS_DIRECTORY = 10;
static const directory_which B_USER_BOOT_DIRECTORY = 11;
static const directory_which B_SYSTEM_BOOT_DIRECTORY = 12;
static const directory_which B_BEOS_ADDONS_DIRECTORY = 13;
static const directory_which B_SYSTEM_FONTS_DIRECTORY = 14;
static const directory_which B_USER_FONTS_DIRECTORY = 15;
static const directory_which B_COMMON_FONTS_DIRECTORY = 16;
static const directory_which B_SYSTEM_DOCUMENTATION_DIRECTORY = 17;
static const directory_which B_USER_DOCUMENTATION_DIRECTORY = 18;
static const directory_which B_SYSTEM_BIN_DIRECTORY = 19;
static const directory_which B_USER_BIN_DIRECTORY = 20;
static const directory_which B_SYSTEM_SERVERS_DIRECTORY = 21;
static const directory_which B_SYSTEM_DEVELOP_DIRECTORY = 22;
static const directory_which B_USER_DEVELOP_DIRECTORY = 23;
static const directory_which B_SYSTEM_LIB_DIRECTORY = 24;
static const directory_which B_USER_LIB_DIRECTORY = 25;
static const directory_which B_SYSTEM_NONPACKAGED_DIRECTORY = 26;
static const directory_which B_USER_NONPACKAGED_DIRECTORY = 27;
static const directory_which B_SYSTEM_NONPACKAGED_BIN_DIRECTORY = 28;
static const directory_which B_USER_NONPACKAGED_BIN_DIRECTORY = 29;
static const directory_which B_SYSTEM_NONPACKAGED_LIB_DIRECTORY = 30;
static const directory_which B_USER_NONPACKAGED_LIB_DIRECTORY = 31;
static const directory_which B_SYSTEM_NONPACKAGED_ADDONS_DIRECTORY = 32;
static const directory_which B_USER_NONPACKAGED_ADDONS_DIRECTORY = 33;
static const directory_which B_SYSTEM_NONPACKAGED_DEVELOP_DIRECTORY = 34;
static const directory_which B_USER_NONPACKAGED_DEVELOP_DIRECTORY = 35;
static const directory_which B_SYSTEM_NONPACKAGED_DOCUMENTATION_DIRECTORY = 36;
static const directory_which B_USER_NONPACKAGED_DOCUMENTATION_DIRECTORY = 37;
static const directory_which B_SYSTEM_NONPACKAGED_FONTS_DIRECTORY = 38;
static const directory_which B_USER_NONPACKAGED_FONTS_DIRECTORY = 39;
static const directory_which B_SYSTEM_NONPACKAGED_SERVERS_DIRECTORY = 40;
static const directory_which B_USER_NONPACKAGED_SERVERS_DIRECTORY = 41;
static const directory_which B_SYSTEM_NONPACKAGED_SETTINGS_DIRECTORY = 42;
static const directory_which B_USER_NONPACKAGED_SETTINGS_DIRECTORY = 43;
static const directory_which B_SYSTEM_NONPACKAGED_LIBEXEC_DIRECTORY = 44;
static const directory_which B_USER_NONPACKAGED_LIBEXEC_DIRECTORY = 45;

/* C++ overloads matching real Haiku API */
status_t find_directory(directory_which which, BPath* path, bool create = false, const char* volume = nullptr);
status_t find_directory(directory_which which, dev_t volume, BPath* path, bool create = false);

#endif /* __cplusplus */

#endif /* _FIND_DIRECTORY_H */