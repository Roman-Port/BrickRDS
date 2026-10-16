#include <core/util/flash.h>
#include "main.h"

/*
 * Writes a single byte and gets a response.
 */
static uint8_t perform_single_command(uint8_t req) {
	uint8_t response = 0x00;
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, &req, 1, 1000);
	HAL_SPI_Receive(&hspi1, &response, 1, 1000);
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
	return response;
}

/*
 * Reads the status register until it is no longer busy. Returns the number of cycles waited.
 */
static int wait_for_unbusy() {
	int cycles = -1;
	uint8_t req = 0x05;
	uint8_t response = 0x00;
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, &req, 1, 1000);
	do {
		HAL_SPI_Receive(&hspi1, &response, 1, 1000);
		cycles++;
	} while ((response & 1) == 1);
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
	return cycles;
}

/*
 * Reads status register.
 */
static uint8_t flash_read_status() {
	return perform_single_command(0x05);
}

/*
 * Writes status register.
 */
static void flash_write_status(uint8_t status) {
	//Enable write status register
	perform_single_command(0x50);

	//Write status register
	uint8_t req[] = { 0x01, status };
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, req, 2, 1000);
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

/*
 * Disables writes.
 */
static void flash_write_disable() {
	perform_single_command(0x04);
}

/*
 * Enables writes.
 */
static void flash_write_enable() {
	perform_single_command(0x06);
}

/*
 * Erases the flash. Waits for operation to finish.
 */
void flash_erase() {
	//Enable write
	flash_write_enable();

	//Send erase command
	perform_single_command(0x60);

	//Disable write
	flash_write_disable();

	//Wait for busy flag to be reset
	wait_for_unbusy();
}

/*
 * Erases a 4 kB sector.
 * sector: The sector index (NOT the physical address).
 */
void flash_erase_sector(int sector) {
	//Enable write
	flash_write_enable();

	//Convert to physical address
	sector *= FLASH_SECTOR_SIZE;

	//Write the command
	uint8_t cmd[4] = {
			0x20,
			(sector >> 16) & 0xFF,
			(sector >> 8) & 0xFF,
			sector & 0xFF
	};
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, cmd, 4, 1000);
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

	//Disable write
	flash_write_disable();

	//Wait for busy flag to be reset
	wait_for_unbusy();
}

/*
 * Reads from the flash.
 * address: Address to read from.
 * result: Output buffer to write into.
 * len: Length to read.
 */
void flash_read(uint32_t address, uint8_t* result, int len) {
	uint8_t cmd[4] = {
			0x03,
			(address >> 16) & 0xFF,
			(address >> 8) & 0xFF,
			address & 0xFF
	};
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, cmd, 4, 1000);
	HAL_SPI_Receive(&hspi1, result, len, 1000);
	HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

/*
 * Writes to the flash.
 * address: Address to write to.
 * data: Data to write from.
 * len: Number of bytes to write.
 */
void flash_write(uint32_t address, const uint8_t* data, int len) {
	uint8_t cmd[5];
	cmd[0] = 0x02;
	for (int i = 0; i < len; i++) {
		//Set address
		cmd[1] = (address >> 16) & 0xFF;
		cmd[2] = (address >> 8) & 0xFF;
		cmd[3] = address & 0xFF;

		//Set payload
		cmd[4] = data[i];

		//Enable write
		flash_write_enable();

		//Transmit
		HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi1, cmd, 5, 1000);
		HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

		//Wait
		wait_for_unbusy();

		//Increment
		address++;
	}

	//Disable write
	flash_write_disable();
}

/*
 * Changes properties of flash to remove protections.
 */
void flash_unprotect() {
	flash_write_status(0);
}

/*
 * Utility function, performs a CRC32; Modified from https://web.archive.org/web/20190108202303/http://www.hackersdelight.org/hdcodetxt/crc.c.txt
 * buffer: Buffer to perform actions on
 * len: Length of buffer
 * crcPos: Location where the CRC is stored within the buffer, or -1 to disable. Bytes are subsistuted as 0 at this plus four bytes.
 */
uint32_t flash_calc_crc(uint8_t* buffer, int len, int crcPos) {
	int j;
	uint32_t crc = 0xFFFFFFFF;
	uint32_t mask;
	int offset = 0;
	uint8_t read;
	for (int i = 0; i < len; i++)
	{
		//Read from buffer
		read = buffer[offset];

		//If offset is between crcPos and crcPos+3, subsistute zero
		if (crcPos != -1 && offset >= crcPos && offset < crcPos + 4)
			read = 0;

		//Update index
		offset++;

		//Perform CRC
	    crc = crc ^ read;
	    for (j = 7; j >= 0; j--)
	    {
	    	// Do eight times.
	        mask = -(crc & 1);
	        crc = (crc >> 1) ^ (0xEDB88320 & mask);
	    }
	}
	return ~crc;
}
