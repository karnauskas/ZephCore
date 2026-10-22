/* SPDX-License-Identifier: MIT
 * See PowerBudget.h; sampling rides existing main-thread events.
 */

#include "PowerBudget.h"
#include <helpers/battery_curve.h>
#include <zephyr/kernel.h>

namespace {
mesh::MainBoard *board;
const NodePrefs *prefs;
PowerBudget budget;
}

void power_budget_bind(mesh::MainBoard &b, const NodePrefs &p) {
	board = &b;
	prefs = &p;
	budget = PowerBudget();
}

uint8_t power_budget_factor(void) {
	if (!board || !prefs || !prefs->adaptive_power) {
		budget.sample(false, false, 0, 0);
		return 1;
	}
	uint32_t now = uint32_t(k_uptime_get());
	if (budget.due(now)) {
		uint16_t mv = board->getBattMilliVolts();
		budget.sample(true, board->isExternalPowered(), mv,
			battery_curve_lookup(&battery_curve_default, mv));
	}
	return budget.factor();
}

uint32_t power_budget_scale(uint32_t base) {
	return PowerBudget::scale(base, power_budget_factor());
}

uint32_t power_budget_sensor_age_ms(void) {
	uint8_t factor = power_budget_factor();
	return factor == 4 ? 300000 : factor == 2 ? 60000 : 0;
}
