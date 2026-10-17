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

static const char* step_prompts[] = {
		"87.9",
		"92.9",
		"97.9",
		"102.9",
		"107.9"
};

static int current_step;
static uint16_t new_calibration_data[5];
static textarea_data_t ta_value;

// Changes to the specified step index
static void change_step(int index) {
	//Update
	current_step = index;

	//Clear lines
	disp_clear_line(1); // Clear old prompt
	disp_clear_line(2); // Clear rejection, if there is any

	//Format and render text
	char buf[40];
	sprintf(buf, "Tune radio to %s FM and press ENTER.", step_prompts[index]);
	disp_write_text_at(1, 0, buf, -1);
}

// Called when ENTER is pressed when prompted.
static void process_submit() {
	//Check if finished
	if (current_step >= 5) {
		//Write to user settings and commit
		for (int i = 0; i < 5; i++)
			user_settings.vfo_calibration[i] = new_calibration_data[i];
		user_settings_save();

		//Recalculate table
		tuner_recalc_table();

		//Change view to the calibration info
		core_change_view(&view_settings_calibration_info);
		return;
	}

	//Poll latest reading
	uint16_t value = tuner_sample;

	//Check if it makes sense: Is it greater than the previous?
	if (current_step > 0 && value <= new_calibration_data[current_step - 1]) {
		disp_write_text_at(2, 0, "Reading invalid. Try again.", -1);
		return;
	}

	//Store new value
	new_calibration_data[current_step] = value;

	//Check if done
	int next = current_step + 1;
	if (next >= 5) {
		//Clear display
		disp_clear_line(1);
		disp_clear_line(2);
		disp_clear_line(3);

		//Show done message
		disp_write_text_at(1, 0, "Calibration complete.", -1);
		disp_write_text_at(2, 0, "Press ENTER to save and continue.", -1);

		//Write to step counter
		current_step = next;
	} else {
		//Continue to next step
		change_step(next);
	}
}

/*
 * Init function
 */
void view_settings_calibrate_init() {
	//Clear
	disp_clear();

	//Set title
	disp_write_text_at(0, 0, "Calibrate Tuner", -1);

	//Set up text area to render raw value
	textarea_init(&ta_value, 3, 34, 6, 1);
	ta_value.flags |= TEXTAREA_FLAG_ALIGN_RIGHT;

	//Change to first step
	change_step(0);
}

/*
 * Tick function
 */
void view_settings_calibrate_tick() {
	//Update the current value on-screen
	char renderedValue[8];
	sprintf(renderedValue, "%i", (int)tuner_sample);
	textarea_copy_text(&ta_value, renderedValue);
	textarea_render(&ta_value);

	//Handle input
	if (core_get_button(BTN_ENTER))
		process_submit();
	else if (core_get_button(BTN_BACK))
		menu_reshow();
}

view_info_t view_settings_calibrate = (view_info_t) {
	.cb_open = view_settings_calibrate_init,
	.cb_frame = view_settings_calibrate_tick,
	.cb_close = 0
};
