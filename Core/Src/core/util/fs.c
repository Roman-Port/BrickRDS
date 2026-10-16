#include <core/util/flash.h>
#include <core/util/fs.h>
#include <core/util/rds.h>
#include <main.h>
#include <string.h>

#define FS_MAX_SECTORS 1024 // The max number of sectors supported; If updating, remember to change free_sectors size in FAT
#define FS_SECTOR_FILE_OFFSET 8 // The offset into each sector where file bytes are written
#define FS_SECTOR_FILE_BYTES (FLASH_SECTOR_SIZE - FS_SECTOR_FILE_OFFSET) // The number of bytes in each sector actually used by files
#define FS_FAT_LOCATION (FLASH_SECTOR_SIZE * FLASH_REGION_FS_SEC) // The location of the FAT region on flash

/* STRUCTS */

typedef struct __attribute__((packed)) fs_sector_header {

	uint32_t checksum; // CRC of the whole sector with this set to 0
	uint16_t next_sector; // The next sector with data where 0 is the FAT sector and also indicates EOF
	uint16_t reserved; // currently unused

} fs_sector_header_t;

/* VARS */

uint8_t fs_status = FS_STATUS_UNINIT; // Status
fs_fat_t fs_fat; // The stored FAT
static int current_file_fat_index; // The index into the FAT table of the currently writtnen file, or -1 if no file is open
static int current_file_sector; // The sector where data is currently being written to; 0 is the first DATA sector
static uint32_t current_file_sector_consumed; // The number of TOTAL bytes currently in the data sector (including header)
static uint32_t current_file_sector_address; // Address of the beginning of the current file sector
static uint32_t current_file_length; // Length, in bytes, of the current file

/* INTERNAL FUNCTIONS */

/*
 * Calculates the FAT CRC
 */
static uint32_t calc_fat_crc() {
	return flash_calc_crc((uint8_t*)&fs_fat, sizeof(fs_fat_t), 0);
}

/*
 * Writes the FAT to the flash.
 */
static void flush_fat() {
	//Calculate CRC and store
	fs_fat.checksum = calc_fat_crc();

	//Disable protection
	flash_unprotect();

	//Clear sector in flash
	flash_erase_sector(FLASH_REGION_FS_SEC);

	//Write to the flash
	flash_write(FS_FAT_LOCATION, (uint8_t*)&fs_fat, sizeof(fs_fat_t));
}

/*
 * Finds the next writable sector. 0 is the first DATA sector, not the FAT sector. Returns -1 on error
 */
static int find_next_free_sector() {
	int sector = 0;
	int arrIndex = 0;
	uint32_t temp;
	while (sector < FS_MAX_SECTORS) {
		//Retrieve from array
		temp = fs_fat.free_sectors[arrIndex++];

		//Scan bits within this
		for (int i = 0; i < 32; i++) {
			if (temp & 1) {
				//Consumed; Advance to next
				sector++;
				temp >>= 1;
			} else {
				//Free!
				return sector;
			}
		}
	}
	return -1;
}

/*
 * Finds the index into the FAT of the next free file entry, or -1 if full.
 */
static int find_next_free_fat_entry() {
	for (int i = 0; i < FS_FILE_COUNT; i++) {
		if (fs_fat.fat[i].file_type == FS_FILE_TYPE_EMPTY)
			return i;
	}
	return -1;
}

/*
 * Copies the src filename to the entry, up to NULL or 8 characters. Leaves untouched if src is NULL. Assumes entry hint is already cleared out
 */
static void copy_file_name_to_entry(fs_fat_entry_t* entry, const char* src) {
	if (src == 0)
		return;
	for (int i = 0; i < 8 && src[i] != 0; i++) {
		entry->hint[i] = src[i];
	}
}

/*
 * Finalizes the data sector at current_file_sector.
 * nextSector: The next sector to write. 0 is the FAT, so add 1.
 */
static void finalize_data_sector(int nextSector) {
	//Set up header to write
	fs_sector_header_t secHeader;
	secHeader.checksum = 0; // TODO: Calc checksum
	secHeader.next_sector = nextSector;
	secHeader.reserved = 0;

	//Write header
	flash_write(current_file_sector_address, (uint8_t*)&secHeader, sizeof(fs_sector_header_t));

	//Clear
	current_file_sector = -1;
}

/*
 * Gets the physical address of a sector.
 * sector: The sector to read from, where 0 is FAT and 1 is the first data sector
 */
static uint32_t get_sector_address(int sectorIndex) {
	return FS_FAT_LOCATION + (FLASH_SECTOR_SIZE * sectorIndex);
}

/*
 * Sets up state to begin writing to the data sector at sectorIndex. Assumes it's already cleared as free. Closes old sector.
 * sectorIndex: The new sector, starting at 0.
 */
static void start_new_data_sector(int sectorIndex) {
	//Finalize old sector, if any
	if (current_file_sector != -1)
		finalize_data_sector(sectorIndex + 1);

	//Clear on flash
	flash_erase_sector(FLASH_REGION_FS_SEC + sectorIndex + 1);

	//Set the sector as consumed
	fs_fat.free_sectors[sectorIndex / 32] |= 1 << (sectorIndex % 32);
	fs_fat.free_sector_count--;

	//Set state
	current_file_sector = sectorIndex;
	current_file_sector_consumed = FS_SECTOR_FILE_OFFSET;
	current_file_sector_address = get_sector_address(sectorIndex + 1);
}

/* PUBLIC FUNCTIONS */

/*
 * Initializes the filesystem. Check status with fs_status
 */
void fs_init() {
	//Read FAT flash
	flash_read(FS_FAT_LOCATION, (uint8_t*)&fs_fat, sizeof(fs_fat_t));

	//Check that the checksum matches
	if (calc_fat_crc() != fs_fat.checksum) {
		fs_status = FS_STATUS_FAT_CORRUPT;
		return;
	}

	//Ready
	fs_status = FS_STATUS_READY;
	current_file_fat_index = -1;
	current_file_sector = -1;
	return;
}

/*
 * Initializes the FAT, clearing all data. Sets state to ready.
 */
void fs_wipe() {
	//Clear out the entire FAT
	memset(&fs_fat, 0, sizeof(fs_fat_t));

	//Set free sectors
	fs_fat.free_sector_count = FLASH_REGION_FS_LEN - 1;

	//Write to flash
	flush_fat();

	//Init
	fs_init();
}

/*
 * Deletes a file. Do not run on currently open file.
 * index: The index into the FAT to remove the file.
 */
int fs_file_delete(int index) {
	//Check if ready
	if (fs_status != FS_STATUS_READY)
		return FS_ERROR_FS_NOT_READY;

	//Check that this entry actually exists
	if (fs_fat.fat[index].file_type == FS_FILE_TYPE_EMPTY)
		return FS_ERROR_FILE_NOT_FOUND;

	//Follow the chain of sectors to mark them as free
	uint16_t nextSector = fs_fat.fat[index].start_sector;
	while (nextSector != 0) {
		//Clear the bit in the FAT signaling this as consumed and increment free count
		int sectorFlagIndex = (int)nextSector - 1;
		fs_fat.free_sectors[sectorFlagIndex / 32] &= ~(1 << (sectorFlagIndex % 32));
		fs_fat.free_sector_count++;

		//Load this sector's header
		fs_sector_header_t secHeader;
		flash_read(get_sector_address(nextSector), (uint8_t*)&secHeader, sizeof(fs_sector_header_t));
		nextSector = secHeader.next_sector;
	}

	//Shift any files after this in the table down
	for (int i = index; i < FS_FILE_COUNT - 1; i++)
		fs_fat.fat[i] = fs_fat.fat[i + 1];

	//Set the topmot entry to be deleted
	fs_fat.fat[FS_FILE_COUNT - 1].file_type = FS_FILE_TYPE_EMPTY;

	//Flush FAT
	flush_fat();

	return FS_ERROR_OK;
}

/*
 * Creates a new file and returns an FS_ERROR_* enum.
 * fileType: The type to use.
 * hint: The hint to assign.
 * fileIndex: Optional; If not 0, set to the FAT table entry for this file.
 */
int fs_file_create(uint8_t fileType, const char* hint, int* fileIndex) {
	//Check if ready
	if (fs_status != FS_STATUS_READY)
		return FS_ERROR_FS_NOT_READY;

	//Check if there is already a file open
	if (current_file_fat_index != -1)
		return FS_ERROR_FILE_OPEN;

	//Find a free FAT table entry
	current_file_fat_index = find_next_free_fat_entry();
	if (current_file_fat_index == -1)
		return FS_ERROR_NO_FREE_FILES;

	//Find a free data sector
	int firstSec = find_next_free_sector();
	if (firstSec == -1) {
		current_file_fat_index = -1;
		return FS_ERROR_NO_FREE_SECTORS;
	}

	//Get the date/time from RDS if we can
	uint32_t timeDay = 0;
	uint32_t timeTime = 0;
	if (rds_state.ct_timestamp) {
		//Convert from minutes/hours
		timeDay = rds_state.ct_timestamp / (60 * 24);
		timeTime = rds_state.ct_timestamp % (60 * 24);

		//Convert timestamp from minutes to milliseconds
		timeTime *= (60 * 1000);

		//Add offset
		timeTime += HAL_GetTick() - rds_state.ct_captured_at;
	}

	//Initialize the FAT table entry
	memset(&fs_fat.fat[current_file_fat_index], 0, sizeof(fs_fat_entry_t));
	fs_fat.fat[current_file_fat_index].file_type = fileType;
	fs_fat.fat[current_file_fat_index].file_size = 0;
	fs_fat.fat[current_file_fat_index].file_id = fs_fat.next_file_id++;
	fs_fat.fat[current_file_fat_index].start_sector = firstSec + 1;
	fs_fat.fat[current_file_fat_index].timestamp_day = timeDay;
	fs_fat.fat[current_file_fat_index].timestamp_time = timeTime;
	copy_file_name_to_entry(&fs_fat.fat[current_file_fat_index], hint);

	//Initialize internal state
	current_file_sector = -1;
	current_file_length = 0;

	//Start the data sector
	start_new_data_sector(firstSec);

	//Save the index
	if (fileIndex != 0)
		*fileIndex = current_file_fat_index;

	return FS_ERROR_OK;
}

/*
 * Writes count bytes to the currently open file.
 */
int fs_file_write(uint8_t* data, int count) {
	//Check if ready
	if (fs_status != FS_STATUS_READY)
		return FS_ERROR_FS_NOT_READY;

	//Make sure there is an active file
	if (current_file_fat_index == -1)
		return FS_ERROR_FILE_NOT_OPEN;

	//Process
	int blockLen;
	while (count > 0) {
		//Check how much can be written in the current sector
		blockLen = FLASH_SECTOR_SIZE - current_file_sector_consumed;
		if (blockLen > count)
			blockLen = count;

		//If no room, move to next sector
		if (blockLen <= 0) {
			//Find another sector to write to
			int nextSec = find_next_free_sector();
			if (nextSec == -1)
				return FS_ERROR_NO_FREE_SECTORS;

			//Start the data sector; This automatically finalizes the old sector
			start_new_data_sector(nextSec);
			continue;
		}

		//Write data into sector
		flash_write(current_file_sector_address + current_file_sector_consumed, data, blockLen);

		//Update global state
		current_file_sector_consumed += blockLen;
		current_file_length += blockLen;

		//Update state
		data += blockLen;
		count -= blockLen;
	}

	return FS_ERROR_OK;
}

/*
 * Finalizes the currently written file.
 * extra: Extra data to store in the FAT.
 */
int fs_file_close(uint8_t extra) {
	//Check if ready
	if (fs_status != FS_STATUS_READY)
		return FS_ERROR_FS_NOT_READY;

	//Make sure there is an active file
	if (current_file_fat_index == -1)
		return FS_ERROR_FILE_NOT_OPEN;

	//Finalize current sector
	finalize_data_sector(0);

	//Finalize the FAT entry
	fs_fat.fat[current_file_fat_index].file_size = current_file_length;

	//Flush the FAT
	flush_fat();

	//Finally, mark the file as closed
	current_file_fat_index = -1;

	return FS_ERROR_OK;
}
