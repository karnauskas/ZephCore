#include "test.h"
#include <helpers/RadioRecovery.h>
#include <cstring>

namespace {
NodePrefs fresh() { NodePrefs p; initNodePrefs(&p); return p; }
}
TEST(recovery_checkpoint, "UNIT-RECOVERY-001", "Only startup confirmation establishes a last-known-good modem configuration") {
	auto p = fresh();
	CHECK(!RadioRecovery::hasGood(p));
	CHECK(!RadioRecovery::prepare(p));
	CHECK(p.radio_checkpoint.pending == 1 && p.radio_checkpoint.attempts == 1);
	CHECK(!RadioRecovery::hasGood(p));
	RadioRecovery::confirm(p);
	CHECK(RadioRecovery::hasGood(p));
	CHECK(p.radio_checkpoint.pending == 0 && p.radio_checkpoint.attempts == 0);
}
TEST(recovery_bootloop, "UNIT-RECOVERY-002", "Three incomplete startups restore only the unchanged failed modem candidate") {
	auto p = fresh();
	float good = p.freq;
	RadioRecovery::confirm(p);
	p.freq = 868.1f;
	strcpy(p.node_name, "Preserve identity settings");
	p.node_lat = 54.7;
	for (int i = 1; i <= 3; ++i) {
		CHECK(!RadioRecovery::prepare(p));
		CHECK(p.radio_checkpoint.attempts == i);
	}
	CHECK(RadioRecovery::prepare(p));
	CHECK(p.freq == good && p.node_lat == 54.7);
	CHECK(std::strcmp(p.node_name, "Preserve identity settings") == 0);
	CHECK(p.radio_checkpoint.attempts == 1);
	RadioRecovery::confirm(p);
	CHECK(!RadioRecovery::prepare(p));
}
TEST(recovery_validation, "UNIT-RECOVERY-003", "Corrupt backups and changed candidates cannot trigger an unrelated rollback") {
	auto p = fresh();
	RadioRecovery::confirm(p);
	p.freq = 868.1f;
	for (int i = 0; i < 3; ++i) RadioRecovery::prepare(p);
	p.freq = 869.525f;
	CHECK(!RadioRecovery::prepare(p));
	CHECK(p.radio_checkpoint.attempts == 1 && p.freq == 869.525f);
	p.radio_checkpoint.attempts = 255;
	CHECK(!RadioRecovery::prepare(p));
	CHECK(p.radio_checkpoint.attempts == 1);
	p.radio_checkpoint.checksum ^= 1;
	CHECK(!RadioRecovery::hasGood(p) && !RadioRecovery::restore(p));
}
TEST(recovery_without_backup, "UNIT-RECOVERY-004", "Startup failures without a confirmed backup saturate without inventing defaults") {
	auto p = fresh();
	for (int i = 0; i < 10; ++i) CHECK(!RadioRecovery::prepare(p));
	CHECK(p.radio_checkpoint.attempts == 3);
	CHECK(!RadioRecovery::restore(p));
}
