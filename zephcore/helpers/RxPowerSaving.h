/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stddef.h>

struct RxPowerSavingPrefs {
	uint32_t rx_us;
	uint32_t sleep_us;
	uint8_t level;
	uint8_t preamble;
};
struct RxPowerSavingTiming { uint32_t rx_us, sleep_us; };

/* Level 0 with zero periods preserves the original automatic rxduty policy. */
bool rxPowerSavingTiming(const RxPowerSavingPrefs &prefs, uint8_t sf, uint32_t bw_hz,
	uint16_t preamble, uint16_t detect, uint32_t wake_us, uint8_t margin_pct,
	RxPowerSavingTiming &timing);
/* 0: unrelated, 1: query/error, 2: valid setting. No mutation on errors. */
int rxPowerSavingCommand(const char *command, uint8_t &enabled, RxPowerSavingPrefs &prefs,
	char *reply, size_t size);
