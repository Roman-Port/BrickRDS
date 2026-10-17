#include <core/util/display.h>
#include <core/util/charset.h>
#include <core/util/textarea.h>
#include <core/util/flash.h>
#include <core/util/pidb.h>
#include <core/util/settings.h>
#include <core/util/tuner.h>
#include <core/util/rds.h>
#include <core/util/fs.h>
#include <core/views.h>
#include <core/core.h>
#include <main.h>

view_info_t* current_view = 0;
static view_info_t* next_view = 0; // Queued view change
static int current_frame_btns = 0; // New buttons held this frame
static int last_frame_btns = 0; // All buttons held during last frame

/*
 * Changes the active view to this, deactivating any old view and activating this view.
 * view: New view; May be null.
 */
void core_change_view(view_info_t* view) {
	//Save for next tick
	next_view = view;
}

/*
 * Called at program launch from main.
 */
void core_init() {
	//Init display and mark internal charsets for both controllers as dirty so charsets will be uploaded. If this was a software reset, skip some init steps
	disp_clear();
	disp_init(DISP_PIN_EX, LL_RCC_IsActiveFlag_SFTRST());
	disp_write_text_at(0, 0, "BrickRDS Initializing...", -1);
	disp_flush();
	charset_mark_dirty(CHARSET_DISPLAY_UPPER);
	charset_mark_dirty(CHARSET_DISPLAY_LOWER);

	//Set up status window
	textarea_data_t status;
	textarea_init(&status, 3, 0, 40, 1);

	//Initialize user settings
	textarea_copy_text(&status, "Initializing settings...");
	disp_flush();
	if (!user_settings_load()) {
		//Show error
		textarea_copy_text(&status, "Settings corrupt; Press BACK to clear.");
		disp_flush();

		//Wait for back button
		while (HAL_GPIO_ReadPin(BTN_BACK_GPIO_Port, BTN_BACK_Pin));

		//Show message
		textarea_copy_text(&status, "Clearing settings...");
		disp_flush();

		//Clear
		user_settings_default(0);
		user_settings_save();
	}

	//Now that settings are loaded, calculate tuner table using calibration data
	tuner_recalc_table();

	//Initialize filesystem
	textarea_copy_text(&status, "Initializing filesystem...");
	disp_flush();
	fs_init();
	if (fs_status != FS_STATUS_READY) {
		//Show error
		textarea_copy_text(&status, "FS corrupt; Press BACK to clear.");
		disp_flush();

		//Wait for back button
		while (HAL_GPIO_ReadPin(BTN_BACK_GPIO_Port, BTN_BACK_Pin));

		//Show message
		textarea_copy_text(&status, "Clearing filesystem...");
		disp_flush();

		//Clear
		fs_wipe();
	}

	//Initialize PI DB
	textarea_copy_text(&status, "Initializing PI DB...");
	disp_flush();
	pidb_init();
	if (pidb_status != PIDB_STATUS_READY) {
		//Show error
		char buffer[32];
		sprintf(buffer, "PI DB Init Error %i", pidb_status);
		textarea_copy_text(&status, buffer);
		disp_flush();

		//Pause
		HAL_Delay(5000);
	}

	//Go to default view
	core_change_view(&view_rds_rt);
}

/*
 * Returns 1 if this button was pressed since the last frame.
 */
int core_get_button(int btn) {
	return (current_frame_btns & btn) != 0;
}

/*
 * Called on an endless loop from main.
 */
void core_tick() {
	//Process view changes
	if (next_view != 0) {
		//Deinit old view
		if (current_view != 0 && current_view->cb_close != 0)
			current_view->cb_close();

		//Set new view
		current_view = next_view;
		next_view = 0;

		//Init new view
		if (current_view != 0 && current_view->cb_open != 0)
			current_view->cb_open();
	}

	//Update RDS
	rds_process();

	//Read current button inputs
	int pressedBtns = 0;
	if (!HAL_GPIO_ReadPin(BTN_LEFT_GPIO_Port, BTN_LEFT_Pin))
		pressedBtns |= BTN_LEFT;
	if (!HAL_GPIO_ReadPin(BTN_RIGHT_GPIO_Port, BTN_RIGHT_Pin))
		pressedBtns |= BTN_RIGHT;
	if (!HAL_GPIO_ReadPin(BTN_BACK_GPIO_Port, BTN_BACK_Pin))
		pressedBtns |= BTN_BACK;
	if (!HAL_GPIO_ReadPin(BTN_CLEAR_GPIO_Port, BTN_CLEAR_Pin))
		pressedBtns |= BTN_ENTER;

	//Check for global clear button
	if (!HAL_GPIO_ReadPin(BTN_RESET_GPIO_Port, BTN_RESET_Pin))
		rds_reset();

	//Check for specific combos
	if ((pressedBtns & (BTN_LEFT | BTN_RIGHT)) == (BTN_LEFT | BTN_RIGHT)) {
		disp_capture_screenshot_interactive();
		pressedBtns = last_frame_btns;
	}

	//Get buttons released since last frame
	current_frame_btns = ~pressedBtns & last_frame_btns;

	//Store raw buttons
	last_frame_btns = pressedBtns;

	//Tick the view if defined
	if (current_view != 0 && current_view->cb_frame != 0)
		current_view->cb_frame();

	//Push changes (only if there is not a view change pending)
	if (next_view == 0)
		disp_flush();
}
