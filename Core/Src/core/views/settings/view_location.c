#include <core/views/settings/view_location.h>
#include <core/util/settings.h>
#include <core/util/display.h>
#include <core/util/menu.h>
#include <core/util/pidb.h>
#include <core/util/textarea.h>
#include <core/core.h>
#include <core/views.h>
#include <stdio.h>
#include <string.h>

view_info_t view_settings_location = (view_info_t) {
	.cb_open = view_settings_location_init,
	.cb_frame = view_settings_location_tick,
	.cb_close = 0
};

static textarea_data_t ta_name;
static int commit_pending; // Set to 1 when the setting has been changed

/*
 * Loads the waypoint entry at index (0 being start). Handles -1 index for none.
 * index: The index (starting at 0) of the waypoint.
 * validate: Bool; If set, checks that the retreived waypoint matches the location in user settings, defaulting to invalid if not.
 * commit: Bool; If set, writes the new value into the settings. If validation fails this will be ignored.
 */
static void load_waypoint_entry(int index, int validate, int commit) {
	//Default to none
	char name[32] = "(none)";
	uint16_t lat = 0;
	uint16_t lon = 0;

	//If >=0, load
	if (index >= 0) {
		//Retreive from flash
		if (!pidb_read_waypoint(index, name, &lat, &lon)) {
			//Default to none
			index = -1;
		}
	}

	//If validate is on, check against user settings
	if (validate && lat != user_settings.location_lat && lon != user_settings.location_lon) {
		strcpy(name, "(invalid)");
		commit = 0;
	}

	//If save is on, write to user settings
	if (commit) {
		commit_pending = 1;
		user_settings.location_waypoint = (uint8_t)(index + 1);
		user_settings.location_lat = lat;
		user_settings.location_lon = lon;
	}

	//Write to display
	textarea_copy_text(&ta_name, name);
}


/*
 * Init function
 */
void view_settings_location_init() {
	//Clear state
	commit_pending = 0;

	//Set up view
	disp_clear();
	disp_write_text_at(0, 0, "SET LOCATION", -1);
	disp_write_text_at(1, 0, "Aids in identifying translators.", -1);

	//Only set up menu if PIDB is ready
	if (pidb_status == PIDB_STATUS_READY) {
		//Render arrows at bottom
		disp_write_text_at(3, 0, "<", -1);
		disp_write_text_at(3, 39, ">", -1);

		//Init text area
		textarea_init(&ta_name, 3, 2, 36, 1);

		//Load stored location and validate but do not save
		load_waypoint_entry((int)user_settings.location_waypoint - 1, 1, 0);
	} else {
		//Show error
		disp_write_text_at(3, 0, "Can't change; PIDB not available.", -1);
	}
}

/*
 * Tick function
 */
void view_settings_location_tick() {
	//If in error state, handle specially
	if (pidb_status != PIDB_STATUS_READY) {
		//Wait for back button
		if (core_get_button(BTN_BACK))
			menu_reshow();

		return;
	}

	//Check inputs
	if (core_get_button(BTN_LEFT)) {
		//Decrement; If it is already 0, get the number of entries and jump to that
		if (user_settings.location_waypoint == 0) {
			//Set to last entry
			user_settings.location_waypoint = (uint8_t)pidb_get_waypoint_count();
		} else {
			//Just decrement
			user_settings.location_waypoint--;
		}

		//Update and save
		load_waypoint_entry((int)user_settings.location_waypoint - 1, 0, 1);
	} else if (core_get_button(BTN_RIGHT)) {
		//Increment; The load will automatically loop it back to 0 if out of range
		user_settings.location_waypoint++;

		//Update and save
		load_waypoint_entry((int)user_settings.location_waypoint - 1, 0, 1);
	} else if (core_get_button(BTN_BACK)) {
		//Commit to flash
		if (commit_pending)
			user_settings_save();

		//Go back to menu
		menu_reshow();
	}
}
