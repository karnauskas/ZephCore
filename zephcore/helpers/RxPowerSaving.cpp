/* SPDX-License-Identifier: MIT */
#include "RxPowerSaving.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

bool rxPowerSavingTiming(const RxPowerSavingPrefs &prefs, uint8_t sf, uint32_t bw_hz,
	uint16_t preamble, uint16_t detect, uint32_t wake_us, uint8_t margin_pct,
	RxPowerSavingTiming &timing)
{
	timing = {};
	if (sf < 5 || sf > 12 || bw_hz < 1000 || margin_pct > 100 || prefs.level > 8 ||
	    (preamble != 16 && preamble != 32) ||
	    (prefs.preamble && prefs.preamble != 16 && prefs.preamble != 32)) return false;
	if (prefs.preamble && prefs.preamble < preamble) preamble = prefs.preamble;
	if (preamble <= 2 * detect + 1) return false;
	const uint32_t sym = uint64_t(1U << sf) * 1000000 / bw_hz;
	const uint32_t budget = uint32_t(preamble - 2 * detect - 1) * sym;
	const uint32_t deaf = budget - uint64_t(budget) * margin_pct / 100;
	if (deaf <= wake_us + 2000) return false;
	uint32_t sleep = deaf - wake_us;
	uint32_t rx = uint32_t(detect + 1) * sym;
	const uint32_t complete = uint32_t(preamble + 14) * sym;
	if (!prefs.level && (prefs.rx_us || prefs.sleep_us)) {
		if (prefs.rx_us < 1000 || prefs.rx_us > 30000000 || prefs.sleep_us < 1000 ||
		    prefs.sleep_us > 30000000 || prefs.rx_us < rx || prefs.sleep_us > sleep ||
		    2ULL * prefs.rx_us + prefs.sleep_us < complete) return false;
		rx = prefs.rx_us; sleep = prefs.sleep_us;
	} else {
		if (prefs.level) {
			static const uint8_t catch16[] = {15,14,13,12,11,10,9,8};
			static const uint8_t catch32[] = {24,20,16,14,12,10,9,8};
			uint32_t caught = preamble == 32 ? catch32[prefs.level - 1] : catch16[prefs.level - 1];
			if (preamble * 5U <= caught * 5U + 1) return false;
			uint32_t requested = uint64_t(preamble * 5U - caught * 5U - 1) * sym / 5;
			if (requested <= wake_us + 2000) return false;
			if (requested - wake_us < sleep) sleep = requested - wake_us;
		}
		if (complete > sleep && rx < (complete - sleep + 1) / 2) rx = (complete - sleep + 1) / 2;
	}
	/* LR receivers take whole milliseconds; one extra RX millisecond covers
	 * kernel tick truncation and the SX126x 15.625 us timer quantization. */
	rx = ((rx + 999) / 1000 + 1) * 1000;
	sleep = sleep / 1000 * 1000;
	if (sleep < 1000) return false;
	timing = {rx, sleep}; return true;
}

int rxPowerSavingCommand(const char *command, uint8_t &enabled, RxPowerSavingPrefs &prefs,
	char *reply, size_t size)
{
	if (!strcmp(command, "get radio.rxps")) return 1;
	if (strncmp(command, "set radio.rxps ", 15)) return 0;
	const char *value = command + 15;
	RxPowerSavingPrefs proposed = prefs;
	uint8_t on = 1;
	if (!strcmp(value, "off")) on = 0;
	else if (!strcmp(value, "on") || !strcmp(value, "conservative") || !strcmp(value, "balanced") || !strcmp(value, "max")) {
		proposed = {};
		proposed.level = !strcmp(value, "balanced") ? 6 : !strcmp(value, "max") ? 8 : 3;
	} else {
		bool level = !strncmp(value, "level ", 6);
		if (level) value += 6;
		char *end;
		unsigned long first = strtoul(value, &end, 10);
		if (*value < '0' || *value > '9' || first > 30000000 || end == value) goto invalid;
		if (!*end || level) {
			if (first < 1 || first > 8) {
				snprintf(reply, size, "Error: levels 1-8 supported; 9-10 are experimental and unavailable"); return 1;
			}
			proposed = {}; proposed.level = first;
			if (*end) {
				if (strcmp(end, " preamble 16") && strcmp(end, " preamble 32")) goto invalid;
				proposed.preamble = !strcmp(end, " preamble 16") ? 16 : 32;
			}
		} else {
			if (*end++ != ' ' || first < 1000 || *end < '0' || *end > '9') goto invalid;
			char *tail; unsigned long second = strtoul(end, &tail, 10);
			if (*tail || second < 1000 || second > 30000000) goto invalid;
			proposed = {}; proposed.rx_us = first; proposed.sleep_us = second;
		}
	}
	enabled = on; prefs = proposed; return 2;
invalid:
	snprintf(reply, size, "Error: radio.rxps off|on|conservative|balanced|max|level 1-8 [preamble 16|32]|rx_us sleep_us");
	return 1;
}
