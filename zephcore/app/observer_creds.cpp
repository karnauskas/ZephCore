/*
 * SPDX-License-Identifier: MIT
 */

#include "observer_creds.h"
#include <adapters/datastore/PrefsFile.h>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(zephcore_observer_creds, CONFIG_ZEPHCORE_DATASTORE_LOG_LEVEL);

extern "C" bool observer_creds_load(struct ObserverCreds *creds,
				    const char *base_path)
{
	char path[96];
	snprintf(path, sizeof(path), "%s/obs_creds", base_path);

	struct fs_file_t f;
	fs_file_t_init(&f);

	int rc = fs_open(&f, path, FS_O_READ);
	if (rc < 0) {
		LOG_DBG("obs_creds not found (%d), using defaults", rc);
		return false;
	}

	ssize_t n = fs_read(&f, creds, OBSERVER_LEGACY_CREDS_SIZE);
	fs_close(&f);

	if (n != (ssize_t)OBSERVER_LEGACY_CREDS_SIZE) {
		LOG_WRN("obs_creds truncated (%d/%d), resetting", (int)n,
			(int)OBSERVER_LEGACY_CREDS_SIZE);
		memset(creds, 0, sizeof(*creds));
		observer_creds_init(creds);
		return false;
	}

	mqtt_migrate_legacy(creds);
	return true;
}

extern "C" bool observer_creds_save(const struct ObserverCreds *creds,
	const char *base_path, const NodePrefs *prefs)
{
	(void)creds;
	char path[96];
	snprintf(path, sizeof(path), "%s/prefs.json", base_path);
	return zephcore_prefs_json_save(path, *prefs, serverPrefsToJson);
}
