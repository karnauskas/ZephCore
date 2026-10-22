#include "test.h"
#include <helpers/PowerBudget.h>

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
