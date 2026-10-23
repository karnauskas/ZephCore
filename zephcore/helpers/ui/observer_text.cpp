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
	case 0: snprintf(out, size, "Observer"); break;
	case 1: snprintf(out, size, "WiFi:%s MQTT:%s",
			s.wifi_ready ? "up" : "down", s.mqtt_connected ? "up" : "down"); break;
	case 2: snprintf(out, size, "RX %" PRIu64, (uint64_t)s.flood + s.direct); break;
	case 3: snprintf(out, size, "Flood %" PRIu32, s.flood); break;
	case 4: snprintf(out, size, "Direct %" PRIu32, s.direct); break;
	case 5: snprintf(out, size, "RX errors %" PRIu32, s.errors); break;
	case 6: duration(out, size, "Up", s.uptime_s); break;
	case 7:
		if (s.heard) duration(out, size, "Last", s.last_heard_age_s);
		else snprintf(out, size, "Last: never");
		break;
	default: if (size) out[0] = '\0'; break;
	}
}
