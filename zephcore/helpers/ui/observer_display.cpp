/* SPDX-License-Identifier: MIT */
#include "observer_display.h"
#include "display.h"

void observer_display_draw(const ObserverDisplayStatus &status)
{
	const unsigned rows = mc_display_height() / mc_display_font_height();
	const unsigned visible = rows < 8 ? rows : 8;
	const unsigned pages = (8 + visible - 1) / visible;
	const unsigned page_seconds = mc_display_is_epd() ? 30 : 5;
	const unsigned first = (status.uptime_s / page_seconds % pages) * visible;
	char line[40];

	mc_display_on();
	mc_display_clear();
	for (unsigned i = 0; i < visible && first + i < 8; ++i) {
		observer_display_line(status, first + i, line, sizeof(line));
		mc_display_text(0, i * mc_display_font_height(), line, false);
	}
	mc_display_finalize();
}
