#ifndef INC_CORE_UTIL_FLASH_H_
#define INC_CORE_UTIL_FLASH_H_

#include <stdint.h>

#define FLASH_SIZE_SECTORS 1000 // number of sectors
#define FLASH_SECTOR_SIZE 4096 // physical size of a sector, in bytes

#define FLASH_REGION_SETTINGS_SEC 0 // The sector settings live on
#define FLASH_REGION_SETTINGS_LEN 1 // Number of sectors

#define FLASH_REGION_PIDB_SEC (FLASH_REGION_SETTINGS_SEC + FLASH_REGION_SETTINGS_LEN) // The sector PIDB live on
#define FLASH_REGION_PIDB_LEN 128 // Number of sectors

#define FLASH_REGION_FS_SEC (FLASH_REGION_PIDB_SEC + FLASH_REGION_PIDB_LEN) // The sector files live on
#define FLASH_REGION_FS_LEN (FLASH_SIZE_SECTORS - FLASH_REGION_FS_SEC) // Number of sectors

/*
 * Changes properties of flash to remove protections.
 */
void flash_unprotect();

/*
 * Erases the flash. Waits for operation to finish.
 */
void flash_erase();

/*
 * Erases a 4 kB sector.
 * sector: The sector index (NOT the physical address).
 */
void flash_erase_sector(int sector);

/*
 * Reads from the flash.
 * address: Address to read from.
 * result: Output buffer to write into.
 * len: Length to read.
 */
void flash_read(uint32_t address, uint8_t* result, int len);

/*
 * Writes to the flash.
 * address: Address to write to.
 * data: Data to write from.
 * len: Number of bytes to write.
 */
void flash_write(uint32_t address, const uint8_t* data, int len);

/*
 * Utility function, performs a CRC32; Modified from https://web.archive.org/web/20190108202303/http://www.hackersdelight.org/hdcodetxt/crc.c.txt
 * buffer: Buffer to perform actions on
 * len: Length of buffer
 * crcPos: Location where the CRC is stored within the buffer, or -1 to disable. Bytes are subsistuted as 0 at this plus four bytes.
 */
uint32_t flash_calc_crc(uint8_t* buffer, int len, int crcPos);

#endif /* INC_CORE_UTIL_FLASH_H_ */
