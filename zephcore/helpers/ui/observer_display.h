/* SPDX-License-Identifier: MIT */
#pragma once

#include <stddef.h>
#include <stdint.h>

struct ObserverDisplayStatus {
	uint32_t flood, direct, errors;
	uint64_t uptime_s, last_heard_age_s;
	bool wifi_ready;
	bool mqtt_connected;
	bool heard;
	int16_t noise_floor_dbm, rssi_dbm, snr_quarters;
};

/* Eight text rows; last-heard refers to a successfully parsed mesh packet. */
void observer_display_line(const ObserverDisplayStatus &status, unsigned row,
			   char *out, size_t size);
void observer_display_draw(const ObserverDisplayStatus &status);
