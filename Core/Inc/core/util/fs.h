#ifndef INC_CORE_UTIL_FS_H_
#define INC_CORE_UTIL_FS_H_

#include <stdint.h>

#define FS_FILE_TYPE_EMPTY 0 // Slot is available
#define FS_FILE_TYPE_RECORDING 1 // Recording of RDS data
#define FS_FILE_TYPE_SCREENSHOT 2 // Screenshot data

#define FS_STATUS_READY 0
#define FS_STATUS_FAT_CORRUPT 1
#define FS_STATUS_UNINIT 0xFF

#define FS_ERROR_OK 0
#define FS_ERROR_FS_NOT_READY 1
#define FS_ERROR_NO_FREE_SECTORS 2
#define FS_ERROR_NO_FREE_FILES 3
#define FS_ERROR_FILE_OPEN 4 // file is already open
#define FS_ERROR_FILE_NOT_OPEN 5 // no file is open
#define FS_ERROR_FILE_NOT_FOUND 6 // file index is invalid

#define FS_FILE_COUNT 64 // Max number of files

typedef struct __attribute__((packed)) fs_fat_entry {

	uint8_t file_type; // The type of file defined, also used for tagging free slots
	uint8_t extra_data; // File-type specific extra data, typically used for versioning
	uint16_t file_id; // A value incremented for each file ever created, used in lou of a reliable timestamp. Unlikely but may be duplicates
	uint16_t start_sector; // The first sector with data where 0 is the FAT sector
	uint32_t file_size; // Size, in bytes, of the file
	uint32_t timestamp_day; // The timestamp, if available, of when file was started to be written. Uses RDS julian date code. 0 if no time available
	uint32_t timestamp_time; // The number of milliseconds into the day this file was written.
	char hint[8]; // Text to describe the file, typically the callsign of the station or PI code in ASCII

} fs_fat_entry_t;

typedef struct __attribute__((packed)) fs_fats {

	uint32_t checksum; // CRC of the whole sector with this set to 0
	fs_fat_entry_t fat[FS_FILE_COUNT]; // File allocation table
	uint32_t free_sectors[32]; // Stores which sectors are free (0 bit) packed into 32 sectors per int. LSB is first.
	uint16_t free_sector_count; // Stores count of number of sectors currently free. Should be sure to keep in sync with free_sectors
	uint16_t next_file_id; // The continous file counter

} fs_fat_t;

extern uint8_t fs_status;
extern fs_fat_t fs_fat;

/*
 * Initializes the filesystem. Check status with fs_status
 */
void fs_init();

/*
 * Initializes the FAT, clearing all data. Sets state to ready.
 */
void fs_wipe();

/*
 * Deletes a file. Do not run on currently open file.
 * index: The index into the FAT to remove the file.
 */
int fs_file_delete(int index);

/*
 * Creates a new file and returns an FS_ERROR_* enum.
 * fileType: The type to use.
 * hint: The hint to assign.
 * fileIndex: Optional; If not 0, set to the FAT table entry for this file.
 */
int fs_file_create(uint8_t fileType, const char* hint, int* fileIndex);

/*
 * Writes count bytes to the currently open file.
 */
int fs_file_write(uint8_t* data, int count);

/*
 * Finalizes the currently written file.
 * extra: Extra data to store in the FAT.
 */
int fs_file_close(uint8_t extra);

#endif /* INC_CORE_UTIL_FS_H_ */
