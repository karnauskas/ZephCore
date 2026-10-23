#pragma once
#include <zephyr/kernel.h>
extern struct k_event g_wifi_events;
#define WIFI_READY_BIT BIT(0)
bool zc_wifi_station_is_connected(void);
void zc_wifi_station_session_up(bool up);
