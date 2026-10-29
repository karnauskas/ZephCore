/* SPDX-License-Identifier: MIT */
#include "observer_display.h"
#include <inttypes.h>
#include <stdio.h>

static void duration(char *out, size_t size, const char *label, uint64_t seconds)
{
	snprintf(out, size, "%s %" PRIu64 "d%02u:%02u:%02u", label,
		 seconds / 86400, (unsigned)(seconds / 3600 % 24),
		 (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
}

void observer_display_line(const ObserverDisplayStatus &s, unsigned row,
			   char *out, size_t size)
{
	switch (row) {
	case 0: {
		char floor[8], rssi[8];
		if (s.noise_floor_dbm == 0) snprintf(floor, sizeof(floor), "--");
		else snprintf(floor, sizeof(floor), "%d", s.noise_floor_dbm);
		if (!s.heard) snprintf(rssi, sizeof(rssi), "--");
		else snprintf(rssi, sizeof(rssi), "%d", s.rssi_dbm);
		snprintf(out, size, "NF:%s RX:%s dBm", floor, rssi);
		break;
	}
	case 1: snprintf(out, size, "WiFi:%s MQTT:%s",
			s.wifi_ready ? "up" : "down", s.mqtt_connected ? "up" : "down"); break;
	case 2: snprintf(out, size, "RX %" PRIu64, (uint64_t)s.flood + s.direct); break;
	case 3: snprintf(out, size, "Flood %" PRIu32, s.flood); break;
	case 4: snprintf(out, size, "Direct %" PRIu32, s.direct); break;
	case 5: {
		int snr = s.snr_quarters;
		unsigned magnitude = snr < 0 ? -snr : snr;
		if (!s.heard) snprintf(out, size, "Err %" PRIu32 " SNR:--", s.errors);
		else snprintf(out, size, "Err %" PRIu32 " SNR:%s%u.%02udB", s.errors,
			snr < 0 ? "-" : "", magnitude / 4, magnitude % 4 * 25);
		break;
	}
	case 6: duration(out, size, "Up", s.uptime_s); break;
	case 7:
		if (s.heard) duration(out, size, "Last", s.last_heard_age_s);
		else snprintf(out, size, "Last: never");
		break;
	default: if (size) out[0] = '\0'; break;
	}
}
