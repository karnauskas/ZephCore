#include "test.h"
extern "C" int test_websocket_native_poll(void);
extern "C" int test_websocket_offload_poll(void);
TEST(websocket_native_poll, "UNIT-MQTT-006", "Native WebSocket poll exposes buffered frames and preserves errors") {
	CHECK(test_websocket_native_poll());
}
TEST(websocket_offload_poll, "UNIT-MQTT-007", "Offloaded WebSocket poll counts buffered readiness and restores descriptors") {
	CHECK(test_websocket_offload_poll());
}
