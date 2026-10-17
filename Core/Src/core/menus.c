#include <core/menus.h>
#include <core/views.h>
#include <core/util/tuner.h>
#include <core/util/menu.h>
#include <core/util/settings.h>
#include <core/util/fs.h>

static void factory_reset_confirmed(void* ctx) {
	//Clear and save user settings
	user_settings_default(0);
	user_settings_save();

	//Nuke filesystem
	fs_wipe();

	//Recalculate tuner table
	tuner_recalc_table();

	//Back out to previous menu
	menu_back();
}

static void generic_save_settings(void* ctx) {
	//Just write settings
	user_settings_save();
}

static menu_item_t utility_menu[] = {
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.text = "View RT+ Info",
				.info = {
						.view = {
								.view = &view_utility_rtp
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.text = "View ODAs",
				.info = {
						.view = {
								.view = &view_utility_odas
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.text = "View Group Count",
				.info = {
						.view = {
								.view = &view_utility_groups
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.text = "View RDS Stats",
				.info = {
						.view = {
								.view = &view_utility_stats
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_BACK,
				.text = "Back"
		},
		{
				.type = MENU_ITEM_TYPE_END
		}
};

static menu_item_t settings_factory_menu[] = {
		{
				.type = MENU_ITEM_TYPE_BTN,
				.text = "Confirm",
				.info = {
						.btn = {
								.callback = factory_reset_confirmed
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_BACK,
				.text = "Cancel"
		},
		{
				.type = MENU_ITEM_TYPE_END
		}
};

static menu_item_t settings_menu[] = {
		{
				.type = MENU_ITEM_TYPE_DIRECTCHECK,
				.text = "Clear On Freq Change",
				.info = {
						.direct_check = {
								.value = &user_settings.use_clear_on_channel,
								.callback = generic_save_settings
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.icon = MENU_ITEM_ICON_PIN,
				.text = "Set Location",
				.info = {
						.view = {
								.view = &view_settings_location
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.icon = 0,
				.text = "PC Mode",
				.info = {
						.view = {
								.view = &view_settings_pcmode
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.icon = 0,
				.text = "Calibrate Tuner",
				.info = {
						.view = {
								.view = &view_settings_calibrate
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.icon = 0,
				.text = "Test Tuner Calibration",
				.info = {
						.view = {
								.view = &view_settings_calibration_info
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.icon = 0,
				.text = "Setup Backlight",
				.info = {
						.view = {
								.view = &view_settings_backlight
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_SUBMENU,
				.icon = MENU_ITEM_ICON_RESET,
				.text = "Factory Reset",
				.info = {
						.submenu = {
								.menu_name = "Factory Reset",
								.items = settings_factory_menu
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_VIEW,
				.icon = 0,
				.text = "About",
				.info = {
						.view = {
								.view = &view_settings_about
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_BACK,
				.text = "Back"
		},
		{
				.type = MENU_ITEM_TYPE_END
		}
};

static menu_item_t main_menu[] = {
		{
				.type = MENU_ITEM_TYPE_DIRECTCHECK,
				.text = "Use RT+",
				.info = {
						.direct_check = {
								.value = &user_settings.use_rtp,
								.callback = generic_save_settings
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_DIRECTCHECK,
				.text = "Show Partial PS/RT",
				.info = {
						.direct_check = {
								.value = &user_settings.use_partial,
								.callback = generic_save_settings
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_DIRECTCHECK,
				.text = "RDS DX",
				.info = {
						.direct_check = {
								.value = &user_settings.use_dx,
								.callback = generic_save_settings
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_SUBMENU,
				.icon = MENU_ITEM_ICON_TOOLBOX,
				.text = "Tools",
				.info = {
						.submenu = {
								.menu_name = "Tools",
								.items = utility_menu
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_SUBMENU,
				.icon = MENU_ITEM_ICON_GEARS,
				.text = "Settings",
				.info = {
						.submenu = {
								.menu_name = "Settings",
								.items = settings_menu
						}
				}
		},
		{
				.type = MENU_ITEM_TYPE_BACK,
				.text = "Back"
		},
		{
				.type = MENU_ITEM_TYPE_END
		}
};

void menus_show_mainmenu() {
	menu_show("Main Menu", main_menu);
}
