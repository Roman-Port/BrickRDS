#include <core/util/settings.h>
#include <core/util/flash.h>

user_settings_info_t user_settings;

#define USER_SETTINGS_CURRENT_VERSION 1
#define USER_SETTINGS_FLASH_LOCATION (FLASH_REGION_SETTINGS_SEC * FLASH_SECTOR_SIZE)

/*
 * Calculates CRC of the user settings struct, ignoring the existing CRC
 */
static uint32_t calc_crc() {
	return flash_calc_crc((uint8_t*)&user_settings, sizeof(user_settings_info_t), 0);
}

/*
 * Loads settings from flash. Returns 1 if OK, otherwise 0.
 */
int user_settings_load() {
	//Read from flash
	flash_read(USER_SETTINGS_FLASH_LOCATION, (uint8_t*)&user_settings, sizeof(user_settings_info_t));

	//Check that the checksum matches
	if (calc_crc() != user_settings.checksum)
		return 0;

	//Check that we haven't downgraded versions
	if (user_settings.version > USER_SETTINGS_CURRENT_VERSION)
		return 0;

	//Upgrade version by initializing defaults for older versions, otherwise this has no effect
	user_settings_default(user_settings.version);

	return 1;
}

/*
 * Clears out settings to default.
 * oldVersion: The old versions of settings (or 0 to clear everything). Only defaults items newer than this version.
 */
void user_settings_default(uint32_t oldVersion) {
	if (oldVersion < 1) {
		user_settings.use_rtp = 1;
		user_settings.use_dx = 0;
		user_settings.use_partial = 0;
		user_settings.location_waypoint = 0;
		user_settings.location_lat = 0;
		user_settings.location_lon = 0;
		user_settings.vfo_calibration[0] = 6087;
		user_settings.vfo_calibration[1] = 11084;
		user_settings.vfo_calibration[2] = 18902;
		user_settings.vfo_calibration[3] = 31639;
		user_settings.vfo_calibration[4] = 53697;
		user_settings.use_clear_on_channel = 1;
	}
}

/*
 * Commits settings to flash, automatically updating checksum and version.
 */
void user_settings_save() {
	//Write current version
	user_settings.version = USER_SETTINGS_CURRENT_VERSION;

	//Calculate CRC and store
	user_settings.checksum = calc_crc();

	//Disable protection
	flash_unprotect();

	//Clear sector in flash
	flash_erase_sector(0);

	//Write to the flash
	flash_write(USER_SETTINGS_FLASH_LOCATION, (uint8_t*)&user_settings, sizeof(user_settings_info_t));
}
