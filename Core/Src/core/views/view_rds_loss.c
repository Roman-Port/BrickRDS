#include <core/views/view_baserds.h>
#include <core/views/view_rds_loss.h>
#include <core/util/textarea.h>
#include <core/util/display.h>
#include <core/util/charset.h>
#include <core/util/rds.h>
#include <core/views.h>
#include <core/data.h>
#include <core/core.h>
#include <core/menus.h>
#include <stdio.h>

view_info_t view_rds_loss = (view_info_t) {
	.cb_open = view_rds_loss_init,
	.cb_frame = view_rds_loss_tick,
	.cb_close = 0
};

static int last_rendered_sync;

static const char* sync_msgs[] = {
		"NO SYNC ",
		"SYNCING ",
		"* SYNC *"
};

static textarea_data_t ta_ber;

/*
 * Init function.
 */
void view_rds_loss_init() {
	view_baserds_init(&view_rds_details, &view_rds_rt);

	//Clear state
	last_rendered_sync = -1;

	//Clear bottom two lines of the display and flush since we are uploading icons
	disp_clear_line(2);
	disp_clear_line(3);
	disp_flush();

	//Write text
	disp_write_text_at(2, 35, "LOSS:", -1);
	disp_write_text_at(2, 10, "BER:[          ]-", -1);

	//Set up text areas
	textarea_init(&ta_ber, 2, 27, 6, 1);

	//Invalidate charset
	charset_mark_dirty(CHARSET_DISPLAY_LOWER);

	//Unpack lines into display ram; It's like this to conserve flash
	uint64_t packed = 743616712736;
	uint64_t extraMask = (packed >> (7 * 5)) << 56;
	uint64_t character;
	uint64_t line;
	for (int i = 0; i < 8; i++) {
		line = packed & 0b11111;
		packed >>= 5;
		character = line;
		for (int l = 0; l < 64; l += 8)
			character |= line << l;
		character |= extraMask;
		disp_upload_font(DISP_PIN_E2, i, character);
	}
}

/*
 * Tick function.
 */
void view_rds_loss_tick() {
	//Render sync message
	if (rds_sync != last_rendered_sync) {
		int index = rds_sync > 2 ? 2 : rds_sync;
		disp_write_text_at(2, 0, sync_msgs[index], -1);
		last_rendered_sync = rds_sync;
	}

	//Render loss
	disp_set_cursor(3, 0);
	for (int i = 39; i >= 0; i--)
		disp_write_char(rds_loss[i] & 0b111);

	//Calc BER and convert to screen format by counting bits
	int ber = 30 - rds_calc_ber_percent(30);
	int berGraphScreen = 0;
	for (int i = 0; i < ber; i++)
		berGraphScreen = (berGraphScreen << 1) | 1;

	//Render BER graph
	disp_set_cursor(2, 15);
	for (int i = 27; i >= 0; i -= 3)
		disp_write_char((berGraphScreen >> i) & 0b111);

	//Write BER text
	char errorBerStr[8];
	sprintf(errorBerStr, "%i%%", rds_calc_ber_percent(100));
	textarea_copy_text(&ta_ber, errorBerStr);

	//Process rest
	view_baserds_tick();
}
