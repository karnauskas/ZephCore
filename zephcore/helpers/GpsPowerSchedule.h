/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

inline uint32_t gpsAcquireWindowMs(bool server, bool cold, bool first_fix, bool first_window_done,
	uint32_t first_ms, uint32_t warm_ms)
{
	return server ? 300000 : cold || (!first_fix && !first_window_done) ? first_ms : warm_ms;
}
