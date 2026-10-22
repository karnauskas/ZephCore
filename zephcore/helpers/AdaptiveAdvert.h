#pragma once

#include <stdint.h>
#include <string.h>

class AdaptiveAdvert {
	uint8_t _keys[16][32] = {};
	uint8_t _count = 0, _next = 0;
	uint32_t _last_heard = 0, _last_new = 0;
	bool _heard = false;

public:
	bool heard(const uint8_t *key, uint32_t now) {
		_last_heard = now;
		_heard = true;
		for (unsigned i = 0; i < _count; ++i) {
			if (memcmp(_keys[i], key, 32) == 0) return false;
		}
		memcpy(_keys[_next], key, 32);
		_next = (_next + 1) % 16;
		if (_count < 16) ++_count;
		_last_new = now;
		return true;
	}

	uint32_t interval(uint32_t base, uint32_t now, float contention) const {
		if (contention >= 4.0f || !_heard || uint32_t(now - _last_heard) >= 900000)
			return base * 2;
		if (uint32_t(now - _last_new) < 600000) return base / 2;
		return base;
	}

	static uint32_t sooner(uint32_t deadline, uint32_t now, uint32_t delay) {
		if (!deadline || int32_t(deadline - now) <= 0) return deadline;
		if (uint32_t(deadline - now) <= delay) return deadline;
		uint32_t next = now + delay;
		return next ? next : 1;
	}
};
