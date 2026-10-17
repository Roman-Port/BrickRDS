#include <core/util/display.h>
#include <core/util/menu.h>
#include <core/util/settings.h>
#include <core/util/tuner.h>
#include <core/util/textarea.h>
#include <core/util/charset.h>
#include <core/core.h>
#include <core/views.h>
#include <stdint.h>
#include <stdio.h>
#include <main.h>

#define CHAR_GRAPH_END_L 0
#define CHAR_GRAPH_END_R 2
#define CHAR_GRAPH_LINE 1
#define CHAR_GRAPH_MIDDLE 3
#define CHAR_GRAPH_CURSOR_MIDDLE 4
#define CHAR_GRAPH_CURSOR_REG 5
#define CHAR_GRAPH_CURSOR_END_L 6
#define CHAR_GRAPH_CURSOR_END_R 7

static textarea_data_t ta_info;
static textarea_data_t ta_timer;
static uint32_t last_updated = 0;

// Renders the tuner showing where in range it is
static void render_tuner_graph(int curCh, uint16_t raw) {
	//Set cursor and render underlying graph
	disp_set_cursor(3, 0);
	disp_write_char(CHAR_GRAPH_END_L);
	for (int i = 0; i < 18; i++)
		disp_write_char(CHAR_GRAPH_LINE);
	disp_write_char(CHAR_GRAPH_MIDDLE);
	for (int i = 0; i < 18; i++)
		disp_write_char(CHAR_GRAPH_LINE);
	disp_write_char(CHAR_GRAPH_END_R);

	//Get the bounds of the current channel
	uint16_t lower;
	uint16_t upper;
	tuner_get_bounds(curCh, &lower, &upper);

	//Get the position of the current raw value between the lower and upper bounds, *39, and rounded
	int pos = (int)((((double)(raw - lower) / (upper - lower)) * 39) + 0.5);

	//Constrain
	if (pos < 0)
		pos = 0;
	if (pos > 38)
		pos = 38;

	//Get the character to use
	char c;
	switch (pos) {
	case 0: c = CHAR_GRAPH_CURSOR_END_L; break;
	case 38: c = CHAR_GRAPH_CURSOR_END_R; break;
	case 19: c = CHAR_GRAPH_CURSOR_MIDDLE; break;
	default: c = CHAR_GRAPH_CURSOR_REG; break;
	}

	//Render cursor
	disp_set_cursor(3, pos);
	disp_write_char(c);
}

/*
 * Init function
 */
void view_settings_calibration_info_init() {
	//Clear
	disp_clear();

	//Change charset
	charset_change(-1, CHARSET_TUNER_CALIBRATION);

	//Set title
	disp_write_text_at(0, 0, "Tuner Calibration Info", -1);

	//Set up text areas
	textarea_init(&ta_info, 1, 0, 40, 1);
	textarea_init(&ta_timer, 2, 0, 40, 1);
}

/*
 * Tick function
 */
void view_settings_calibration_info_tick() {
	char renderedValue[40];

	//Only update periodically
	if ((HAL_GetTick() - last_updated) > 100) {
		//Read current info
		int currentCh = tuner_current_channel;
		uint16_t raw = tuner_sample;

		//Update the current info on-screen
		if (currentCh >= 0) {
			//Valid channel
			//Get freq
			char channelFreq[8];
			tuner_get_channel_freq(currentCh, channelFreq);

			//Get the bounds of the current channel
			uint16_t lower;
			uint16_t upper;
			tuner_get_bounds(currentCh, &lower, &upper);

			//Format
			sprintf(renderedValue, "%s [%i-%i] (%i/%i/%i)", channelFreq, (int)lower, (int)upper, (int)tuner_diag_min, (int)raw, (int)tuner_diag_max);
		} else {
			//Invalid channel
			sprintf(renderedValue, "Invalid channel (%i/%i/%i)", (int)tuner_diag_min, (int)raw, (int)tuner_diag_max);
		}
		textarea_copy_text(&ta_info, renderedValue);
		textarea_render(&ta_info);

		//Again testing if current channel is valid, update bottom display
		if (currentCh >= 0)
			render_tuner_graph(currentCh, raw);
		else
			disp_clear_line(3); // hide graph

		//Update last update timer
		last_updated = HAL_GetTick();
	}

	//Make timer
	sprintf(renderedValue, "Last tuned %is ago. %i sample/sec", (int)((HAL_GetTick() - tuner_last_changed) / 1000), tuner_get_samples_per_sec());
	textarea_copy_text(&ta_timer, renderedValue);
	textarea_render(&ta_timer);

	//Handle input
	if (core_get_button(BTN_CLEAR))
		tuner_diag_reset = 1;
	if (core_get_button(BTN_BACK))
		menu_reshow();
}

view_info_t view_settings_calibration_info = (view_info_t) {
	.cb_open = view_settings_calibration_info_init,
	.cb_frame = view_settings_calibration_info_tick,
	.cb_close = 0
};
