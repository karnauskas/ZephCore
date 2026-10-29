#include "test.h"
#include <cstddef>

static int modem_seen;
static int16_t SX1272ReadRssi(int modem) { modem_seen = modem; return -117; }
static int16_t SX1276ReadRssi(int modem) { modem_seen = modem; return -121; }
#include "sx127x_table.inc"

TEST(sx127x_radio_table, "UNIT-SX127X-002", "SX1272 and SX1276 Radio tables dispatch RSSI to the matching library callback") {
	CHECK(sx1272::Radio.Rssi != nullptr);
	CHECK(sx1276::Radio.Rssi != nullptr);
	CHECK(sx1272::Radio.Rssi(1) == -117 && modem_seen == 1);
	CHECK(sx1276::Radio.Rssi(0) == -121 && modem_seen == 0);
}
