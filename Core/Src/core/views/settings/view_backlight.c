#include <core/util/display.h>
#include <core/util/menu.h>
#include <core/util/charset.h>
#include <core/util/backlight.h>
#include <core/core.h>
#include <core/views.h>
#include <stdint.h>
#include <stdio.h>
#include <main.h>

/*
 * Renders a 32 pixel graph with the specified points.
 */
static void render_graph(int p1, int p2, int p3) {
	//Position cursor
	disp_set_cursor(2, 2);

	//Render
	char render;
	for (int i = 0; i < 32; i++) {
		//Calculate
		render = 0x00; // 0
		if (i == p1)
			render = 0x01; // 0 -> 1
		if (i > p1)
			render = 0x02; // 1
		if (i == p2)
			render = 0x03; // 1 -> 2
		if (i > p2)
			render = '-'; // 2
		if (i == p3)
			render = 0x04; // 2 -> 3
		if (i > p3)
			render = 0x05; // 3

		//Write
		disp_write_char(render);
	}
}

/*
 * Renders the marker character and text at the row
 */
static void render_marker(int row, char cursor, uint16_t raw) {
	//Init buffer and clear
	char buf[40];
	for (int i = 0; i < 40; i++)
		buf[i] = ' ';

	//Render cursor
	buf[raw >> 11] = cursor;

	//Render raw
	sprintf(&buf[33], "%i", raw);

	//Render
	disp_write_text_at(row, 2, buf, -1);
}

static void update_io_graphs() {
	//Update each
	render_marker(1, 0x06, backlight_reading);
	render_marker(3, 0x07, backlight_brightness);
}

/*
 * Init function
 */
void view_settings_backlight_init() {
	//Clear
	disp_clear();

	//Change charset
	charset_change(CHARSET_BACKLIGHT, CHARSET_BACKLIGHT);

	//Set title
	disp_write_text_at(0, 0, "Backlight Setup", -1);

	//Set left side
	disp_write_text_at(1, 0, "I:", -1);
	disp_write_text_at(3, 0, "O:", -1);

	render_graph(2, 5, 10);
	update_io_graphs();
}

/*
 * Tick function
 */
void view_settings_backlight_tick() {
	/*char buffer[80];

	//Create status line
	sprintf(buffer, "RECEIVED=%c TOGGLE=%c RT_BANK=%c RUN=%c", read_bit_from_status(RDS_STATUS_RTP_RECEIVED), read_bit_from_status(RDS_STATUS_RTP_TOGGLE), read_bit_from_status(RDS_STATUS_RT_BANK_B), read_bit_from_status(RDS_STATUS_RTP_RUNNING));
	disp_write_text_at(1, 0, buffer, -1);

	//Render tags
	render_rtp_line(2, 0, buffer);
	render_rtp_line(3, 1, buffer);*/

	//Update graphs
	update_io_graphs();

	//Handle input
	if (core_get_button(BTN_ENTER)) {
		disp_write_text_at(30, 30, "O:", -1);
	}
	if (core_get_button(BTN_BACK))
		menu_reshow();
}

view_info_t view_settings_backlight = (view_info_t) {
	.cb_open = view_settings_backlight_init,
	.cb_frame = view_settings_backlight_tick,
	.cb_close = 0
};
