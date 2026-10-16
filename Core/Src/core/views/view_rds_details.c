#include <core/views/view_baserds.h>
#include <core/views/view_rds_details.h>
#include <core/util/textarea.h>
#include <core/util/display.h>
#include <core/util/rds.h>
#include <core/views.h>
#include <core/data.h>
#include <core/core.h>
#include <core/menus.h>
#include <stdio.h>
#include "stm32c0xx_hal.h"

#define PTY_COL 0
#define CLK_COL 0

view_info_t view_rds_details = (view_info_t) {
	.cb_open = view_rds_details_init,
	.cb_frame = view_rds_details_tick,
	.cb_close = 0
};

textarea_data_t ta_pty;
textarea_data_t ta_clk;

/*
 * Init function for RDS details
 */
void view_rds_details_init() {
	view_baserds_init(&view_rds_rt, &view_rds_loss);

	//Set up display
	disp_write_text_at(2, 0, "PTY:", -1);
	disp_write_text_at(3, 0, "CLK:", -1);

	//Initialize text areas
	textarea_init(&ta_pty, 2, PTY_COL + 5, 35, 1);
	textarea_init(&ta_clk, 3, CLK_COL + 5, 35, 1);
}

/*
 * Tick function for RDS details
 */
void view_rds_details_tick() {
	char buffer[40];

	//Get PTY
	const char* pty = "N/A";
	if (rds_state.pty >= 0)
		pty = pty_names[rds_state.pty];

	//If there is no PTYN, display as-is, otherwise do additional processing
	if (!(rds_state.status & RDS_STATUS_PTYN_READY)) {
		//Copy PTY directly from the flash
		textarea_copy_text(&ta_pty, pty);
	} else {
		//Copy PTY to buffer...
		int ptyLen = 0;
		while (pty[ptyLen] != 0) {
			buffer[ptyLen] = pty[ptyLen];
			ptyLen++;
		}

		//Add slash
		buffer[ptyLen++] = '/';

		//Copy the PTYN which adds a NULL terminator
		copy_display_rds_field(rds_state.ptyn, &buffer[ptyLen], 8);

		//Copy to buffer
		textarea_copy_text(&ta_pty, buffer);
	}

	//Create date/time
	buffer[0] = 0;
	if (rds_state.status & RDS_STATUS_CT_READY) {
		//Get current offset from recieved time, in seconds
		int offsetSecs = (int)((HAL_GetTick() - rds_state.ct_captured_at) / 1000);

		//Get local timestamp
		rds_datetime_t time = rds_convert_timestamp(rds_state.ct_timestamp + rds_state.ct_offset + (offsetSecs / 60));

		//Convert to 12-hour time
		const char* ampm = "AM";
		int hour = time.hour;
		if (hour == 0) {
			hour = 12;
		} else if (hour >= 12) {
			ampm = "PM";
			if (hour > 12)
				hour -= 12;
		}

		//Create the offset
		int absOffset = rds_state.ct_offset;
		if (absOffset < 0)
			absOffset = -absOffset;
		int offsetHours = absOffset / 60;
		int offsetMins = absOffset % 60;
		char offsetDir = rds_state.ct_offset >= 0 ? '+' : '-';

		//Pick indicator for old times
		char oldIndicator = ' ';
		if (offsetSecs > 61)
			oldIndicator = '*';

		//Format
		sprintf(buffer, "%i/%i/%i %i:%02i:%02i %s (UTC%c%i:%02i)%c", time.month, time.day, time.year, hour, time.minute, offsetSecs % 60, ampm, offsetDir, offsetHours, offsetMins, oldIndicator);
	}
	textarea_copy_text(&ta_clk, buffer);

	//Run everything else
	view_baserds_tick();
}
