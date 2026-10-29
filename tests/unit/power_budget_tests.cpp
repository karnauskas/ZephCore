#include "test.h"
#include <helpers/PowerBudget.h>
#include <helpers/FeatureObservation.h>

TEST(power_reserve, "UNIT-BUDGET-001", "Reserve bands confirm three samples and recover with hysteresis") {
	PowerBudget p;
	CHECK(p.due(0xfffffff0u));
	CHECK(!p.due(100));
	CHECK(!p.due(29983));
	CHECK(p.due(29984));
	p.sample(true, false, 3500, 20);
	p.sample(true, false, 3500, 20);
	CHECK(p.factor() == 1);
	p.sample(true, false, 3500, 20);
	CHECK(p.factor() == 2);
	for (int i = 0; i < 5; ++i) p.sample(true, false, 3500, 24);
	CHECK(p.factor() == 2);
	for (int i = 0; i < 3; ++i) p.sample(true, false, 3300, 10);
	CHECK(p.factor() == 4);
	for (int i = 0; i < 5; ++i) p.sample(true, false, 3500, 14);
	CHECK(p.factor() == 4);
	for (int i = 0; i < 3; ++i) p.sample(true, false, 3500, 15);
	CHECK(p.factor() == 2);
	for (int i = 0; i < 3; ++i) p.sample(true, false, 3700, 25);
	CHECK(p.factor() == 1);
}

TEST(power_resets, "UNIT-BUDGET-002", "Disabled external power absent battery and alternating readings preserve normal service") {
	PowerBudget p;
	for (int i = 0; i < 3; ++i) p.sample(true, false, 3300, 5);
	CHECK(p.factor() == 4);
	p.sample(true, true, 3300, 5);
	CHECK(p.factor() == 1);
	for (int i = 0; i < 10; ++i) {
		p.sample(true, false, 3500, 20);
		p.sample(true, false, 3700, 30);
	}
	CHECK(p.factor() == 1);
	for (int i = 0; i < 3; ++i) p.sample(true, false, 3300, 5);
	p.sample(true, false, 0, 0);
	CHECK(p.factor() == 1);
	for (int i = 0; i < 3; ++i) p.sample(true, false, 3300, 5);
	p.sample(false, false, 3300, 5);
	CHECK(p.factor() == 1);
	CHECK(PowerBudget::scale(0, 4) == 0);
	CHECK(PowerBudget::scale(300000, 4) == 1200000);
	CHECK(PowerBudget::scale(1209600000, 4) == INT32_MAX);
}

TEST(power_cache, "UNIT-BUDGET-003", "Sensor samples expire at the bound and invalidation and uptime wrap are safe") {
	BudgetSample<int> c;
	int out = 0;
	CHECK(!c.get(0, 60000, out));
	c.put(0xfffffff0u, 42);
	CHECK(c.get(10, 60000, out) && out == 42);
	CHECK(!c.get(59984, 60000, out));
	CHECK(!c.get(1, 0, out));
	c.clear();
	CHECK(!c.get(10, 60000, out));
}

TEST(observation_flags, "UNIT-OBSERVE-001", "Observation flags require exact keys and boolean values") {
	NodePrefs prefs{};
	CHECK(observationFlag(prefs, "immune") == &prefs.immune);
	CHECK(observationFlag(prefs, "winter") == &prefs.winter);
	CHECK(observationFlag(prefs, "winter.auto") == &prefs.winter_auto);
	CHECK(!observationFlag(prefs, "winter.autox"));
	CHECK(!observationFlag(prefs, "winter.status"));
	CHECK(observationValue("on") == 1);
	CHECK(observationValue("off") == 0);
	CHECK(observationValue("default") == 0);
	CHECK(observationValue("on extra") == -1);
	CHECK(observationValue("ON") == -1);
}

TEST(winter_observation, "UNIT-OBSERVE-002", "Voltage observations exclude invalid readings and retain bounded raw summaries") {
	WinterObservation w;
	w.sample(0, false);
	CHECK(!w.valid && w.samples == 0);
	w.sample(3900, false);
	w.sample(3700, true);
	w.sample(4000, true);
	CHECK(w.valid && w.samples == 3 && w.first_mv == 3900 && w.last_mv == 4000);
	CHECK(w.min_mv == 3700 && w.max_mv == 4000 && w.usb_detected);
	w.sample(1999, false);
	CHECK(!w.valid && w.samples == 3 && w.min_mv == 3700);
	w.samples = UINT32_MAX;
	w.sample(3800, false);
	CHECK(w.samples == UINT32_MAX);
	w = WinterObservation();
	CHECK(!w.samples && !w.valid);
}

TEST(immune_observation, "UNIT-OBSERVE-003", "Recovery observations distinguish failed local restoration and subsequent receiver activity") {
	ImmuneObservation i;
	CHECK(strcmp(i.outcome(0), "none") == 0);
	i.record(false, 10, false);
	CHECK(i.cad == 1 && i.agc == 0);
	CHECK(strcmp(i.outcome(11), "failed") == 0);
	i.record(true, UINT32_MAX, true);
	CHECK(strcmp(i.outcome(UINT32_MAX), "local-restored") == 0);
	CHECK(strcmp(i.outcome(0), "activity-observed") == 0);
	i.record(false, 20, true);
	i.clearCounters(20);
	CHECK(strcmp(i.outcome(0), "local-restored") == 0);
	i.clearCounters(1);
	CHECK(strcmp(i.outcome(0), "activity-observed") == 0);
	i.cad = UINT32_MAX;
	i.record(false, 0, true);
	CHECK(i.cad == UINT32_MAX && i.agc == 1);
}

TEST(observation_runtime, "UNIT-OBSERVE-005", "Winter observation samples existing wakes once per cadence and never changes the reserve factor") {
	struct Board : mesh::MainBoard {
		unsigned reads = 0;
		uint16_t getBattMilliVolts() override { ++reads; return 3900; }
		const char *getManufacturerName() const override { return "test"; }
		void reboot() override {}
		uint8_t getStartupReason() const override { return 0; }
	} board;
	NodePrefs prefs{};
	power_budget_bind(board, prefs);
	CHECK(power_budget_factor() == 1 && board.reads == 0);
	prefs.winter = 1;
	prefs.winter_auto = 1;
	CHECK(power_budget_factor() == 1 && board.reads == 1);
	for (int i = 0; i < 100; ++i) CHECK(power_budget_factor() == 1);
	CHECK(board.reads == 1);
	char status[256];
	winter_observation_status(status, sizeof(status));
	CHECK(strstr(status, "state=observe") && strstr(status, "samples=1"));
	CHECK(strstr(status, "usb=unknown"));
	char bounded[12]; memset(bounded, '#', sizeof(bounded));
	winter_observation_status(bounded + 1, 10);
	CHECK(bounded[0] == '#' && bounded[11] == '#' && bounded[10] == 0);
	prefs.winter = 0;
	CHECK(power_budget_factor() == 1 && board.reads == 1);
	winter_observation_status(status, sizeof(status));
	CHECK(strstr(status, "state=off") && strstr(status, "samples=1"));
	winter_observation_reset();
	winter_observation_status(status, sizeof(status));
	CHECK(strstr(status, "samples=0"));
	// Unbind the test's stack-owned board and prefs.
	static Board idle_board;
	static NodePrefs idle_prefs{};
	power_budget_bind(idle_board, idle_prefs);
}
