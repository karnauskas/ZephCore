/* SPDX-License-Identifier: MIT */
#pragma once

#include <mesh/Packet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct ForwardFilters {
	uint8_t one_byte;
	uint8_t data;
	uint8_t public_channel;
};

bool allowFilteredForward(const mesh::Packet *packet, const ForwardFilters &filters);

template<class Save>
bool handleForwardFilterCommand(ForwardFilters &filters, const char *command,
	char *reply, size_t size, Save save)
{
	if (strncmp(command, "filter", 6) || (command[6] && command[6] != ' ')) return false;
	const char *name = command + 6;
	while (*name == ' ') ++name;
	if (!*name) {
		snprintf(reply, size, "> 1byte %s, data %s, public %s", filters.one_byte ? "on" : "off",
			filters.data ? "on" : "off", filters.public_channel ? "on" : "off");
		return true;
	}
	const char *names[] = { "1byte", "data", "public" };
	uint8_t *settings[] = { &filters.one_byte, &filters.data, &filters.public_channel };
	for (unsigned i = 0; i < 3; ++i) {
		size_t len = strlen(names[i]);
		if (strncmp(name, names[i], len) || (name[len] && name[len] != ' ')) continue;
		const char *arg = name + len;
		while (*arg == ' ') ++arg;
		if (!*arg) {
			snprintf(reply, size, "> filter %s %s", names[i], *settings[i] ? "on" : "off");
			return true;
		}
		size_t n = strlen(arg);
		while (n && (arg[n - 1] == ' ' || arg[n - 1] == '\t' || arg[n - 1] == '\r' || arg[n - 1] == '\n')) --n;
		char word[4] = {};
		if (n < sizeof(word)) for (size_t j = 0; j < n; ++j)
			word[j] = arg[j] >= 'A' && arg[j] <= 'Z' ? arg[j] + ('a' - 'A') : arg[j];
		bool on = !strcmp(word, "on");
		if (!on && strcmp(word, "off")) {
			snprintf(reply, size, "Error, must be: on or off");
			return true;
		}
		uint8_t previous = *settings[i];
		*settings[i] = on;
		if (previous != on && !save()) {
			*settings[i] = previous;
			snprintf(reply, size, "Error - could not save filter settings");
		} else snprintf(reply, size, "OK - filter %s %s", names[i], on ? "on" : "off");
		return true;
	}
	snprintf(reply, size, "Error - unknown filter");
	return true;
}
