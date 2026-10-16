/* Utility for testing the RDS IC */

#include <core/views/utility/view_stats.h>
#include <core/util/display.h>
#include <core/util/rds.h>
#include <core/util/menu.h>
#include <core/core.h>
#include <core/views.h>
#include <stdint.h>
#include <stdio.h>
#include <main.h>

view_info_t view_utility_stats = (view_info_t) {
	.cb_open = bootutils_utility_stats_open,
	.cb_frame = bootutils_utility_stats_tick,
	.cb_close = 0
};

void bootutils_utility_stats_open() {
	//Set up display
	disp_clear();
	disp_write_text_at(0, 0, "RDS STATS", -1);

	//Enable RDS
	rds_enable();
}

void bootutils_utility_stats_tick() {
	//Look for back
	if (core_get_button(BTN_BACK)) {
		menu_reshow();
		return;
	}

	//Create text buffer
	char buffer[40];

	//Create layer 1 info
	int speed = (rds_received_symbols * 1000) / (HAL_GetTick() - rds_start_time);
	sprintf(buffer, "L1: DROP=%i SYM=%i SP=%i CERR=%i", rds_dropped_symbols, rds_received_symbols, speed, rds_clk_errors);
	disp_write_text_at(1, 0, buffer, -1);
	sprintf(buffer, "    BTOTAL=%i BERR=%i", rds_total_bits, rds_total_bits_bad);
	disp_write_text_at(2, 0, buffer, -1);

	//Create string from partial PS
	char ps[9];
	copy_display_rds_field(rds_state.ps_partial, ps, 8);

	//Create layer 2 info
	sprintf(buffer, "L2: SYNC=%i PI=%X PTY=%i PS=%s ", rds_sync, rds_state.pi, rds_state.pty, ps);
	disp_write_text_at(3, 0, buffer, -1);
}
