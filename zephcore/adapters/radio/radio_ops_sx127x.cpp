/*
 * SPDX-License-Identifier: MIT
 * SX127x family over Zephyr's loramac-node backend, for LoRaRadio.
 *
 * Instantaneous RSSI uses the Zephyr LoRa API. RX-busy gating, RX boost,
 * CAD and duty cycle remain unavailable through this adapter.
 */

#include "LoRaRadioOps.h"
#include <zephyr/drivers/lora.h>

namespace mesh {

static int16_t sx127x_get_rssi_inst(const struct device *dev)
{
	int16_t rssi;
	/* The shared burst sampler abandons a read returning -128. */
	return lora_rssi(dev, &rssi) == 0 ? rssi : -128;
}

const LoRaRadioOps kLoRaRadioOps = {
	.loramac_node = true,
	.rx_duty_cycle = false,
	.rssi_inst = sx127x_get_rssi_inst,
};

} /* namespace mesh */
