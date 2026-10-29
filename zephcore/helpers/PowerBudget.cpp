/* SPDX-License-Identifier: MIT
 * See PowerBudget.h; sampling rides existing main-thread events.
 */

#include "PowerBudget.h"
#include "FeatureObservation.h"
#include <stdio.h>
#include <helpers/battery_curve.h>
#include <zephyr/kernel.h>

namespace {
mesh::MainBoard *board;
const NodePrefs *prefs;
PowerBudget budget, cadence;
WinterObservation winter;
}

void power_budget_bind(mesh::MainBoard &b, const NodePrefs &p) {
	board = &b;
	prefs = &p;
	budget = PowerBudget();
	cadence = PowerBudget();
	winter = WinterObservation();
}

uint8_t power_budget_factor(void) {
	if (!board || !prefs || (!prefs->adaptive_power && !prefs->winter)) {
		budget.sample(false, false, 0, 0);
		cadence = PowerBudget();
		return 1;
	}
	if (!prefs->adaptive_power) budget.sample(false, false, 0, 0);
	uint32_t now = uint32_t(k_uptime_get());
	if (cadence.due(now)) {
		uint16_t mv = board->getBattMilliVolts();
		bool external = board->isExternalPowered();
		if (prefs->winter) winter.sample(mv, board->hasUsbPowerDetect() && board->isUsbPowered());
		budget.sample(prefs->adaptive_power, external, mv,
			battery_curve_lookup(&battery_curve_default, mv));
	}
	return budget.factor();
}

void winter_observation_reset(void) {
	winter = WinterObservation();
}

void winter_observation_status(char *reply, unsigned size) {
	snprintf(reply, size,
		"%s auto=%s state=%s samples=%u first=%u last=%u min=%u max=%u usb=%s history=RAM",
		prefs && prefs->winter ? "on" : "off",
		prefs && prefs->winter_auto ? "on" : "off",
		!prefs || !prefs->winter ? "off" : !winter.valid ? "insufficient-evidence" : "observe",
		(unsigned)winter.samples, winter.first_mv, winter.last_mv,
		winter.min_mv, winter.max_mv,
		board && board->hasUsbPowerDetect() ? (winter.usb_detected ? "detected" : "absent") : "unknown");
}

uint32_t power_budget_scale(uint32_t base) {
	return PowerBudget::scale(base, power_budget_factor());
}

uint32_t power_budget_sensor_age_ms(void) {
	uint8_t factor = power_budget_factor();
	return factor == 4 ? 300000 : factor == 2 ? 60000 : 0;
}
