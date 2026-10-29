#pragma once
#include <stdint.h>
#include <zephyr/device.h>
int lora_rssi(const struct device *dev, int16_t *rssi);
