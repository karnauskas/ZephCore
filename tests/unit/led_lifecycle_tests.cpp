#include "test.h"
#include "helpers/ui/ui_common.c"

static bool disabled;
bool zephcore_leds_disabled(void) { return disabled; }
void zephcore_leds_set_disabled(bool value) { disabled = value; zephcore_leds_ui_sync(value); }
bool zephcore_led_radio_holds_pin(void) { return false; }
uint8_t zephcore_leds_hb_mode(void) { return LEDS_HB_ALL; }
void ui_set_battery(uint16_t, uint8_t) {}

TEST(led_lifecycle, "UNIT-LED-001", "Boot LED preferences wait for heartbeat initialization") {
	zephcore_leds_set_disabled(false);
	zephcore_leds_set_disabled(true);
	zephcore_leds_set_disabled(false);
	CHECK(!s_led_on_work.pending && !s_led_off_work.pending);
	ui_led_heartbeat_init();
	CHECK(s_led_on_work.pending);
	zephcore_leds_set_disabled(true);
	CHECK(!s_led_on_work.pending && !s_led_off_work.pending);
	zephcore_leds_set_disabled(false);
	CHECK(s_led_on_work.pending);
	s_led_on_work.pending = false;
	s_led_on_work.handler(nullptr);
	CHECK(s_led_off_work.pending);
	s_led_off_work.pending = false;
	s_led_off_work.handler(nullptr);
	CHECK(s_led_on_work.pending);
}
