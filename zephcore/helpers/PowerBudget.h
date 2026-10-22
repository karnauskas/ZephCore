/* SPDX-License-Identifier: MIT
 * Main-thread reserve policy and bounded sensor samples.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <mesh/MeshCore.h>
#include <helpers/NodePrefs.h>

class PowerBudget {
	uint8_t _factor = 1, _candidate = 1, _count = 0;
	uint32_t _sampled_at = 0;
	bool _sampled = false;
public:
	bool due(uint32_t now) {
		if (_sampled && uint32_t(now - _sampled_at) < 30000) return false;
		_sampled = true;
		_sampled_at = now;
		return true;
	}
	void sample(bool enabled, bool external, uint16_t mv, uint8_t reserve) {
		if (!enabled || external || mv < 2000) {
			if (!enabled) _sampled = false;
			_factor = _candidate = 1;
			_count = 0;
			return;
		}
		uint8_t target = reserve <= 10 ? 4 : reserve <= 20 ? 2 : 1;
		if (_factor == 4 && reserve < 15) target = 4;
		if (_factor >= 2 && target == 1 && reserve < 25) target = 2;
		if (target == _factor) { _candidate = target; _count = 0; return; }
		if (target != _candidate) { _candidate = target; _count = 0; }
		if (++_count >= 3) { _factor = target; _count = 0; }
	}
	uint8_t factor() const { return _factor; }
	static uint32_t scale(uint32_t base, uint8_t factor) {
		uint64_t scaled = uint64_t(base) * factor;
		return scaled > INT32_MAX ? INT32_MAX : uint32_t(scaled);
	}
};

template<class T> class BudgetSample {
	T _value{};
	uint32_t _at = 0;
	bool _valid = false;
public:
	bool get(uint32_t now, uint32_t age, T &out) const {
		if (!age || !_valid || uint32_t(now - _at) >= age) return false;
		out = _value;
		return true;
	}
	void put(uint32_t now, const T &value) { _value = value; _at = now; _valid = true; }
	void clear() { _valid = false; }
};

void power_budget_bind(mesh::MainBoard &board, const NodePrefs &prefs);
extern "C" {
#endif
uint8_t power_budget_factor(void);
uint32_t power_budget_scale(uint32_t base);
uint32_t power_budget_sensor_age_ms(void);
#ifdef __cplusplus
}
#endif
