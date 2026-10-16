/* Utility for programming the SPI flash */

/*
 * Communicates with PC application using following protocol. TX/RX packets consist of the following 6-byte header:
 * 0   : 0xFC (sync)
 * 1   : 0xF0 (sync)
 * 2   : 0xC1 (sync)
 * 3   : 0x8F (sync)
 * 4   : Opcode (Computer->MCU) or Response (MCU->Computer) where 0x00 is OK, 1 is error, and 2 is data error
 *       EXCEPT MSB is used to indicate that length is actually 256
 * 5   : Data length - Bytes part of this packet AFTER this header
 */

#include <stdint.h>
#include <core/util/display.h>
#include <core/util/flash.h>
#include <core/util/rds.h>
#include <core/util/fs.h>
#include <core/views/settings/view_pcmode.h>
#include <core/views.h>
#include <core/core.h>
#include "main.h"

#define SERIAL_TIMEOUT 100
#define SERIAL_FIFO_SIZE 300

#define RESULT_CODE_OK 0
#define RESULT_CODE_ERROR 1
#define RESULT_CODE_DATA_ERROR 2

view_info_t view_settings_pcmode = (view_info_t) {
	.cb_open = view_settings_pcmode_open,
	.cb_frame = view_settings_pcmode_tick,
	.cb_close = 0
};

typedef void (*exec_uploaded_func)();

static uint8_t packet_buffer[256]; // Buffer used for storing packet info for both TX and RX
static uint32_t sync_search = 0; // Shift register used for searching for sync
static uint32_t io_address = 0; // The address, automatically incremented, used for reading and writing to flash

/*
 * Scans for the sync code and returns 1 on success, otherwise 0 for timeout
 */
static int find_sync() {
	uint8_t rx;
	 __HAL_UART_CLEAR_IT(&huart1, UART_CLEAR_NEF|UART_CLEAR_OREF);
	while(HAL_UART_Receive(&huart1, &rx, 1, SERIAL_TIMEOUT) == HAL_OK) {
		//Push into sync buffer
		sync_search = (sync_search << 8) | rx;

		//Check if sync matches
		if (sync_search == 0xFCF0C18F) {
			return 1;
		}
	}
	return 0;
}

/*
 * Recieves a packet and writes its info into the buffer. Sets op and len and returns 1 on success, otherwise 0 for timeout.
 */
static int rx_packet(uint8_t* op, int* len) {
	//Search for sync
	if (!find_sync())
		return 0;

	//Read in the rest of the header
	uint8_t header[2];
	if (HAL_UART_Receive(&huart1, header, 2, SERIAL_TIMEOUT) != HAL_OK)
		return 0;

	//Set
	*op = header[0];
	*len = header[1];

	//If MSB of opcode is set, set length to 256
	if ((*op >> 7) == 1)
		*len = 256;

	//Clear the MSB of opcode
	(*op) &= 0x7F;

	//Read packet data. For reasons I don't understand, this only works in chunks
	int remaining = *len;
	int offset = 0;
	while (remaining > 0) {
		//Calculate amount to recieve
		int chunk = 4;
		if (remaining < chunk)
			chunk = remaining;

		//Receive
		if (HAL_UART_Receive(&huart1, &packet_buffer[offset], chunk, SERIAL_TIMEOUT) != HAL_OK)
			return 0;

		//Update state
		remaining -= chunk;
		offset += chunk;
	}

	return 1;
}

/*
 * Decodes a big endian int32 within the data buffer at offset and returns it.
 */
static uint32_t decode_be_int32(int offset) {
	return (((uint32_t)packet_buffer[offset + 0]) << 24) |
			(((uint32_t)packet_buffer[offset + 1]) << 16) |
			(((uint32_t)packet_buffer[offset + 2]) << 8) |
			(((uint32_t)packet_buffer[offset + 3]) << 0);
}

/*
 * Encodes a big endian uint16 at the offset within the packet buffer.
 */
static void encode_be_int16(uint8_t offset, uint16_t value) {
	packet_buffer[offset + 0] = (uint8_t)(value >> 8);
	packet_buffer[offset + 1] = (uint8_t)(value & 0xFF);
}

/*
 * Transmits a packet usin data in the data buffer.
 */
static void tx_packet(uint8_t op, int len) {
	//If len >= 256, set MSB of opcode
	if (len >= 256)
		op |= 1 << 7;

	//Set up header
	uint8_t header[6];
	header[0] = 0xFC;
	header[1] = 0xF0;
	header[2] = 0xC1;
	header[3] = 0x8F;
	header[4] = op;
	header[5] = len;

	//Transmit header
	HAL_UART_Transmit(&huart1, header, 6, HAL_MAX_DELAY);

	//Transmit payload
	HAL_UART_Transmit(&huart1, packet_buffer, len, HAL_MAX_DELAY);
}

/*
 * Handles a PING command.
 */
static void proc_packet_ping(int len) {
	//Cap length at 255
	if (len > 255)
		len = 255;

	//Transmit data back
	tx_packet(RESULT_CODE_OK, len);
}

/*
 * Erases flash chip
 */
static void proc_packet_erase() {
	//Remove protections
	flash_unprotect();

	//Erase flash
	flash_erase();

	//Send result
	tx_packet(RESULT_CODE_OK, 0);
}

/*
 * Sets the read/write address used.
 */
static void proc_packet_set_address(int len) {
	//Make sure length is sensible
	if (len != 4) {
		//Error
		tx_packet(RESULT_CODE_ERROR, 0);
		return;
	}

	//Decode big-endian address sent
	io_address = decode_be_int32(0);

	//Send response
	tx_packet(RESULT_CODE_OK, 0);
}

static void proc_packet_read(int len) {
	//Make sure length is sensible
	if (len != 1) {
		//Error
		tx_packet(RESULT_CODE_ERROR, 0);
		return;
	}

	//Get length sent as it will be clobbered
	int read = packet_buffer[0] + 1;

	//Perform flash read
	flash_read(io_address, packet_buffer, read);

	//Increment address
	io_address += read;

	//Send OK result
	tx_packet(RESULT_CODE_OK, read);
}

static void proc_packet_write(int len) {
	//Make sure length is sensible
	if (len == 0) {
		//Error
		tx_packet(RESULT_CODE_ERROR, 0);
		return;
	}

	//Peform write
	flash_write(io_address, packet_buffer, len);

	//Increment address
	io_address += len;

	//Send OK result
	tx_packet(RESULT_CODE_OK, 0);
}

static void proc_packet_erase_sectors(int len) {
	//Make sure length is sensible
	if (len != 8) {
		//Error
		tx_packet(RESULT_CODE_ERROR, 0);
		return;
	}

	//Get start sector and number of sectors
	uint32_t startSector = decode_be_int32(0);
	uint32_t sectorCount = decode_be_int32(4);

	//Remove protections
	flash_unprotect();

	//Erase flash sectors
	for (uint32_t i = 0; i < sectorCount; i++)
		flash_erase_sector(startSector + i);

	//Send result
	tx_packet(RESULT_CODE_OK, 0);
}

static void proc_packet_read_layout(int len) {
	//Encode information about the flash layout
	encode_be_int16(0, FLASH_SIZE_SECTORS);
	encode_be_int16(2, FLASH_SECTOR_SIZE);
	encode_be_int16(4, FLASH_REGION_SETTINGS_SEC);
	encode_be_int16(6, FLASH_REGION_SETTINGS_LEN);
	encode_be_int16(8, FLASH_REGION_PIDB_SEC);
	encode_be_int16(10, FLASH_REGION_PIDB_LEN);
	encode_be_int16(12, FLASH_REGION_FS_SEC);
	encode_be_int16(14, FLASH_REGION_FS_LEN);
	encode_be_int16(16, sizeof(fs_fat_t));

	//Send result
	tx_packet(RESULT_CODE_OK, 9 * 2);
}

static void proc_packet_delete_file(int len) {
	//Make sure length is sensible
	if (len != 4) {
		//Error
		tx_packet(RESULT_CODE_ERROR, 0);
		return;
	}

	//Get start sector and number of sectors
	uint32_t fileIndex = decode_be_int32(0);

	//Initialize FS and delete
	fs_init();
	int status = fs_file_delete(fileIndex);

	//Create buffer with error code, or any
	packet_buffer[0] = (uint8_t)status;
	tx_packet(status == FS_ERROR_OK ? RESULT_CODE_OK : RESULT_CODE_ERROR, 1);
}

void view_settings_pcmode_open() {
	//Init
	io_address = 0;

	//Disable RDS as it causes interrupts
	rds_disable();

	//Set up display
	disp_clear();
	disp_write_text_at(0, 0, "PC MODE", -1);
	disp_write_text_at(1, 0, "Connect using ACC port at 9600 baud.", -1);
}

void view_settings_pcmode_tick() {
	//Check for back button
	if (core_get_button(BTN_BACK)) {
		//Re-run init
		core_init();
		return;
	}

	//Read packets
	uint8_t op;
	int len;
	while (rx_packet(&op, &len)) {
		switch (op) {
		case 0: /* PING packet */
			proc_packet_ping(len);
			break;
		case 1: /* ERASE packet */
			proc_packet_erase();
			break;
		case 2: /* SET_ADDRESS packet */
			proc_packet_set_address(len);
			break;
		case 3: /* READ packet */
			proc_packet_read(len);
			break;
		case 4: /* WRITE packet */
			proc_packet_write(len);
			break;
		case 5: /* ERASE SECTORS packet */
			proc_packet_erase_sectors(len);
			break;
		case 6: /* READ_FLASH_LAYOUT packet */
			proc_packet_read_layout(len);
			break;
		case 7: /* DELETE_FILE packet */
			proc_packet_delete_file(len);
			break;
		default: /* Unknown opcode; Send back error */
			tx_packet(RESULT_CODE_ERROR, 0);
			break;
		}
	}
}
