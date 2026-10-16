#ifndef INC_CORE_UTIL_PIDB_H_
#define INC_CORE_UTIL_PIDB_H_

#include <stdint.h>

#define PIDB_STATUS_READY 0
#define PIDB_STATUS_ERR_JUNK 1 /* First page is missing magic; Likely means no data */
#define PIDB_STATUS_ERR_BADCRC 2 /* Bad CRC of first page */
#define PIDB_STATUS_ERR_BADCONFIG 3 /* Configuration of the file isn't compatible */
#define PIDB_STATUS_ERR_BADTOC 4 /* TOC appears corrupted */
#define PIDB_STATUS_UNINIT 0xFF

#define PIDB_FLAG_DUPLICATE (1 << 0)
#define PIDB_FLAG_LP_SUFFIX (1 << 1)
#define PIDB_FLAG_LONG_CALL (1 << 2)

#define PIDB_LOCATION_SCALE 0b111111111111

typedef struct pidb_entry {

	char callsign[8];
	uint8_t channel;
	uint8_t state;
	uint8_t flags;
	uint16_t loc_lat;
	uint16_t loc_lon;
	char col[17];

} pidb_entry_t;

extern uint8_t pidb_status;
extern pidb_entry_t pidb_last_entry;
extern uint16_t pidb_last_entry_pi;
extern uint8_t pidb_last_entry_valid; // 1 if true, 0 if false
extern const char* pidb_state_names_short; // Two-digit state codes in one long string

/*
 * Initializes the database by reading the header.
 */
void pidb_init();

/*
 * Attempts to find info for the current PI. Resets the state of the last PI and caches it.
 * Returns 1 on success otherwise 0.
 * targetPi: PI code to scan for
 */
int pidb_scan(uint16_t targetPi);

/*
 * Utility function to convert channel to [mhz].[decimal].
 * channel: The channel from the DB.
 * mhz: The part before the decimal.
 * decimal: 1-9 part after the decimal.
 */
void pidb_decode_channel(int channel, int* mhz, int* decimal);

/*
 * Reads a waypoint from the flash. Returns 1 on success, otherwise 0
 * waypointIndex: The index to read from.
 * name: Name of the waypoint is copied here, up to 32 bytes.
 * lat: Lat (encoded as 12-bit integer) copied here.
 * lon: Lon (encoded as 12-bit integer) copied here.
 */
int pidb_read_waypoint(int waypointIndex, char* name, uint16_t* lat, uint16_t* lon);

/*
 * Gets the number of defined waypoints.
 */
int pidb_get_waypoint_count();

/*
 * Reads info from the PIDB header. Requires state to be ready.
 * dbName: Destination for name, must be at least 17 characters.
 * dbFileVer: File version copied here.
 */
void pidb_copy_info(char* dbName, uint8_t* dbFileVer);

#endif /* INC_CORE_UTIL_PIDB_H_ */
