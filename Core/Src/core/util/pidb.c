#include <core/util/pidb.h>
#include <core/util/flash.h>
#include <core/util/settings.h>
#include <string.h>

/* DEFINES */

#define PIDB_FLASH_START 4096 // First physical sector of the flash
#define PIDB_SECTOR_SIZE 512
#define PIDB_INDEX_RESOLUTION 8
#define PIDB_INDEX_SHIFT (16 - PIDB_INDEX_RESOLUTION) /* Bits to shift the pi code by to get the bin number */
#define PIDB_INDEX_COUNT 256 /* 2^PIDB_INDEX_RESOLUTION */
#define PIDB_WAYPOINT_LENGTH 32
#define PIDB_WAYPOINTS_PER_SECTOR (PIDB_SECTOR_SIZE / PIDB_WAYPOINT_LENGTH)

#define PIDB_FEATURE_LOCATION (1 << 0)

//#define PIDB_ENABLE_CRC

/* STRUCTS */

typedef struct __attribute__((packed)) pidb_file_header {

	uint32_t magic;
	uint32_t crc;
	uint8_t file_version;
	uint8_t waypoint_count;
	uint16_t sector_size;
	uint16_t sector_count;
	uint8_t features;
	uint8_t index_resolution;
	char name[16];

} pidb_file_header_t;

typedef struct __attribute__((packed)) pidb_sector_header {

	uint16_t start_pi;
	uint16_t end_pi; // inclusive
	uint16_t entries_count;
	uint32_t crc;

} pidb_sector_header_t;

/* VARS */

uint8_t pidb_status = PIDB_STATUS_UNINIT;
pidb_entry_t pidb_last_entry;
uint16_t pidb_last_entry_pi;
uint8_t pidb_last_entry_valid; // 1 if true, 0 if false

static pidb_file_header_t file_header;
static uint16_t current_sector = 0xFFFF; // Currently loaded sector in sector_buffer.
static uint16_t current_sector_read = 0; // The number of bytes loaded from the current sector.
static uint8_t sector_buffer[PIDB_SECTOR_SIZE]; // Buffer holding the latest sector.
static uint8_t toc_bin_len[PIDB_INDEX_COUNT]; // Number of sectors in this bin
static uint16_t toc_bin_sector[PIDB_INDEX_COUNT]; // Absolute sector this bin begins at
static pidb_entry_t closest_entry; // Copied into whilist scanning for other entries with matching PIs
static uint64_t closest_entry_distance; // Distance the closest entry is from the user.
static uint16_t duplicate_pis; // Counter of duplicate PIs found; Also indicates if closest_entry is clear. If 0, assume closest_entry is invalid
static uint16_t cached_entry_user_lat; // Latitude of user in cached entry. Used to check if cache is stale
static uint16_t cached_entry_user_lon; // Longitude of user in cached entry. Used to check if cache is stale

// 2-letter state names
const char* pidb_state_names_short =
		"AL"
		"AK"
		"AZ"
		"AR"
		"AS"
		"CA"
		"CO"
		"CT"
		"DE"
		"DC"
		"FL"
		"GA"
		"GU"
		"HI"
		"ID"
		"IL"
		"IN"
		"IA"
		"KS"
		"KY"
		"LA"
		"ME"
		"MD"
		"MA"
		"MI"
		"MN"
		"MS"
		"MO"
		"MT"
		"NE"
		"NV"
		"NH"
		"NJ"
		"NM"
		"NY"
		"NC"
		"ND"
		"MP"
		"OH"
		"OK"
		"OR"
		"PA"
		"PR"
		"RI"
		"SC"
		"SD"
		"TN"
		"TX"
		"TT"
		"UT"
		"VT"
		"VA"
		"VI"
		"WA"
		"WV"
		"WI"
		"WY";

// Decoding for CHAR
static const char encchar_lookup[] = {
		0x00,
		'A',
		'B',
		'C',
		'D',
		'E',
		'F',
		'G',
		'H',
		'I',
		'J',
		'K',
		'L',
		'M',
		'N',
		'O',
		'P',
		'Q',
		'R',
		'S',
		'T',
		'U',
		'V',
		'W',
		'X',
		'Y',
		'Z',
		' ',
		',',
		'.',
		'/',
		'-',
		'0',
		'1',
		'2',
		'3',
		'4',
		'5',
		'6',
		'7',
		'8',
		'9'
};

/* INTERNAL FUNCTIONS */

/*
 * Loads only the first specified number of bytes of the sector at the specified index. Caches this for later use.
 * index: The sector index, 0 being the file header.
 * count: Number of bytes to read.
 */
static void load_sector_partial(uint16_t index, int count) {
	//If the index matches the currently loaded sector and we have enough data, ignore
	if (current_sector == index && count <= current_sector_read)
		return;

	//Convert to address
	int addr = (int)index * PIDB_SECTOR_SIZE;

	//Read and set state
	flash_read(PIDB_FLASH_START + addr, sector_buffer, count);
	current_sector_read = count;
	current_sector = index;
}

/*
 * Loads the sector at the specified index. Stores it in the cache.
 * index: The sector index, 0 being the file header.
 */
static void load_sector(uint16_t index) {
	//Read full sector
	load_sector_partial(index, PIDB_SECTOR_SIZE);
}

/*
 * Decodes an encchar encoded string. Returns length consumed, or 0xFFFF if maxLen is hit.
 * offset: Offset within the sector buffer to read.
 * maxSrcLen: Max to read from source.
 * result: Output result.
 * maxLen: The maximum length of data that can be written. If it, returns 0xFFFF.
 */
static int decode_encchar_string(int offset, int maxSrcLen, char* result, int maxLen) {
	int readOffset = 0; // Offset, in bytes, from main offset
	int written = 0; // Decoded chars
	int bitsAvail = 0; // Bits available in register
	int reg = 0; // Temp place to store
	uint8_t read = 0; // Decoded char
	while (written < maxLen)
	{
	    //If register is running low, refil
	    if (bitsAvail < 5)
	    {
	        //Read from input if possible
	        read = 0;
	        if (readOffset < maxSrcLen)
	            read = sector_buffer[offset + readOffset++];

	        //Read from input
	        reg |= read << bitsAvail;
	        bitsAvail += 8;
	    }

	    //Decode upper 5 bits
	    if (bitsAvail >= 5)
	    {
	        //Read out
	        read = reg & 0b11111;
	        bitsAvail -= 5;
	        reg >>= 5;

	        //Copy to output
	        result[written++] = encchar_lookup[read];

	        //Check if EOF
	        if (read == 0)
	            break;
	    }
	}
	return readOffset;
}

/*
 * Decodes a located PI entry within a sector.
 * pi: The PI code that was decoded
 * sectorOffset: The offset into the sector buffer where the entry begins.
 */
static void decode_pi_entry(uint16_t pi, int sectorOffset) {
	//Zero out
	memset(&pidb_last_entry, 0, sizeof(pidb_entry_t));

	//Set state
	pidb_last_entry_pi = pi;
	pidb_last_entry_valid = 1;

	//Calculate the "extra length" of the entry from the lower nibble of the length byte
	int extraLen = (sector_buffer[sectorOffset] & 0x0F) + 1;

	//Get channel from the 3rd byte
	pidb_last_entry.channel = sector_buffer[sectorOffset + 2] >> 1;

	//Prefix of the callsign is defined by the MSB of the following byte regardless of if it is a long or short call
	if (sector_buffer[sectorOffset + 3] >> 7)
		pidb_last_entry.callsign[0] = 'W';
	else
		pidb_last_entry.callsign[0] = 'K';

	//The lowest bit of the channel byte indicates if this is a short or long call
	int pcOffset = sectorOffset + 5; // absolute offset to read from after this section, since it now moves around
	int extraFlags = 0;
	if (sector_buffer[sectorOffset + 2] & 1) {
		// LONG 32-bit call

		// Subtract 2 from extra length
		extraLen -= 2;
		pcOffset += 2;

		//Set flag
		extraFlags |= PIDB_FLAG_LONG_CALL;

		//Parse characters
		pidb_last_entry.callsign[1] = encchar_lookup[sector_buffer[sectorOffset + 3] & 0x3F];
		pidb_last_entry.callsign[2] = encchar_lookup[(sector_buffer[sectorOffset + 4] >> 2) & 0x3F];
		pidb_last_entry.callsign[3] = encchar_lookup[((sector_buffer[sectorOffset + 4] & 0b11) << 4) | (sector_buffer[sectorOffset + 5] >> 4)];
		pidb_last_entry.callsign[4] = encchar_lookup[((sector_buffer[sectorOffset + 5] & 0b1111) << 2) | (sector_buffer[sectorOffset + 6] >> 6)];
		pidb_last_entry.callsign[5] = encchar_lookup[sector_buffer[sectorOffset + 6] & 0b111111];
		pidb_last_entry.callsign[6] = 0;
	} else {
		// SHORT 16-bit call

		//Parse characters
		pidb_last_entry.callsign[1] = encchar_lookup[(sector_buffer[sectorOffset + 3] >> 2) & 0x1F];
		pidb_last_entry.callsign[2] = encchar_lookup[((sector_buffer[sectorOffset + 3] & 0b11) << 3) | (sector_buffer[sectorOffset + 4] >> 5)];
		pidb_last_entry.callsign[3] = encchar_lookup[sector_buffer[sectorOffset + 4] & 0x1F];
		pidb_last_entry.callsign[4] = 0;
	}

	//Mask the state of license and flags
	pidb_last_entry.state = sector_buffer[pcOffset] >> 2;
	pidb_last_entry.flags = (sector_buffer[pcOffset] & 0b11) | extraFlags;
	pcOffset++;

	//If enabled, read the location
	if (file_header.features & PIDB_FEATURE_LOCATION) {
		//Read the 3-byte packed location
		int locPacked = 0;
		locPacked |= sector_buffer[pcOffset++] << 0;
		locPacked |= sector_buffer[pcOffset++] << 8;
		locPacked |= sector_buffer[pcOffset++] << 16;

		//Unpack
		pidb_last_entry.loc_lat = locPacked & PIDB_LOCATION_SCALE;
		pidb_last_entry.loc_lon = locPacked >> 12;
	} else {
		//Set defaults
		pidb_last_entry.loc_lat = 0;
		pidb_last_entry.loc_lon = 0;
	}

	//Read the city of license
	int consumed = decode_encchar_string(pcOffset, extraLen, pidb_last_entry.col, (int)sizeof(pidb_last_entry.col));
	if (consumed == 0xFFFF) {
		//Signal error
		pidb_last_entry.col[0] = 'E';
		pidb_last_entry.col[1] = 'R';
		pidb_last_entry.col[2] = 'R';
		pidb_last_entry.col[3] = 'O';
		pidb_last_entry.col[4] = 'R';
		pidb_last_entry.col[5] = 0x00;
	}
}

// Derived from https://www.azillionmonkeys.com/qed/sqroot.html
// I don't totally understand what's going on here, admittedly
static uint64_t isqrt(uint64_t val) {
	uint64_t temp, g=0, b = 0x8000, bshft = 15;
	do {
		if (val >= (temp = (((g << 1) + b)<<bshft--))) {
		   g += b;
		   val -= temp;
		}
	} while (b >>= 1);
	return g;
}

/*
 * Compares distance between pidb_last_entry and closest_entry. If pidb_last_entry is closer than closest_entry (or it is not set), it overrides it
 */
static void compare_locations() {
	//Get the difference between lat/lon of the last entry and the user
	int diffLat = (int)user_settings.location_lat - (int)pidb_last_entry.loc_lat;
	if (diffLat < 0)
		diffLat = -diffLat;
	int diffLon = (int)user_settings.location_lon - (int)pidb_last_entry.loc_lon;
	if (diffLon < 0)
		diffLon = -diffLon;

	//Use the x^2+y^2=d^2 from middle school to calculate distance. This is probably not right for a globe but it's good enough for this
	uint64_t x = (uint64_t)diffLat;
	x *= x;
	uint64_t y = (uint64_t)diffLon;
	y *= y;
	uint64_t distance = isqrt(x + y);

	//If this is closer than the current closest entry (or this is the first one) override it
	if (duplicate_pis == 0 || distance <= closest_entry_distance) {
		//Overwrite
		closest_entry = pidb_last_entry;
		closest_entry_distance = distance;
	}
}

/*
 * Scans the single sector for data on the PI code. Returns 0 if not found, 1 if singular is found (read from pidb_last_entry) or 2 if duplicates are found (read from closest_entry)
 * pi: PI code to scan for.
 * sector: Sector index to read from.
 * binPi: The PI code of the bin to be used to generate full PI code
 * header: Header of the sector already loaded.
 */
static int scan_single_sector(uint16_t targetPi, uint16_t sector, uint16_t binPi, pidb_sector_header_t header) {
	//Load entire sector
	load_sector(sector);

	//Check CRC
#ifdef PIDB_ENABLE_CRC
	uint32_t calcCrc = perform_crc(sector_buffer, PIDB_SECTOR_SIZE, 6);
	if (header.crc != calcCrc)
		return; // maybe set an error flag?
#endif

	//Begin looping through PIs
	int offset = (int)sizeof(pidb_sector_header_t);
	uint8_t entryLen;
	uint8_t piNibble;
	uint16_t entryPi;
	int result = 0;
	for (int i = 0; i < header.entries_count; i++) {
		//Only the first two bytes are important for scanning, read them
		entryLen = sector_buffer[offset];
		piNibble = sector_buffer[offset + 1];

		//Use the upper nibble of the entry PI, bin PI, and lower byte of PI to create the PI code
		entryPi = ((entryLen & 0xF0) << 4) | binPi | piNibble;

		//Check that this is within the start and end in the header. If not, something has gone horribly wrong. Bail out.
		if (entryPi < header.start_pi || entryPi > header.end_pi)
			return 0;

		//Check if matches
		if (entryPi == targetPi) {
			//Match; Decode
			decode_pi_entry(targetPi, offset);

			//If it is not a duplicate, return it right away
			if (!(pidb_last_entry.flags & PIDB_FLAG_DUPLICATE))
				return 1;

			//Compare to the current closest item and override if blank or if this is closer
			compare_locations();

			//Update counter and set return to signal duplicates
			duplicate_pis++;
			result = 2;
		}

		//Determine skip size
		int skip = (entryLen & 0x0F) + 7;
		if (file_header.features & PIDB_FEATURE_LOCATION) {
			skip += 3;
		}

		//Use the lower nibble of the entry length to calculate how much to jump ahead
		offset += skip;
	}

	return result;
}

/* FUNCTIONS */

/*
 * Initializes the database by reading the header.
 */
void pidb_init() {
	//Init vars
	pidb_last_entry_pi = 0;
	pidb_last_entry_valid = 0;

	//Read first sector from flash
	load_sector(0);

	//Copy into the file header
	memcpy((uint8_t*)&file_header, sector_buffer, (int)sizeof(pidb_file_header_t));

	//Check that the constant matches
	if (file_header.magic != 0x42444950) {
		pidb_status = PIDB_STATUS_ERR_JUNK;
		return;
	}

	//Check CRC of the header
#ifdef PIDB_ENABLE_CRC
	uint32_t calcCrc = perform_crc(sector_buffer, PIDB_SECTOR_SIZE, 4);
	if (file_header.crc != calcCrc) {
		pidb_status = PIDB_STATUS_ERR_BADCRC;
		return;
	}
#endif

	//Check configuration
	if (file_header.sector_size != PIDB_SECTOR_SIZE || file_header.index_resolution != PIDB_INDEX_RESOLUTION) {
		pidb_status = PIDB_STATUS_ERR_BADCONFIG;
		return;
	}

	//Load the TOC
	int tocPos = (int)sizeof(pidb_file_header_t);
	uint16_t accum = ((file_header.waypoint_count + (PIDB_WAYPOINTS_PER_SECTOR - 1)) / PIDB_WAYPOINTS_PER_SECTOR) + 1;
	for (int i = 0; i < PIDB_INDEX_COUNT; i++) {
		//Store the length and offset of this bin
		toc_bin_len[i] = sector_buffer[tocPos];
		toc_bin_sector[i] = accum;

		//Add length to accumulator for next
		accum += sector_buffer[tocPos];

		//Increment pos
		tocPos++;
	}

	//Finally, as a sanity check, the last byte is 0xFF
	if (sector_buffer[tocPos] != 0xFF) {
		pidb_status = PIDB_STATUS_ERR_BADTOC;
		return;
	}

	//Ready!
	pidb_status = PIDB_STATUS_READY;
}

/*
 * Attempts to find info for the current PI. Resets the state of the last PI and caches it.
 * Returns 1 on success otherwise 0.
 * targetPi: PI code to scan for
 */
int pidb_scan(uint16_t targetPi) {
	//If not ready, immediately reject
	if (pidb_status != PIDB_STATUS_READY)
		return 0;

	//Check if cached (and the user hasn't changed their location)
	if (targetPi == pidb_last_entry_pi && user_settings.location_lat == cached_entry_user_lat && user_settings.location_lon == cached_entry_user_lon)
		return pidb_last_entry_valid;

	//Cache
	pidb_last_entry_pi = targetPi;
	pidb_last_entry_valid = 0;
	cached_entry_user_lat = user_settings.location_lat;
	cached_entry_user_lon = user_settings.location_lon;

	//Get the bin number
	int binIndex = targetPi >> PIDB_INDEX_SHIFT;

	//Using the table of contents, get the sector and number of sectors it may be in
	uint16_t sector = toc_bin_sector[binIndex];
	int sectorCount = toc_bin_len[binIndex];

	//Clear duplicate counter
	duplicate_pis = 0;

	//Scan headers of possible sectors
	pidb_sector_header_t header;
	for (int i = 0; i < sectorCount; i++) {
		//Read only header of this sector
		load_sector_partial(sector, (int)sizeof(pidb_sector_header_t));

		//Copy to aligned struct
		memcpy((uint8_t*)&header, sector_buffer, (int)sizeof(pidb_sector_header_t));

		//Check if valid
		if (targetPi >= header.start_pi && targetPi <= header.end_pi) {
			if (scan_single_sector(targetPi, sector, binIndex << PIDB_INDEX_SHIFT, header) == 1) // Only a return of 1 signals that a single one was found
				return 1;
		}

		//Scan next sector
		sector++;
	}

	//Check if any duplicates were found since duplicates cause the scan to continue
	if (duplicate_pis > 0) {
		//There is a valid result in closest_entry; Copy it and return OK
		pidb_last_entry = closest_entry;
		return 1;
	}

	return 0; // Fail
}

/*
 * Utility function to convert channel to [mhz].[decimal].
 * channel: The channel from the DB
 * mhz: The part before the decimal.
 * decimal: 1-9 part after the decimal.
 */
void pidb_decode_channel(int channel, int* mhz, int* decimal) {
	//Get the format before division
	int i = (channel * 2) + 879;

	//Split
	*mhz = i / 10;
	*decimal = i % 10;
}

/*
 * Reads a waypoint from the flash. Returns 1 on success, otherwise 0
 * waypointIndex: The index to read from.
 * name: Name of the waypoint is copied here, up to 32 bytes.
 * lat: Lat (encoded as 12-bit integer) copied here.
 * lon: Lon (encoded as 12-bit integer) copied here.
 */
int pidb_read_waypoint(int waypointIndex, char* name, uint16_t* lat, uint16_t* lon) {
	//Check that index is in range
	if (waypointIndex < 0 || waypointIndex >= file_header.waypoint_count)
		return 0;

	//Determine sector to read from and load it
	int sector = (waypointIndex / PIDB_WAYPOINTS_PER_SECTOR) + 1;
	load_sector(sector);

	//Get offset within sector
	int sectorOffset = PIDB_WAYPOINT_LENGTH * (waypointIndex % PIDB_WAYPOINTS_PER_SECTOR);

	//Copy the waypoint name
	memcpy(name, &sector_buffer[sectorOffset], 29);

	//Read packed lat/lon
	int locPacked = 0;
	locPacked |= sector_buffer[sectorOffset + 29] << 0;
	locPacked |= sector_buffer[sectorOffset + 30] << 8;
	locPacked |= sector_buffer[sectorOffset + 31] << 16;

	//Unpack lat/lon
	(*lat) = locPacked & PIDB_LOCATION_SCALE;
	(*lon) = locPacked >> 12;

	return 1;
}

/*
 * Gets the number of defined waypoints.
 */
int pidb_get_waypoint_count() {
	return file_header.waypoint_count;
}

/*
 * Reads info from the PIDB header. Requires state to be ready.
 * dbName: Destination for name, must be at least 17 characters.
 * dbFileVer: File version copied here.
 */
void pidb_copy_info(char* dbName, uint8_t* dbFileVer) {
	//Copy name to NULL terminator
	int nameLen = 0;
	while (nameLen < sizeof(file_header.name) / sizeof(char) && file_header.name[nameLen] != 0) {
		dbName[nameLen] = file_header.name[nameLen];
		nameLen++;
	}
	dbName[nameLen] = 0;

	//Copy version
	(*dbFileVer) = file_header.file_version;
}
