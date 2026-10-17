#include <core/views/view_baserds.h>
#include <core/util/display.h>
#include <core/util/textarea.h>
#include <core/util/charset.h>
#include <core/util/pidb.h>
#include <core/util/rds.h>
#include <core/util/settings.h>
#include <core/menus.h>
#include <core/core.h>
#include <stdio.h>

static textarea_data_t ta_pi;

static view_info_t* next_view;
static view_info_t* prev_view;

static const char* suffix_blank = "";
static const char* suffix_lp = "-LP";
static const char* suffix_fm = "-FM";

/*
 * Updates the PI area
 */
static void update_pi(uint16_t pi) {
	//Create text buffer
	char buffer[80];

	//Look up in flash
	if (pidb_scan(pi)) {
		//Determine suffix; Leave blank if long call
		const char* suffix = suffix_blank;
		if ((pidb_last_entry.flags & PIDB_FLAG_LONG_CALL) == 0) {
			//Add -FM by default otherwise -LP
			if (pidb_last_entry.flags & PIDB_FLAG_LP_SUFFIX)
				suffix = suffix_lp;
			else
				suffix = suffix_fm;
		}

		//Format
		int stateNameIndex = pidb_last_entry.state * 2;
		sprintf(buffer, "%s%s %s, %c%c", pidb_last_entry.callsign, suffix, pidb_last_entry.col, pidb_state_names_short[stateNameIndex], pidb_state_names_short[stateNameIndex + 1]);
	} else {
		//No match; Use straight PI code
		sprintf(buffer, "%02X%02X", pi >> 8, pi & 0xFF);
	}

	//Update textarea
	textarea_copy_text(&ta_pi, buffer);
}

static void update_view() {
	//Update the PI
	update_pi(rds_state.pi);

	//Update the PS, only copying legal characters
	char ps[9];
	copy_display_rds_field(user_settings.use_partial ? rds_state.ps_partial : rds_state.ps, ps, 8);
	disp_write_text_at(0, 4, ps, 8);
}

static void render_sig_meter() {
	//Calculate BER between 0-100%
	int ber = rds_calc_ber_percent(100);

	//Scale BER to 0-3 bars
	int bars = 0;
	if (ber <= 10)
		bars = 3;
	else if (ber <= 30)
		bars = 2;
	else if (ber <= 60)
		bars = 1;

	//Render signal meter
	disp_set_cursor(0, 1);

	//First bar
	if (bars == 0)
		disp_write_char(4); // none
	else if (bars == 1)
		disp_write_char(3); // one bar
	else
		disp_write_char(1); // two

	//Second bars
	if (bars == 3)
		disp_write_char(2); // up
	else
		disp_write_char(5); // down
}

/*
 * Init function for baserds, which provides the upper portion scrolling PS/callsign
 */
void view_baserds_init(view_info_t* prevView, view_info_t* nextView) {
	//Store next/previous menus
	next_view = nextView;
	prev_view = prevView;

	//Upload charset
	charset_change(CHARSET_MAIN_UPPER, CHARSET_MAIN_LOWER);

	//Clear display
	disp_clear();

	//Initialize textareas o
	textarea_init(&ta_pi, 0, 13, 40 - 13, 1);
	ta_pi.flags |= TEXTAREA_FLAG_ALIGN_RIGHT;

	//Render signal meter
	disp_set_cursor(0, 0);
	disp_write_char(0x00);
	render_sig_meter();

	//Render line below top
	disp_set_cursor(1, 0);
	for (int i = 0; i < 40; i++)
		disp_write_char(0x06);

	//Start RDS
	rds_enable();

	//Update now
	update_view();
}

/*
 * Tick function for baserds
 */
void view_baserds_tick() {
	//Update signal meter
	render_sig_meter();

	//Process menu button
	if (core_get_button(BTN_ENTER))
		menus_show_mainmenu();
	else if (core_get_button(BTN_LEFT))
		core_change_view(prev_view);
	else if (core_get_button(BTN_RIGHT))
		core_change_view(next_view);
	else {
		//Update without handling inputs
		update_view();

		//Reset upon pressing clear
		if (core_get_button(BTN_BACK))
			rds_reset();
	}
}
