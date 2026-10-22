/* SPDX-License-Identifier: MIT */
#pragma once
#include <helpers/NodePrefs.h>
#include <string.h>

class RadioRecovery {
	static uint32_t hash(float freq, float bw, uint8_t sf, uint8_t cr, int8_t power) {
		uint8_t data[11];
		/* Match prefs.json four-decimal float serialization. */
		uint32_t f = uint32_t(double(freq) * 10000 + 0.5);
		uint32_t b = uint32_t(double(bw) * 10000 + 0.5);
		memcpy(data, &f, 4);
		memcpy(data + 4, &b, 4);
		data[8] = sf; data[9] = cr; data[10] = uint8_t(power);
		uint32_t h = 2166136261u;
		for (uint8_t b : data) h = (h ^ b) * 16777619u;
		return h;
	}
public:
	static uint32_t candidate(const NodePrefs &p) { return hash(p.freq, p.bw, p.sf, p.cr, p.tx_power_dbm); }
	static bool hasGood(const NodePrefs &p) {
		const auto &c = p.radio_checkpoint;
		return c.sf >= 5 && c.sf <= 12 && c.cr >= 5 && c.cr <= 8 &&
			c.freq >= ZC_RADIO_FREQ_MIN_MHZ && c.freq <= ZC_RADIO_FREQ_MAX_MHZ &&
			c.bw >= 7 && c.bw <= ZC_RADIO_BW_MAX_KHZ &&
			c.checksum == hash(c.freq, c.bw, c.sf, c.cr, c.power);
	}
	static bool restore(NodePrefs &p) {
		if (!hasGood(p) || candidate(p) == p.radio_checkpoint.checksum) return false;
		const auto &c = p.radio_checkpoint;
		p.freq = c.freq; p.bw = c.bw; p.sf = c.sf; p.cr = c.cr; p.tx_power_dbm = c.power;
		return true;
	}
	static bool prepare(NodePrefs &p) {
		auto &c = p.radio_checkpoint;
		uint32_t fingerprint = candidate(p);
		bool rolled_back = c.pending == 1 && c.attempts == 3 && c.candidate == fingerprint && restore(p);
		if (c.pending != 1 || c.candidate != candidate(p) || c.attempts > 3) c.attempts = 0;
		if (c.attempts < 3) ++c.attempts;
		c.pending = 1;
		c.candidate = candidate(p);
		return rolled_back;
	}
	static void confirm(NodePrefs &p) {
		auto &c = p.radio_checkpoint;
		c.freq = p.freq; c.bw = p.bw; c.sf = p.sf; c.cr = p.cr; c.power = p.tx_power_dbm;
		c.checksum = candidate(p);
		c.pending = c.attempts = 0;
		c.candidate = 0;
	}
};
