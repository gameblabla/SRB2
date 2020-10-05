/** SRB2 CMake Configuration */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* DO NOT MODIFY config.h DIRECTLY! It will be overwritten by cmake.
 * If you want to change a configuration option here, modify it in
 * your CMakeCache.txt. config.h.in is used as a template for CMake
 * variables, so you can insert them here too.
 */

#ifdef CMAKECONFIG

#define ASSET_HASH_SRB2_PK3   "0277c9416756627004e83cbb5b2e3e28"
#define ASSET_HASH_PLAYER_DTA "49dad7b24634c89728cc3e0b689e12bb"
#define ASSET_HASH_ZONES_PK3  "f7e88afb6af7996a834c7d663144bead"
#ifdef USE_PATCH_DTA
#define ASSET_HASH_PATCH_PK3  "466cdf60075262b3f5baa5e07f0999e8"
#endif

#define SRB2_COMP_REVISION    "17ce7d57c"
#define SRB2_COMP_BRANCH      "master"

#define CMAKE_ASSETS_DIR      "/home/anonymous/Documents/DEV/srb2_old/assets"

#else

/* Manually defined asset hashes for non-CMake builds
 * Last updated 2020 / 02 / 15 - v2.2.1 - main assets
 * Last updated 2020 / 02 / 22 - v2.2.2 - patch.pk3
 * Last updated 2020 / 05 / 10 - v2.2.3 - player.dta & patch.pk3
 * Last updated 2020 / 05 / 11 - v2.2.4 - patch.pk3
 * Last updated 2020 / 07 / 07 - v2.2.5 - player.dta & patch.pk3
 * Last updated 2020 / 07 / 10 - v2.2.6 - player.dta & patch.pk3
 * Last updated 2020 / 09 / 27 - v2.2.7 - patch.pk3
 * Last updated 2020 / 10 / 02 - v2.2.8 - patch.pk3
 */
#define ASSET_HASH_SRB2_PK3   "0277c9416756627004e83cbb5b2e3e28"
#define ASSET_HASH_ZONES_PK3  "f7e88afb6af7996a834c7d663144bead"
#define ASSET_HASH_PLAYER_DTA "49dad7b24634c89728cc3e0b689e12bb"
#ifdef USE_PATCH_DTA
#define ASSET_HASH_PATCH_PK3  "466cdf60075262b3f5baa5e07f0999e8"
#endif

#endif
#endif
