/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>
#include <string.h>
#include "NodePrefs.h"

inline uint8_t *observationFlag(NodePrefs &prefs, const char *key)
{
	if (strcmp(key, "immune") == 0) return &prefs.immune;
	if (strcmp(key, "winter") == 0) return &prefs.winter;
	if (strcmp(key, "winter.auto") == 0) return &prefs.winter_auto;
	return nullptr;
}

inline int observationValue(const char *value)
{
	if (strcmp(value, "on") == 0) return 1;
	if (strcmp(value, "off") == 0 || strcmp(value, "default") == 0) return 0;
	return -1;
}

/* Raw voltage observations carry no charging or battery-presence inference. */
struct WinterObservation {
	uint32_t samples = 0;
	uint16_t first_mv = 0, last_mv = 0, min_mv = 0, max_mv = 0;
	bool usb_detected = false;
	bool valid = false;

	void sample(uint16_t mv, bool external)
	{
		valid = mv >= 2000;
		usb_detected = external;
		if (!valid) return;
		if (!samples) first_mv = min_mv = max_mv = mv;
		last_mv = mv;
		if (mv < min_mv) min_mv = mv;
		if (mv > max_mv) max_mv = mv;
		if (samples != UINT32_MAX) ++samples;
	}
};

/* Counts existing remedies; it does not authorize or select a remedy. */
struct ImmuneObservation {
	uint32_t cad = 0, agc = 0;
	uint32_t rx_at_attempt = 0;
	bool attempted = false, local_restored = false, activity_observed = false;

	void record(bool is_agc, uint32_t rx, bool restored)
	{
		uint32_t &count = is_agc ? agc : cad;
		if (count != UINT32_MAX) ++count;
		rx_at_attempt = rx;
		attempted = true;
		local_restored = restored;
		activity_observed = false;
	}
	void clearCounters(uint32_t rx)
	{
		activity_observed = activity_observed || (attempted && rx != rx_at_attempt);
		rx_at_attempt = 0;
	}
	const char *outcome(uint32_t rx) const
	{
		if (!attempted) return "none";
		if (!local_restored) return "failed";
		return activity_observed || rx != rx_at_attempt ? "activity-observed" : "local-restored";
	}
};
