#include "test.h"
#include <RxPowerSaving.h>
#include <GpsPowerSchedule.h>
#include <NodePrefs.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

TEST(rxps_timing, "UNIT-RXPS-001", "RX profiles and manual windows preserve catch completion wake and rounding bounds") {
    for (unsigned sf = 5; sf <= 12; ++sf)
    for (unsigned bw : {62500U,125000U,250000U,500000U})
    for (unsigned wake : {1500U,6000U})
    for (unsigned level = 0; level <= 8; ++level) {
        RxPowerSavingPrefs p{}; p.level = level;
        unsigned pre = sf <= 8 ? 32 : 16, detect = sf < 7 ? 12 : 8;
        RxPowerSavingTiming t;
        if (!rxPowerSavingTiming(p,sf,bw,pre,detect,wake,10,t)) { CHECK(!t.rx_us && !t.sleep_us); continue; }
        unsigned sym = (1U << sf) * 1000000ULL / bw;
        unsigned budget = (pre - 2 * detect - 1) * sym;
        CHECK(t.sleep_us + wake <= budget - budget / 10);
        CHECK(t.rx_us >= (detect + 1) * sym);
        CHECK(2ULL * t.rx_us + t.sleep_us >= (pre + 14) * sym);
        CHECK(t.rx_us % 1000 == 0 && t.sleep_us % 1000 == 0);
    }
    RxPowerSavingTiming t; RxPowerSavingPrefs manual{100000,20000,0,0};
    CHECK(rxPowerSavingTiming(manual,8,62500,32,8,1500,10,t));
    CHECK(t.rx_us == 101000 && t.sleep_us == 20000);
    manual.sleep_us = 100000; CHECK(!rxPowerSavingTiming(manual,8,62500,32,8,1500,10,t));
    manual = {}; manual.preamble = 16;
    CHECK(!rxPowerSavingTiming(manual,8,62500,32,8,1500,10,t));
    manual.level = 9; CHECK(!rxPowerSavingTiming(manual,8,62500,32,8,1500,10,t));
    CHECK(!rxPowerSavingTiming({},8,0,32,8,1500,10,t));
}

TEST(rxps_cli, "UNIT-RXPS-002", "RXPS command parser matches reviewed profiles preserves errors and rejects experimental levels") {
    uint8_t on = 0; RxPowerSavingPrefs p{}; char reply[160];
    auto run = [&](const char *cmd) { return rxPowerSavingCommand(cmd,on,p,reply,sizeof(reply)); };
    CHECK(run("set radio.rxps conservative") == 2 && on && p.level == 3 && !p.preamble);
    CHECK(run("set radio.rxps balanced") == 2 && p.level == 6);
    CHECK(run("set radio.rxps max") == 2 && p.level == 8);
    CHECK(run("set radio.rxps 4") == 2 && p.level == 4 && !p.preamble);
    CHECK(run("set radio.rxps level 8 preamble 32") == 2 && p.preamble == 32);
    CHECK(run("set radio.rxps 80000 20000") == 2 && !p.level && p.rx_us == 80000 && p.sleep_us == 20000);
    const auto before = p;
    for (const char *cmd : {"set radio.rxps 0", "set radio.rxps 9", "set radio.rxps 10", "set radio.rxps -1",
        "set radio.rxps level 4 preamble 33", "set radio.rxps 999 2000", "set radio.rxps 1000 30000001",
        "set radio.rxps 1000 2000 extra", "set radio.rxps 9999999999999999999999999999"}) {
        CHECK(run(cmd) == 1 && on && !memcmp(&p,&before,sizeof(p)));
    }
    CHECK(run("set radio.rxps off") == 2 && !on && p.rx_us == before.rx_us);
    CHECK(run("get radio.rxps") == 1); CHECK(run("get radio.rxpsx") == 0);
}

TEST(gps_acquire_schedule, "UNIT-RXPS-003", "GPS acquisition remains bounded for server cold warm and first-window phases") {
    for (bool server : {false,true}) for (bool cold : {false,true})
    for (bool first : {false,true}) for (bool done : {false,true}) {
        CHECK(gpsAcquireWindowMs(server,cold,first,done,300000,120000) ==
            (server || cold || (!first && !done) ? 300000U : 120000U));
    }
    CHECK(gpsAcquireWindowMs(true,true,false,false,1800000,120000) == 300000);
}

namespace {
struct lora_modem_config { int datarate = 8, bandwidth = 62500, preamble_len = 32; };
struct device { int arms = 0, continuous = 0, cancels = 0, error = 0; bool packet = false; };
int atomic_get(const int *p) { return *p; }
void atomic_set(int *p, int v) { *p = v; }
int k_cycle_get_32() { return 1; }
int k_us_to_ticks_floor64(unsigned us) { return us / 100; }
#define K_TICKS(t) (t)
#define CONFIG_ZEPHCORE_LORA_DC_MARGIN_PCT 10
#define LOG_ERR(...) do {} while (0)
unsigned bandwidth_to_hz(int bw) { return bw; }
unsigned rxDutyDetectSymbols(int sf) { return sf < 7 ? 12 : 8; }
int lora_recv_duty_cycle_async(device *d,int,int,void*,void*) { ++d->arms; return d->error; }
int lora_recv_async(device *d,void*,void*) { ++d->continuous; return 0; }
struct LoRaRadio {
    device *_dev; NodePrefs *_prefs;
    struct { bool rx_duty_cycle = true; } _ops;
    bool _config_cached = true, _rx_duty_cycle_enabled = false, _rxps_armed = false;
    int _rxps_error = 0, _in_recv_mode = 1, _tx_active = 0, _rx_entry_cyc = 0, _last_hw_error = 0;
    unsigned _rxps_failures = 0, _dc_last_rx_us = 0, _dc_last_sleep_us = 0;
    static inline void *rxCallbackStatic = nullptr;
    void configureRx() {}
    void buildModemConfig(lora_modem_config &cfg,bool) { cfg = {}; }
    unsigned hwWakeupTimeUs() { return 1500; }
    bool hwIsReceiving() { return _dev->packet; }
    void hwCancelReceive() { ++_dev->cancels; }
    void startReceive(); void enableRxDutyCycle(bool);
    void applyRxPowerSaving(); void formatRxPowerSaving(char*,size_t) const;
};
#include "rxps_adapter.inc"
#undef K_TICKS
#undef LOG_ERR
}

TEST(rxps_adapter, "UNIT-RXPS-004", "Production RX adapter reports arm failures unsupported radios and defers active packet changes") {
    NodePrefs p; initNodePrefs(&p); device d; LoRaRadio radio{&d,&p}; char reply[160];
    p.rx_duty_cycle = 1; radio.applyRxPowerSaving();
    CHECK(d.arms == 1 && radio._rxps_armed && !d.continuous);
    radio.formatRxPowerSaving(reply,sizeof(reply)); CHECK(strstr(reply,"desired=on,effective=armed,supported=yes"));
    radio._tx_active = 1; radio.formatRxPowerSaving(reply,sizeof(reply));
    CHECK(strstr(reply,"effective=continuous")); radio._tx_active = 0;
    d.packet = true; p.rx_duty_cycle = 0; radio.applyRxPowerSaving();
    CHECK(d.cancels == 1 && radio._rxps_armed);
    d.packet = false; radio.applyRxPowerSaving(); CHECK(!radio._rxps_armed && d.continuous == 1);
    p.rx_duty_cycle = 1; d.error = -EIO; radio.applyRxPowerSaving();
    CHECK(!radio._rxps_armed && radio._rxps_error == -EIO && radio._rxps_failures == 1 && d.continuous == 2);
    radio._ops.rx_duty_cycle = false; radio.applyRxPowerSaving();
    CHECK(radio._rxps_error == -ENOTSUP && !radio._rxps_armed && d.arms == 2);
    radio._ops.rx_duty_cycle = true; p.rxps.preamble = 16; radio.applyRxPowerSaving();
    CHECK(radio._rxps_error == -ERANGE && !radio._rxps_armed);
    radio._tx_active = 1; unsigned cancels = d.cancels; radio.applyRxPowerSaving(); CHECK(d.cancels == int(cancels));
}
