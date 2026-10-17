#ifndef INC_CORE_UTIL_SETTINGS_H_
#define INC_CORE_UTIL_SETTINGS_H_

#include <stdint.h>

/*
 * Main user settings
 */
typedef struct user_settings_info {

	//SYSTEM
	uint32_t checksum;
	uint32_t version;

	//SETTINGS
	uint8_t use_rtp;
	uint8_t use_dx;
	uint8_t use_partial;
	uint8_t location_waypoint; // Waypoint index+1, 0 meaning unset
	uint16_t location_lat; // Latitude of user
	uint16_t location_lon; // Longitude of user
	uint16_t vfo_calibration[5]; // ADC values at 87.9,	92.9, 97.9, 102.9 and 107.9 for calculating the tuner frequency
	uint8_t use_clear_on_channel; // If set, resets RDS when the channel changes

} user_settings_info_t;

extern user_settings_info_t user_settings;

/*
 * Loads settings from flash. Returns 1 if OK, otherwise 0.
 */
int user_settings_load();

/*
 * Clears out settings to default.
 * oldVersion: The old versions of settings (or 0 to clear everything). Only defaults items newer than this version.
 */
void user_settings_default(uint32_t oldVersion);

/*
 * Commits settings to flash, automatically updating checksum and version.
 */
void user_settings_save();

#endif /* INC_CORE_UTIL_SETTINGS_H_ */
