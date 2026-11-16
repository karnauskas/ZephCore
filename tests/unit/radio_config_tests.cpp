#include "test.h"
#include <cerrno>

namespace {
struct lora_modem_config {
	int frequency = 869618000, bandwidth = 62, datarate = 8, coding_rate = 8;
	int preamble_len = 32, tx_power = 22;
	bool tx = false, iq_inverted = false, public_network = false;
	struct { int mode = 1; } cad;
};
struct device {
	bool receiving = true;
	int configs = 0, cancels = 0;
	int error = 0;
};
bool device_is_ready(const device*) { return true; }
int lora_config(const device *dev, const lora_modem_config*) {
	auto *driver = const_cast<device*>(dev);
	++driver->configs;
	return driver->receiving ? -EBUSY : driver->error;
}
int atomic_get(const int *value) { return *value; }
void atomic_set(int *value, int next) { *value = next; }
#define LOG_DBG(...) do {} while (0)
#define LOG_ERR(...) do {} while (0)
struct LoRaRadio {
	struct { bool loramac_node = false; } _ops;
	device *_dev;
	lora_modem_config desired, _last_cfg;
	bool _config_cached = true;
	int _in_recv_mode = 1, _last_hw_error = 0;
	explicit LoRaRadio(device& driver) : _dev(&driver) {}
	void buildModemConfig(lora_modem_config& cfg, bool tx) { cfg = desired; cfg.tx = tx; }
	void hwCancelReceive() { ++_dev->cancels; _dev->receiving = false; }
	void configure(bool tx);
	void configureRx();
	void configureTx();
	bool hwConfigure(const lora_modem_config& cfg);
};
#include "radio_config.inc"
#undef LOG_DBG
#undef LOG_ERR
}

TEST(radio_power_change, "UNIT-RADIOCFG-001", "Changing TX power cancels RX before configuring and preserves direction fast paths") {
	device driver;
	LoRaRadio radio(driver);
	radio.desired.tx_power = 20;
	radio.configureTx();
	CHECK(driver.cancels == 1 && driver.configs == 1);
	CHECK(radio._in_recv_mode == 0 && radio._config_cached && radio._last_hw_error == 0);
	CHECK(radio._last_cfg.tx && radio._last_cfg.tx_power == 20);
	radio.configureRx();
	CHECK(driver.cancels == 1 && driver.configs == 1);
	CHECK(!radio._last_cfg.tx);
	driver.receiving = true;
	radio._in_recv_mode = 1;
	radio.configureRx();
	CHECK(driver.receiving && driver.cancels == 1 && driver.configs == 1);
	radio.configureTx();
	CHECK(driver.receiving && driver.cancels == 1 && driver.configs == 1);
}

TEST(radio_config_full, "UNIT-RADIOCFG-002", "Full RX configuration and loramac direction changes obey the driver state contract") {
	for (bool loramac : { false, true }) {
		device driver;
		LoRaRadio radio(driver);
		radio._ops.loramac_node = loramac;
		radio.desired.frequency = 868000000;
		radio.configureRx();
		CHECK(driver.cancels == 1 && driver.configs == 1 && radio._config_cached);
		driver.receiving = true;
		radio._in_recv_mode = 1;
		radio.configureTx();
		CHECK(driver.cancels == (loramac ? 2 : 1));
		CHECK(driver.configs == (loramac ? 2 : 1));
	}
	device driver;
	driver.receiving = false;
	LoRaRadio radio(driver);
	radio._in_recv_mode = 0;
	radio._config_cached = false;
	radio.configureRx();
	CHECK(driver.cancels == 0 && driver.configs == 1 && radio._config_cached);
	driver.receiving = true;
	driver.error = -EIO;
	radio._in_recv_mode = 1;
	radio.desired.tx_power = -9;
	radio.configureTx();
	CHECK(driver.cancels == 1 && driver.configs == 2);
	CHECK(!radio._config_cached && radio._in_recv_mode == 0 && radio._last_hw_error == -EIO);
}
