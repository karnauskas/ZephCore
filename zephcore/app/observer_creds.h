/*
 * SPDX-License-Identifier: MIT
 * Observer runtime credentials — stored in LittleFS, configured via serial CLI.
 *
 * Network settings live in prefs.json; obs_creds is a read-only migration source.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../adapters/mqtt/uplink_creds.h"  /* struct ObserverCreds, observer_creds_init() */

#ifdef __cplusplus
extern "C" {
#endif

/* Read the legacy obs_creds prefix. Initialise the caller before loading. */
bool observer_creds_load(struct ObserverCreds *creds, const char *base_path);

struct NodePrefs;
/* Save network settings with the role preferences in prefs.json. */
bool observer_creds_save(const struct ObserverCreds *creds, const char *base_path, const struct NodePrefs *prefs);

#ifdef __cplusplus
}
#endif
