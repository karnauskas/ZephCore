#include "test.h"
#include <ForwardFilter.h>
#include <NodePrefs.h>
#include <cstring>

extern int mac_test_calls;
extern bool mac_test_fail;

TEST(forward_filter_bounds, "UNIT-FILTER-001", "Forwarding filters cover payload routes path sizes and zero-hop boundaries") {
    NodePrefs prefs; initNodePrefs(&prefs);
    CHECK(!prefs.forward_filters.one_byte && prefs.forward_filters.data && !prefs.forward_filters.public_channel);
    mesh::Packet p{};
    int calls = mac_test_calls;
    for (int flags = 0; flags < 4; ++flags)
    for (int route = 0; route < 4; ++route)
    for (int type = 0; type < 16; ++type)
    for (int size = 1; size <= 3; ++size)
    for (int hops = 0; hops <= 2; ++hops) {
        ForwardFilters f{uint8_t(flags & 1), uint8_t((flags >> 1) & 1), 0};
        p.header = route | (type << 2); p.setPathHashSizeAndCount(size, hops);
        bool drop = (f.one_byte && p.isRouteFlood() && size == 1 && hops > 0) ||
            (f.data && (type == 6 || type >= 12));
        CHECK(allowFilteredForward(&p, f) == !drop);
    }
    CHECK(mac_test_calls == calls);
}

TEST(forward_filter_public, "UNIT-FILTER-002", "Public filtering verifies MACs without decrypting and preserves hash collisions") {
    ForwardFilters f{0,0,1}; mesh::Packet p{};
    p.payload[0] = 0x11;
    for (int type : {PAYLOAD_TYPE_GRP_TXT, PAYLOAD_TYPE_GRP_DATA})
    for (int route = 0; route < 4; ++route)
    for (int len : {16,32}) {
        p.header = route | (type << 2); p.payload_len = len + 3;
        p.payload[1] = len == 16 ? 0x47 : 0x50;
        p.payload[2] = len == 16 ? 0x0c : 0x30;
        for (int i = 0; i < len; ++i) p.payload[i + 3] = i;
        CHECK(!allowFilteredForward(&p, f));
        p.payload[1] ^= 1; CHECK(allowFilteredForward(&p, f)); p.payload[1] ^= 1;
        mac_test_fail = true; CHECK(allowFilteredForward(&p, f)); mac_test_fail = false;
        int calls = mac_test_calls;
        p.payload[0] = 0x12; CHECK(allowFilteredForward(&p, f)); p.payload[0] = 0x11;
        CHECK(mac_test_calls == calls);
        CHECK(allowFilteredForward(&p, ForwardFilters{}));
    }
    p.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << 2);
    p.payload_len = 19;
    // A different channel with SHA256(key)[0] == 0x11, using the same ciphertext.
    // key 0b000000000000000000000000000000, Python HMAC-SHA256 tag 4b8b.
    p.payload[1] = 0x4b; p.payload[2] = 0x8b;
    CHECK(allowFilteredForward(&p, f));
    int calls = mac_test_calls;
    for (int len = 0; len < 19; ++len) { p.payload_len = len; CHECK(allowFilteredForward(&p, f)); }
    p.payload_len = 20; CHECK(allowFilteredForward(&p, f));
    CHECK(mac_test_calls == calls);
    p.payload_len = 19; p.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_TXT_MSG << 2);
    CHECK(allowFilteredForward(&p, f)); CHECK(mac_test_calls == calls);
}

TEST(forward_filter_cli, "UNIT-FILTER-003", "Filter CLI responses persistence failures and invalid arguments preserve settings") {
    ForwardFilters f{0,1,0}; char reply[96]; int saves = 0; bool persist = true;
    auto run = [&](const char *cmd) {
        return handleForwardFilterCommand(f, cmd, reply, sizeof(reply), [&]() { ++saves; return persist; });
    };
    CHECK(run("filter")); CHECK(!strcmp(reply, "> 1byte off, data on, public off"));
    CHECK(run("filter data")); CHECK(!strcmp(reply, "> filter data on")); CHECK(saves == 0);
    CHECK(run("filter data off")); CHECK(!strcmp(reply, "OK - filter data off")); CHECK(!f.data && saves == 1);
    CHECK(run("filter data off")); CHECK(saves == 1);
    CHECK(run("filter 1byte on")); CHECK(f.one_byte && saves == 2);
    persist = false;
    CHECK(run("filter public on")); CHECK(!f.public_channel && saves == 3);
    CHECK(!strcmp(reply, "Error - could not save filter settings"));
    for (const char *cmd : {"filter data 1", "filter data on extra", "filter 1byte of", "filter public onward"}) {
        CHECK(run(cmd)); CHECK(!strcmp(reply, "Error, must be: on or off"));
    }
    CHECK(saves == 3 && f.one_byte && !f.data && !f.public_channel);
    CHECK(run("filter datax on")); CHECK(!strcmp(reply, "Error - unknown filter"));
    CHECK(!run("filtering")); CHECK(!run("get filter"));
    persist = true; CHECK(run("filter   public  on   ")); CHECK(f.public_channel);
    CHECK(run("filter 1byte OFF\t\r\n")); CHECK(!f.one_byte);
    CHECK(run("filter data On\t")); CHECK(f.data);
    char tiny[4]; CHECK(handleForwardFilterCommand(f, "filter", tiny, sizeof(tiny), []() { return true; }));
    CHECK(tiny[3] == 0);
}
