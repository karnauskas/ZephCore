/* SPDX-License-Identifier: MIT */
#include "PrivacyEngine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double R = 6371000.0;
bool coordinate(PrivacyEngine::Position p)
{
	return isfinite(p.lat) && isfinite(p.lon) && p.lat >= -90 && p.lat <= 90 &&
		p.lon >= -180 && p.lon <= 180 && isfinite(p.altitude);
}
double longitude(double lon)
{
	return lon - 360.0 * floor((lon + 180.0) / 360.0);
}
bool number(const char *s, double &value)
{
	char *end;
	errno = 0;
	value = strtod(s, &end);
	return *s && !*end && !errno && isfinite(value);
}
bool minute(const char *s, uint16_t &value)
{
	if (strlen(s) != 5 || s[2] != ':' || s[0] < '0' || s[0] > '2' ||
	    s[1] < '0' || s[1] > '9' || s[3] < '0' || s[3] > '5' ||
	    s[4] < '0' || s[4] > '9') return false;
	value = ((s[0] - '0') * 10 + s[1] - '0') * 60 + (s[3] - '0') * 10 + s[4] - '0';
	return value < 1440;
}
bool nameValid(const char *s)
{
	if (!*s || strlen(s) >= sizeof(PrivacyZone::name)) return false;
	for (; *s; ++s) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
		(*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return false;
	return true;
}
}

void PrivacyEngine::clear()
{
	_count = 0;
	_have_released = false;
	if (++_generation == 0) ++_generation;
}

void PrivacyEngine::reset(uint32_t random)
{
	_random = random;
	clear();
}

double PrivacyEngine::distance(Position a, Position b)
{
	double dlat = (b.lat - a.lat) * PI / 180;
	double dlon = longitude(b.lon - a.lon) * PI / 180;
	double h = sin(dlat / 2) * sin(dlat / 2) + cos(a.lat * PI / 180) *
		cos(b.lat * PI / 180) * sin(dlon / 2) * sin(dlon / 2);
	return 2 * R * asin(sqrt(fmin(1.0, fmax(0.0, h))));
}

bool PrivacyEngine::valid(const PrivacyPrefs &p)
{
	if (p.fuzz_m > 100000 || p.precision_m > 100000 || p.movement_m > 100000 ||
	    p.delay_s > 86400 || p.start_min >= 1440 || p.end_min >= 1440 ||
	    (p.schedule && p.start_min == p.end_min)) return false;
	for (const auto &z : p.zones) {
		if (!memchr(z.name, 0, sizeof(z.name))) return false;
		if (!z.name[0]) {
			if (z.radius_m || z.lat_e6 || z.lon_e6) return false;
			continue;
		}
		if (!nameValid(z.name) || !z.radius_m || z.radius_m > 100000 ||
		    z.lat_e6 < -90000000 || z.lat_e6 > 90000000 ||
		    z.lon_e6 < -180000000 || z.lon_e6 > 180000000) return false;
		for (const auto &other : p.zones)
			if (&z != &other && !strcmp(z.name, other.name)) return false;
	}
	return true;
}

bool PrivacyEngine::active(const PrivacyPrefs &p)
{
	if (p.recovery_hold || p.fuzz_m || p.precision_m || p.movement_m || p.delay_s || p.schedule) return true;
	for (const auto &z : p.zones) if (z.name[0]) return true;
	return false;
}

bool PrivacyEngine::inZone(const PrivacyPrefs &p, Position pos)
{
	if (!coordinate(pos)) return true;
	for (const auto &z : p.zones) {
		if (z.name[0] && distance(pos, {z.lat_e6 / 1e6, z.lon_e6 / 1e6, 0}) <= z.radius_m + 1.0)
			return true;
	}
	return false;
}

bool PrivacyEngine::blackout(const PrivacyPrefs &p, int64_t utc, int8_t timezone)
{
	if (!p.schedule) return false;
	/* No trusted calendar yet: the blackout fails closed. */
	if (utc < 1609459200) return true;
	int minute = (int)(((utc % 86400) / 60 + timezone * 60 + 1440) % 1440);
	return p.start_min < p.end_min ? minute >= p.start_min && minute < p.end_min :
		minute >= p.start_min || minute < p.end_min;
}

bool PrivacyEngine::blocked(const PrivacyPrefs &p, Position pos, int64_t utc, int8_t timezone) const
{
	return p.recovery_hold || !valid(p) || inZone(p, pos) || blackout(p, utc, timezone);
}

void PrivacyEngine::observe(const PrivacyPrefs &p, Position pos, uint32_t now)
{
	if (p.recovery_hold || !valid(p) || inZone(p, pos)) {
		clear();
		return;
	}
	/* Eight snapshots cover the delay even when GPS fixes arrive much faster. */
	uint32_t spacing = (p.delay_s * 1000 + 6) / 7;
	if (_count && (uint32_t)(now - _history[_count - 1].at) < spacing) return;
	if (_count == 8) {
		memmove(_history, _history + 1, 7 * sizeof(Sample));
		--_count;
	}
	_history[_count++] = {pos, now};
}

bool PrivacyEngine::filter(const PrivacyPrefs &p, Position &pos, uint32_t now,
			   int64_t utc, int8_t timezone)
{
	if (blocked(p, pos, utc, timezone)) {
		clear();
		return false;
	}
	observe(p, pos, now);
	if (p.delay_s) {
		int eligible = -1;
		for (int i = 0; i < _count; ++i)
			if ((uint32_t)(now - _history[i].at) >= p.delay_s * 1000) eligible = i;
		if (eligible < 0) return false;
		pos = _history[eligible].pos;
		if (inZone(p, pos)) return false;
	}
	if (p.movement_m && _have_released && distance(pos, _released) < p.movement_m) return false;
	Position raw = pos;
	if (p.fuzz_m) {
		/* A boot-stable bearing prevents repeated reports averaging to the true point. */
		double bearing = _random * (2 * PI / 4294967296.0);
		double arc = p.fuzz_m / R;
		double lat = pos.lat * PI / 180, lon = pos.lon * PI / 180;
		double shifted = asin(fmax(-1.0, fmin(1.0,
			sin(lat) * cos(arc) + cos(lat) * sin(arc) * cos(bearing))));
		pos.lon = longitude((lon + atan2(sin(bearing) * sin(arc) * cos(lat),
			cos(arc) - sin(lat) * sin(shifted))) * 180 / PI);
		pos.lat = shifted * 180 / PI;
	}
	if (p.precision_m) {
		double lat_step = p.precision_m / R * 180 / PI;
		pos.lat = fmax(-90.0, fmin(90.0, round(pos.lat / lat_step) * lat_step));
		double c = cos(pos.lat * PI / 180);
		double lon_step = c > 1e-6 ? fmin(360.0, lat_step / c) : 360.0;
		pos.lon = longitude(round(pos.lon / lon_step) * lon_step);
	}
	if (p.fuzz_m || p.precision_m) pos.altitude = 0;
	if (inZone(p, pos)) return false;
	_released = raw;
	_have_released = true;
	return true;
}

bool PrivacyEngine::command(PrivacyPrefs &p, const char *line, char *reply, size_t size)
{
	if (strncmp(line, "privacy", 7) || (line[7] && line[7] != ' ')) return false;
	char words[7][48]{};
	int n = 0;
	const char *s = line + 7;
	while (*s) {
		while (*s == ' ') ++s;
		if (!*s) break;
		const char *begin = s;
		while (*s && *s != ' ') ++s;
		if (n == 7 || s - begin >= 48) { snprintf(reply, size, "Error: invalid privacy command"); return true; }
		memcpy(words[n++], begin, s - begin);
	}
	bool ok = false;
	if (!n || (n == 1 && !strcmp(words[0], "status"))) {
		unsigned zones = 0;
		for (const auto &z : p.zones) if (z.name[0]) ++zones;
		snprintf(reply, size, "Privacy %s; zones=%u; fuzz=%u precision=%u movement=%u delay=%u; schedule=%s",
			p.recovery_hold ? "settings recovery (location withheld; use privacy off to clear)" :
			!valid(p) ? "invalid (location withheld)" : active(p) ? "active" : "off",
			zones, (unsigned)p.fuzz_m, (unsigned)p.precision_m,
			(unsigned)p.movement_m, (unsigned)p.delay_s, p.schedule ? "on" : "off");
		return true;
	}
	if (n == 1 && !strcmp(words[0], "off")) { p = {}; ok = true; }
	if (n == 2 && !strcmp(words[0], "zone") && !strcmp(words[1], "list")) {
		snprintf(reply, size, "Zones:");
		for (const auto &z : p.zones) if (z.name[0]) {
			size_t used = strlen(reply);
			if (used < size) snprintf(reply + used, size - used, " %s", z.name);
		}
		return true;
	}
	if (!strcmp(words[0], "gps") && n == 3) {
		double v;
		if (number(words[2], v) && v >= 0 && v <= 100000 && floor(v) == v) {
			if (!strcmp(words[1], "fuzz")) { p.fuzz_m = v; ok = true; }
			if (!strcmp(words[1], "precision")) { p.precision_m = v; ok = true; }
			if (!strcmp(words[1], "movement")) { p.movement_m = v; ok = true; }
			if (!strcmp(words[1], "delay") && v <= 86400) { p.delay_s = v; ok = true; }
		}
		if (!strcmp(words[1], "schedule") && !strcmp(words[2], "off")) { p.schedule = false; ok = true; }
	}
	if (n == 4 && !strcmp(words[0], "gps") && !strcmp(words[1], "schedule")) {
		uint16_t a, b;
		if (minute(words[2], a) && minute(words[3], b) && a != b) {
			p.start_min = a; p.end_min = b; p.schedule = true; ok = true;
		}
	}
	if (!strcmp(words[0], "zone")) {
		if (n == 6 && !strcmp(words[1], "add") && nameValid(words[2])) {
			double lat, lon, radius;
			if (number(words[3], lat) && number(words[4], lon) && number(words[5], radius) &&
			    coordinate({lat, lon, 0}) && radius >= 1 && radius <= 100000 && floor(radius) == radius) {
				PrivacyZone *slot = nullptr;
				for (auto &z : p.zones) if (!strcmp(z.name, words[2])) slot = &z;
				if (!slot) for (auto &z : p.zones) if (!z.name[0]) { slot = &z; break; }
				if (slot) {
					*slot = {}; strcpy(slot->name, words[2]);
					slot->lat_e6 = lround(lat * 1e6); slot->lon_e6 = lround(lon * 1e6);
					slot->radius_m = radius; ok = true;
				}
			}
		}
		if (n == 3 && !strcmp(words[1], "delete"))
			for (auto &z : p.zones) if (!strcmp(z.name, words[2])) { z = {}; ok = true; }
		if (n == 4)
			for (const auto &z : p.zones) if (z.name[0] && !strcmp(z.name, words[1]))
				ok = (!strcmp(words[2], "action") && !strcmp(words[3], "suppress")) ||
				     (!strcmp(words[2], "apply") && !strcmp(words[3], "all"));
	}
	snprintf(reply, size, ok ? "OK" : "Error: invalid privacy command or zone capacity reached");
	return true;
}
