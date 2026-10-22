#include "test.h"
#include <helpers/AdaptiveAdvert.h>

TEST(adaptive_policy, "UNIT-ADAPTIVE-001", "Quiet discovery stable and congestion intervals") {
	AdaptiveAdvert a;
	uint8_t key[32] = {1};
	CHECK(a.interval(1000, 0, 0) == 2000);
	CHECK(a.heard(key, 100));
	CHECK(a.interval(1000, 100, 0) == 500);
	CHECK(a.interval(1000, 100, 4) == 2000);
	CHECK(!a.heard(key, 600100));
	CHECK(a.interval(1000, 600100, 0) == 1000);
	CHECK(a.interval(1000, 1500099, 0) == 1000);
	CHECK(a.interval(1000, 1500100, 0) == 2000);
	CHECK(a.interval(0, 1500100, 4) == 0);
	CHECK(a.interval(604800000, 1500100, 0) == 1209600000);
}

TEST(adaptive_wrap_cache, "UNIT-ADAPTIVE-002", "Identity cache eviction and uptime wrap") {
	AdaptiveAdvert a;
	uint8_t key[32] = {};
	CHECK(a.heard(key, 0xfffffff0u));
	CHECK(!a.heard(key, 20));
	CHECK(a.interval(1000, 20, 0) == 500);
	for (unsigned i = 1; i <= 16; ++i) {
		key[31] = i;
		CHECK(a.heard(key, 30));
		CHECK(!a.heard(key, 31));
	}
	key[31] = 0;
	CHECK(a.heard(key, 32));
	CHECK(AdaptiveAdvert::sooner(0, 100, 50) == 0);
	CHECK(AdaptiveAdvert::sooner(90, 100, 50) == 90);
	CHECK(AdaptiveAdvert::sooner(120, 100, 50) == 120);
	CHECK(AdaptiveAdvert::sooner(200, 100, 50) == 150);
	CHECK(AdaptiveAdvert::sooner(200, 0xfffffff0u, 32) == 16);
	CHECK(AdaptiveAdvert::sooner(200, 0xfffffff0u, 16) == 1);
}
