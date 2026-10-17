#include <core/views/settings/view_about.h>
#include <core/util/display.h>
#include <core/util/menu.h>
#include <core/util/pidb.h>
#include <core/core.h>
#include <core/views.h>
#include <core/defines.h>
#include <stdio.h>

view_info_t view_settings_about = (view_info_t) {
	.cb_open = view_settings_about_init,
	.cb_frame = view_settings_about_tick,
	.cb_close = 0
};

static int secret_counter = 0;

/*
 * Init function
 */
void view_settings_about_init() {
	//Clear state
	secret_counter = 0;

	//Set up view
	disp_clear();
	disp_write_text_at(0, 0, PRODUCT_NAME " by " PRODUCT_AUTHOR " (version " PRODUCT_VERSION_MAJOR "." PRODUCT_VERSION_MINOR ")", -1);
	disp_write_text_at(2, 0, "Contains GPL-3 licensed code from SDR++", -1);

	//Read out info from the PIDB
	if (pidb_status == PIDB_STATUS_READY) {
		//Read info
		char nameBuf[17];
		uint8_t piDbFileVer;
		pidb_copy_info(nameBuf, &piDbFileVer);

		//Write to buffer
		char buffer[40];
		sprintf(buffer, "PIDB built %s (ver %i)", nameBuf, (int)piDbFileVer);
		disp_write_text_at(1, 0, buffer, -1);
	} else {
		//Write error
		disp_write_text_at(1, 0, "PIDB not loaded.", -1);
	}
}

/*
 * Tick function
 */
void view_settings_about_tick() {
	//Handle easter egg by hitting menu 3 times
	if (core_get_button(BTN_ENTER))
		secret_counter++;

	//Show secret
	if (secret_counter == 3)
		disp_write_text_at(3, 0, "\"Roman D. Strode radio project\"", -1);

	//Handle back
	if (core_get_button(BTN_BACK))
		menu_reshow();
}
