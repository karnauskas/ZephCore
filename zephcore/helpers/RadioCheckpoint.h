/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

struct RadioCheckpoint {
	float freq, bw;
	uint8_t sf, cr;
	int8_t power;
	uint32_t checksum, candidate;
	uint8_t pending, attempts;
};
