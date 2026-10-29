#include "test.h"
#include "../../zephcore/adapters/radio/radio_ops_sx127x.cpp"

namespace {
int result;
int16_t reading;
const struct device *queried;
}

int lora_rssi(const struct device *dev, int16_t *rssi)
{
	queried = dev;
	if (result == 0) *rssi = reading;
	return result;
}

TEST(sx127x_rssi, "UNIT-SX127X-001", "SX127x adapter reads instantaneous RSSI and rejects failed queries") {
	device radio{1};
	CHECK(mesh::kLoRaRadioOps.loramac_node);
	CHECK(mesh::kLoRaRadioOps.rssi_inst != nullptr);
	result = 0;
	reading = -117;
	CHECK(mesh::kLoRaRadioOps.rssi_inst(&radio) == -117);
	CHECK(queried == &radio);
	reading = -103;
	CHECK(mesh::kLoRaRadioOps.rssi_inst(&radio) == -103);
	result = -16;
	CHECK(mesh::kLoRaRadioOps.rssi_inst(&radio) == -128);
	result = -38;
	CHECK(mesh::kLoRaRadioOps.rssi_inst(&radio) == -128);
	result = 0;
	reading = -121;
	CHECK(mesh::kLoRaRadioOps.rssi_inst(&radio) == -121);
}
