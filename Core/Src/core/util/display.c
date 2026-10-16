// Driver for ST7066U display controller, 4-bit mode

#include <core/util/display.h>
#include <core/util/fs.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "stm32c0xx_hal.h"
#include "main.h"

/*
 * PINOUT, PB0:
 * 0-3 : D0-3
 * 4   : R/W
 * 5   : E1
 * 6   : RS
 * 7   : E2
 */

#define DISP_DATA_MASK 0b1111
#define DISP_PIN_RS (1 << 6)
#define DISP_GPIO_PORT GPIOB
#define DISP_OP_WAIT 1
#define DISP_WIDTH 40

#define DISP_BITBANG_DELAY 100 /* can likely go faster but this is good enough */

static uint64_t custom_chars[16]; // Custom characters loaded on the display, used for screenshot saving

/*
 * Waits a short amount of time for operations.
 * us: Delay in uS
 */
static void disp_delay_us(uint16_t us) {
	//The delay uses TIM17 - Operating with prescaler equal to processor freq in MHz. This makes each tick of the timer = 1 uS

	//Reset the timer
	__HAL_TIM_SET_COUNTER(&htim17, 0);

	//Wait for it to reach the value
	while (__HAL_TIM_GET_COUNTER(&htim17) < us);
}

/*
 * Writes a nibble of data to controller.
 * data: 4-bit nibble ALREADY MASKED.
 * rs: Boolean - Instruction/Data register select.
 * ctrlE: E1 or E2 to assert to pick which controller (or both) to interact with.
 */
void disp_raw_write_nibble(uint8_t data, int rs, int ctrlE) {
	//Clear all pins
	HAL_GPIO_WritePin(DISP_GPIO_PORT, 0xFF, GPIO_PIN_RESET);

	//Write nibble of data
	uint16_t pins = data | ctrlE;
	if (rs)
		pins |= DISP_PIN_RS;
	HAL_GPIO_WritePin(DISP_GPIO_PORT, pins, GPIO_PIN_SET);

	//Wait for execution and then reset enable bit
	disp_delay_us(DISP_BITBANG_DELAY);

	//Reset enable
	HAL_GPIO_WritePin(DISP_GPIO_PORT, DISP_PIN_EX, GPIO_PIN_RESET);
}

/*
 * Writes data to the controller.
 * data: 8-bit value to be written.
 * rs: Boolean - Instruction/Data register select.
 * ctrlE: E1 or E2 to assert to pick which controller (or both) to interact with.
 */
void disp_raw_write(uint8_t data, int rs, int ctrlE) {
	//Write each nibble
	disp_raw_write_nibble(data >> 4, rs, ctrlE);
	disp_delay_us(DISP_BITBANG_DELAY);
	disp_raw_write_nibble(data & 0xF, rs, ctrlE);
}

/*
 * Transfers data to the controller once the address has already been set.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * data: Data to read from.
 * len: Number of bytes to transmit.
 */
static void disp_transfer_mem(int ctrlE, const char* data, int len) {
	for (int i = 0; i < len; i++) {
		//Write and perform wait
		disp_raw_write(data[i], 1, ctrlE);
		disp_delay_us(80); // 37 uS min
	}
}

/* LOW-LEVEL PUBLIC API */

/*
 * Clears the display directly, skipping the buffer.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 */
void disp_clear_ll(int ctrlE) {
	disp_raw_write(0b00000001, 0, ctrlE);
	HAL_Delay(2); // 1.52 ms min
}

/*
 * Returns home.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 */
void disp_return_home(int ctrlE) {
	disp_raw_write(0b00000010, 0, ctrlE);
	HAL_Delay(2); // 1.52 ms min
}

/*
 * Configures entry mode of the display.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * inc: Flag; If set, cursor/RAM pointer advances by 1, otherwise it is subtracted by 1
 * shift: Flag; If set, perform shift of display.
 */
void disp_set_entry_mode(int ctrlE, int inc, int shift) {
	//Prepare command
	uint8_t cmd = 0b00000100;
	cmd |= (inc & 1) << 1;
	cmd |= (shift & 1);

	//Write to display
	disp_raw_write(cmd, 0, ctrlE);
	disp_delay_us(40); // 37 uS min
}

/*
 * Configures display on/off as well as cursor settings.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * dispOn: Flag; If set, turn on display.
 * cursorOn: Flag; If set, cursor is on.
 * cursorBlink: Flag; If set, cursor blinks.
 */
void disp_set_power(int ctrlE, int dispOn, int cursorOn, int cursorBlink) {
	//Prepare command
	uint8_t cmd = 0b00001000;
	cmd |= (dispOn & 1) << 2;
	cmd |= (cursorOn & 1) << 1;
	cmd |= (cursorBlink & 1);

	//Write to display
	disp_raw_write(cmd, 0, ctrlE);
	disp_delay_us(40); // 37 uS min
}

/*
 * Shifts either the display or cursor left or right.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * shiftDisp: Flag; If set, shifts display, otherwise shifts cursor.
 * shiftRight: Flag; If set, shifts display/cursor right, otherwise shifts it left.
 */
void disp_shift(int ctrlE, int shiftDisp, int shiftRight) {
	//Prepare command
	uint8_t cmd = 0b00010000;
	cmd |= (shiftDisp & 1) << 3;
	cmd |= (shiftRight & 1) << 2;

	//Write to display
	disp_raw_write(cmd, 0, ctrlE);
	disp_delay_us(40); // 37 uS min
}

/*
 * Configures display functionality. Since driver is only 4-bit mode, forces this mode to always be set.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * dispLineNum: Flag; If set, configures controller for 2-line mode. Otherwise sets up for 1-line mode.
 * fontHeight: Flag; If set, configures for 5x11 font, otherwise 5x8 font.
 */
void disp_set_func(int ctrlE, int dispLineNum, int fontHeight) {
	//Prepare command
	uint8_t cmd = 0b00100000;
	cmd |= (dispLineNum & 1) << 3;
	cmd |= (fontHeight & 1) << 2;

	//Write to display
	disp_raw_write(cmd, 0, ctrlE);
	disp_delay_us(40); // 37 uS min
}

/*
 * Writes data directly to CGRAM (character generator RAM).
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * addr: 6-bit address to write to.
 * data: Data to transfer.
 * len: Number of bytes to transfer.
 */
void disp_ll_write_cgram(int ctrlE, int addr, const char* data, int len) {
	//Prepare command
	uint8_t cmd = 0b01000000;
	cmd |= addr & 0b111111;

	//Write to display
	disp_raw_write(cmd, 0, ctrlE);
	disp_delay_us(40); // 37 uS min

	//Perform transfer
	disp_transfer_mem(ctrlE, data, len);
}

/*
 * Writes data directly to DDRAM (data RAM).
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * addr: 7-bit address to write to.
 * data: Data to transfer.
 * len: Number of bytes to transfer.
 */
void disp_ll_write_ddram(int ctrlE, int addr, const char* data, int len) {
	//Prepare command
	uint8_t cmd = 0b10000000;
	cmd |= addr & 0b1111111;

	//Write to display
	disp_raw_write(cmd, 0, ctrlE);
	disp_delay_us(40); // 37 uS min

	//Perform transfer
	if (len > 0)
		disp_transfer_mem(ctrlE, data, len);
}

/* HIGH-LEVEL PUBLIC API */

#define CANVAS_SIZE 160

static char disp_canvas[CANVAS_SIZE]; // Temporary location where rendering is done to
static int disp_canvas_cursor = 0; // Cursor in the canvas
static char disp_canvas_uploaded[CANVAS_SIZE]; // The canvas that the display currently has

/*
 * Clears the buffer.
 */
void disp_clear() {
	for (int i = 0; i < CANVAS_SIZE; i++)
		disp_canvas[i] = ' ';
}

/*
 * Clears just a single line.
 * lineNum: The line number, 0-3, to use.
 */
void disp_clear_line(int lineNum) {
	lineNum *= 40;
	for (int i = 0; i < 40; i++)
		disp_canvas[lineNum + i] = ' ';
}

/*
 * Sets the row/column of the current memory cursor.
 * row: Row 0-3
 * col: Column 0-39
 */
void disp_set_cursor(int row, int col) {
	//Update
	disp_canvas_cursor = (row * 40) + col;
}

/*
 * Writes text at the current memory location.
 * data: Data to write
 * len: Number of characters to write if >= 0; IF -1, automatically writes to null terminator.
 */
void disp_write_text(const char* data, int len) {
	//If len == -1, automatically read to null terminator
	if (len == -1) {
		len = 0;
		while (data[len] != 0)
			len++;
	}

	//Copy
	for (int i = 0; i < len; i++) {
		assert(disp_canvas_cursor >= 0 && disp_canvas_cursor < CANVAS_SIZE);
		disp_canvas[disp_canvas_cursor++] = data[i];
	}
}

/*
 * Writes a single character to the cursor location.
 * data: The single character to write.
 */
void disp_write_char(char data) {
	//Perform write
	disp_write_text(&data, 1);
}

/*
 * Writes a line of data to the display, automatically choosing a controller. You should avoid spanning lines.
 * row: Row 0-3
 * col: Column 0-39
 * data: Data to write
 * len: Number of characters to write if >= 0; IF -1, automatically writes to null terminator.
 */
void disp_write_text_at(int row, int col, const char* data, int len) {
	//Move cursor
	disp_set_cursor(row, col);

	//Write
	disp_write_text(data, len);
}

/*
 * Flushes only a specific line, 0-3
 */
void disp_flush_line(int line) {
	//Calculate parameters
	int e = line <= 1 ? DISP_PIN_E1 : DISP_PIN_E2;
	int addr = line % 2 == 0 ? 0 : 0x40;
	int offset = line * 40;

	//Check for the first change
	int deltaStart = 0;
	while (disp_canvas[offset + deltaStart] == disp_canvas_uploaded[offset + deltaStart]) {
		deltaStart++;
		if (deltaStart == DISP_WIDTH)
			return; // no changes to be made
	}

	//Find the length of changes
	int deltaLen = 1;
	int curIndex = 1; // since we're sending a length, make it start at 1 not 0
	for (int i = deltaStart; i < DISP_WIDTH; i++) {
		if (disp_canvas[offset + i] != disp_canvas_uploaded[offset + i]) {
			deltaLen = curIndex;
		}
		curIndex++;
	}

	//Write to display
	disp_ll_write_ddram(e, addr + deltaStart, &disp_canvas[offset + deltaStart], deltaLen);

	//Copy to internal buffer
	memcpy(&disp_canvas_uploaded[offset + deltaStart], &disp_canvas[offset + deltaStart], deltaLen);
}

/*
 * Fluses unsaved changes to the buffer.
 */
void disp_flush() {
	//For now, just copy all
	for (int i = 0; i < 4; i++)
		disp_flush_line(i);
}

/*
 * Writes an 8-byte glyph to CGRAM. Assumes 5x8 characters.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * index: The character index to use. There are 8 available slots.
 * data: Character data.
 */
void disp_upload_font(int ctrlE, int index, uint64_t data) {
	//Upload to display
	disp_ll_write_cgram(ctrlE, index << 3, (char*)&data, 8);

	//Store in internal buffer for saving screenshots
	if (ctrlE & DISP_PIN_E1)
		custom_chars[index] = data;
	if (ctrlE & DISP_PIN_E2)
		custom_chars[index + 8] = data;
}

/*
 * Renders a test page to the display.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * iterations: Number of times to run the function, or -1 to run indefinately
 * delay: Delay between each page, in milliseconds; May be 0
 */
void disp_render_test(int ctrlE, int iterations, int delay) {
	int iteration = 0;
	char buffer[80];
	uint8_t nextChar = 0;
	while (iterations == -1 || iteration++ < iterations) {
		//Fill buffer
		for (int i = 0; i < 80; i++)
			buffer[i] = (char)nextChar++;

		//Write both lines
		disp_ll_write_ddram(ctrlE, 0x00, &buffer[0], 40);
		disp_ll_write_ddram(ctrlE, 0x40, &buffer[40], 40);

		//Delay
		if (delay > 0)
			HAL_Delay(delay);
	}
}

/*
 * Initializes the display to a known-good initial configuration.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * warm: Set to skip first time initialization if this is believed to be a warm reset
 */
void disp_init(int ctrlE, int warm) {
	//Turn on backlight
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

	//Make sure the timer, used for delays, is enabled
	HAL_TIM_Base_Start(&htim17);

	//Skip warm setup steps
	if (!warm) {
		//Initial "wake up"; Not sure if needed
		disp_raw_write_nibble(0b0010, 0, ctrlE);
		HAL_Delay(40);

		//Initialize function; Documentation shows this twice, not sure if needed.
		disp_set_func(ctrlE, 1, 0);
		HAL_Delay(60);
		disp_set_func(ctrlE, 1, 0);
		HAL_Delay(60);
	}

	//Set power settings
	disp_set_power(ctrlE, 1, 0, 0);
	HAL_Delay(60);

	//Clear display
	disp_clear_ll(ctrlE);
	HAL_Delay(60);

	//Confiure scrolling
	disp_set_entry_mode(ctrlE, 1, 0);
	HAL_Delay(60);
	disp_shift(DISP_PIN_EX, 0, 1);
	HAL_Delay(60);

	//Finally, return home
	disp_return_home(ctrlE);
	HAL_Delay(60);
}

/*
 * Saves the current display to a file. Returns a FS_ERROR_* value.
 * outputFileIndex: Set to the FAT file index. Only valid if FS_ERROR_OK is returned
 */
int disp_capture_screenshot(int* outputFileIndex) {
	//Scan display for first non-control, non-space character to use as a hint
	char hint[8];
	memset(hint, 0, sizeof(hint));
	int copy = -1;
	for (int i = 0; i < CANVAS_SIZE && copy < 8; i++) {
		if (disp_canvas[i] > ' ' && copy == -1)
			copy = 0;
		if (disp_canvas[i] >= ' ' && copy >= 0)
			hint[copy++] = disp_canvas[i];
	}

	//Open a new file
	int status = fs_file_create(FS_FILE_TYPE_SCREENSHOT, hint, outputFileIndex);
	if (status != FS_ERROR_OK)
		return status;

	//Copy canvas data
	status = fs_file_write((uint8_t*)disp_canvas, CANVAS_SIZE);
	if (status != FS_ERROR_OK)
		return status;

	//Write each CGRAM character
	for (int i = 0; i < 16; i++) {
		int status = fs_file_write((uint8_t*)&custom_chars[i], sizeof(uint64_t));
		if (status != FS_ERROR_OK)
			return status;
	}


	//Close file
	return fs_file_close(0);
}

/*
 * Captures a new screenshot and displays a message
 */
void disp_capture_screenshot_interactive() {
	//Attempt to save
	int fileIndex;
	int status = disp_capture_screenshot(&fileIndex);

	//Save first row data as well as cursor
	char temp[40];
	int tempCursor = disp_canvas_cursor;
	memcpy(temp, disp_canvas, sizeof(temp));

	//Format a message to write
	char msg[40];
	if (status == FS_ERROR_OK) {
		//Get name
		char name[9];
		memcpy(name, fs_fat.fat[fileIndex].hint, 8);
		name[8] = 0;

		//Write message
		sprintf(msg, "Screenshot saved to \"%s\" (%i)", name, fs_fat.fat[fileIndex].file_id);
	} else {
		//Write error
		sprintf(msg, "Screenshot save failed: %i", status);
	}

	//Write to display
	disp_write_text_at(0, 0, "*", 1);
	for (int i = 0; i < 38; i++)
		disp_write_char(' ');
	disp_write_text_at(0, 2, msg, -1);
	disp_write_text_at(0, 39, "*", 1);
	disp_flush();

	//Delay
	HAL_Delay(5000);

	//Return old data
	disp_canvas_cursor = tempCursor;
	memcpy(disp_canvas, temp, sizeof(temp));
	disp_flush();
}
