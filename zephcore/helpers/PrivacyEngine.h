/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>
#include <stddef.h>

struct PrivacyZone {
	char name[12];
	int32_t lat_e6, lon_e6;
	uint32_t radius_m;
};

struct PrivacyPrefs {
	PrivacyZone zones[4];
	uint32_t fuzz_m, precision_m, movement_m, delay_s;
	uint16_t start_min, end_min;
	bool schedule;
	bool recovery_hold;
};

/* Main-thread policy; coordinates and delay history never enter the packet pool. */
class PrivacyEngine {
public:
	struct Position { double lat, lon, altitude; };
	void reset(uint32_t random);
	void invalidate() { clear(); }
	void observe(const PrivacyPrefs &p, Position pos, uint32_t now_ms);
	bool filter(const PrivacyPrefs &p, Position &pos, uint32_t now_ms,
		    int64_t utc, int8_t timezone);
	bool blocked(const PrivacyPrefs &p, Position pos, int64_t utc, int8_t timezone) const;
	uint32_t generation() const { return _generation; }
	static bool valid(const PrivacyPrefs &p);
	static bool active(const PrivacyPrefs &p);
	static bool canConfigure(bool local, bool usb, bool authenticated_ble)
	{
		return local && (usb || authenticated_ble);
	}
	static double distance(Position a, Position b);
	/* Returns false for other CLI families. Mutations are staged in p by the caller. */
	static bool command(PrivacyPrefs &p, const char *line, char *reply, size_t size);

private:
	struct Sample { Position pos; uint32_t at; };
	Sample _history[8]{};
	Position _released{};
	uint32_t _generation = 1, _random = 0;
	uint8_t _count = 0;
	bool _have_released = false;
	static bool inZone(const PrivacyPrefs &p, Position pos);
	static bool blackout(const PrivacyPrefs &p, int64_t utc, int8_t timezone);
	void clear();
};
