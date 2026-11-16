#pragma once

#include <mesh/Mesh.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/SignalQuality.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class AntennaStats {
	struct Samples {
		int64_t sum = 0;
		uint32_t count = 0;
		int16_t min = 0, max = 0;
		void add(int16_t value) {
			if (count == UINT32_MAX) return;
			if (!count || value < min) min = value;
			if (!count || value > max) max = value;
			sum += value;
			++count;
		}
	};
	struct Entry {
		uint8_t key[PUB_KEY_SIZE] = {};
		Samples rssi, snr, remote_snr;
		uint32_t heard = 0, probe_tag = 0;
	};
	Entry _entries[8] = {};
	unsigned _count = 0, _next = 0;
	uint32_t _tag = 0, _started = 0;
	bool _probing = false;

	Entry& entry(const uint8_t *key) {
		for (unsigned i = 0; i < _count; ++i) {
			if (memcmp(_entries[i].key, key, PUB_KEY_SIZE) == 0) return _entries[i];
		}
		auto& e = _entries[_next];
		e = Entry{};
		memcpy(e.key, key, PUB_KEY_SIZE);
		_next = (_next + 1) % 8;
		if (_count < 8) ++_count;
		return e;
	}

public:
	void reset() { *this = AntennaStats{}; }

	void advert(const mesh::Packet& packet, const uint8_t *key, int rssi, uint32_t now) {
		if (packet.getPathHashCount() != 0 ||
		    (packet.hasTransportCodes() && packet.transport_codes[0] == 0 && packet.transport_codes[1] == 0)) return;
		auto& e = entry(key);
		e.rssi.add(rssi);
		e.snr.add(packet._snr);
		e.heard = now;
	}

	void startProbe(uint32_t tag, uint32_t now) {
		_tag = tag;
		_started = now;
		_probing = true;
	}

	bool probe(mesh::Mesh& mesh, uint32_t now) {
		uint8_t data[10] = { 0x80, 1 << ADV_TYPE_REPEATER };
		uint32_t tag = mesh.getRNG()->nextInt(1, INT32_MAX);
		memcpy(data + 2, &tag, 4);
		auto *packet = mesh.createControlData(data, sizeof(data));
		if (!packet) return false;
		startProbe(tag, now);
		mesh.sendZeroHop(packet);
		return true;
	}

	void discovery(const mesh::Packet& packet, int rssi, uint32_t now) {
		if (!_probing || uint32_t(now - _started) >= 60000 ||
		    !packet.isRouteDirect() || packet.getPathHashCount() != 0 ||
		    packet.payload_len != 6 + PUB_KEY_SIZE || packet.payload[0] != (0x90 | ADV_TYPE_REPEATER)) return;
		uint32_t tag;
		memcpy(&tag, packet.payload + 2, 4);
		if (tag != _tag) return;
		auto& e = entry(packet.payload + 6);
		if (e.remote_snr.count && e.probe_tag == tag) return;
		e.probe_tag = tag;
		e.rssi.add(rssi);
		e.snr.add(packet._snr);
		e.remote_snr.add(static_cast<int8_t>(packet.payload[1]));
		e.heard = now;
	}

	void format(unsigned index, char *reply, size_t size, uint32_t now) const {
		if (index >= _count) {
			snprintf(reply, size, "No neighbour at index %u (%u stored)", index, _count);
			return;
		}
		const auto& e = _entries[index];
		char key[PUB_KEY_SIZE * 2 + 1];
		const bool remote = size <= 161;
		const unsigned key_bytes = remote ? 4 : PUB_KEY_SIZE;
		for (unsigned i = 0; i < key_bytes; ++i) snprintf(key + i * 2, 3, "%02X", e.key[i]);
		char age[32] = {};
		if (!remote) snprintf(age, sizeof(age), " age=%lus", (unsigned long)(uint32_t(now - e.heard) / 1000));
		const double rssi_mean = double(e.rssi.sum) / e.rssi.count;
		const double snr_mean = double(e.snr.sum) / e.snr.count / 4;
		int used = snprintf(reply, size,
			"%u %s%s RX=%lu RSSI=%d/%.2f/%d(%s) SNR=%.2f/%.2f/%.2f(%s) TX=%lu",
			index, key, age,
			(unsigned long)e.rssi.count, e.rssi.min, rssi_mean, e.rssi.max, rssiQuality(rssi_mean),
			e.snr.min / 4.0, snr_mean, e.snr.max / 4.0, snrQuality(snr_mean),
			(unsigned long)e.remote_snr.count);
		if (used < 0 || size_t(used) >= size) return;
		if (e.remote_snr.count) {
			const double remote_mean = double(e.remote_snr.sum) / e.remote_snr.count / 4;
			snprintf(reply + used, size - used, " SNR=%.2f/%.2f/%.2f(%s)",
				e.remote_snr.min / 4.0, remote_mean, e.remote_snr.max / 4.0, snrQuality(remote_mean));
		}
	}

	bool command(const char *command, char *reply, size_t size, uint32_t now) {
		if (strcmp(command, "antenna") == 0) {
			snprintf(reply, size, "%u/8 neighbours; antenna <0..7> | probe | reset; min/mean/max", _count);
		} else if (strcmp(command, "antenna reset") == 0) {
			reset();
			snprintf(reply, size, "OK - antenna stats reset");
		} else if (strncmp(command, "antenna ", 8) == 0) {
			char *end;
			unsigned long index = strtoul(command + 8, &end, 10);
			if (end == command + 8 || *end || index >= 8) {
				snprintf(reply, size, "Usage: antenna <0..7> | probe | reset");
			} else {
				format(index, reply, size, now);
			}
		} else return false;
		return true;
	}
};
