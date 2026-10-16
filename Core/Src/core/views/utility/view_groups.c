#include <core/views/utility/view_groups.h>
#include <core/util/textarea.h>
#include <core/util/display.h>
#include <core/util/rds.h>
#include <core/views.h>
#include <core/core.h>

#define GROUP_WIDTH 10
#define GROUPS_PER_ROW 4
#define GROUP_COUNT 16

view_info_t view_utility_groups = (view_info_t) {
	.cb_open = view_utility_groups_init,
	.cb_frame = view_utility_groups_tick,
	.cb_close = 0
};

/*
 * Writes a right-aligned numerical value to output.
 * output: Output string
 * outputLen: Number of characters to write
 * value: Value to write
 */
static void write_counter(char* output, int outputLen, int value) {
	for (int i = outputLen - 1; i >= 0; i--) {
		output[i] = '0' + (value % 10);
		value /= 10;
	}
}

/*
 * Init function for RDS groups
 */
void view_utility_groups_init() {
	//Clear
	disp_clear();

	//Render counters
	char buffer[3];
	buffer[2] = ':';
	int group = 0;
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < GROUPS_PER_ROW; col++) {
			//Render counter
			write_counter(buffer, 2, group++);

			//Draw
			disp_write_text_at(row, col * GROUP_WIDTH, buffer, 3);
		}
	}

	//Enable RDS
	rds_enable();
}

/*
 * Tick function for RDS groups
 */
void view_utility_groups_tick() {
	//Render counters
	char buffer[GROUP_WIDTH];
	int counterLen = GROUP_WIDTH - 4;
	int group = 0;
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < GROUPS_PER_ROW; col++) {
			//Render counter
			write_counter(buffer, counterLen, rds_state.group_counter[group++]);

			//Draw
			disp_write_text_at(row, (col * GROUP_WIDTH) + 3, buffer, counterLen);
		}
	}

	//Handle back button
	if (core_get_button(BTN_BACK))
		menu_reshow();
}
