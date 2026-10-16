#ifndef INC_CORE_UTIL_DISPLAY_H_
#define INC_CORE_UTIL_DISPLAY_H_

#include <stdint.h>

#define DISP_PIN_E1 (1 << 5)
#define DISP_PIN_E2 (1 << 7)
#define DISP_PIN_EX (DISP_PIN_E1 | DISP_PIN_E2) /* both E1 and E2 */

/* LOW-LEVEL PUBLIC API */

/*
 * Writes a nibble of data to controller.
 * data: 4-bit nibble ALREADY MASKED.
 * rs: Boolean - Instruction/Data register select.
 * ctrlE: E1 or E2 to assert to pick which controller (or both) to interact with.
 */
void disp_raw_write_nibble(uint8_t data, int rs, int ctrlE);

/*
 * Writes data to the controller.
 * data: 8-bit value to be written.
 * rs: Boolean - Instruction/Data register select.
 * ctrlE: E1 or E2 to assert to pick which controller (or both) to interact with.
 */
void disp_raw_write(uint8_t data, int rs, int ctrlE);

/*
 * Clears the display directly, skipping the buffer
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 */
void disp_clear_ll(int ctrlE);

/*
 * Returns home.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 */
void disp_return_home(int ctrlE);

/*
 * Configures entry mode of the display.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * inc: Flag; If set, cursor/RAM pointer advances by 1, otherwise it is subtracted by 1
 * shift: Flag; If set, perform shift of display.
 */
void disp_set_entry_mode(int ctrlE, int inc, int shift);

/*
 * Configures display on/off as well as cursor settings.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * dispOn: Flag; If set, turn on display.
 * cursorOn: Flag; If set, cursor is on.
 * cursorBlink: Flag; If set, cursor blinks.
 */
void disp_set_power(int ctrlE, int dispOn, int cursorOn, int cursorBlink);

/*
 * Shifts either the display or cursor left or right.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * shiftDisp: Flag; If set, shifts display, otherwise shifts cursor.
 * shiftRight: Flag; If set, shifts display/cursor right, otherwise shifts it left.
 */
void disp_shift(int ctrlE, int shiftDisp, int shiftRight);

/*
 * Configures display functionality. Since driver is only 4-bit mode, forces this mode to always be set.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * dispLineNum: Flag; If set, configures controller for 2-line mode. Otherwise sets up for 1-line mode.
 * fontHeight: Flag; If set, configures for 5x11 font, otherwise 5x8 font.
 */
void disp_set_func(int ctrlE, int dispLineNum, int fontHeight);

/*
 * Writes data directly to CGRAM (character generator RAM).
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * addr: 6-bit address to write to.
 * data: Data to transfer.
 * len: Number of bytes to transfer.
 */
void disp_ll_write_cgram(int ctrlE, int addr, const char* data, int len);

/*
 * Writes data directly to DDRAM (data RAM).
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * addr: 7-bit address to write to.
 * data: Data to transfer.
 * len: Number of bytes to transfer.
 */
void disp_ll_write_ddram(int ctrlE, int addr, const char* data, int len);

/* HIGH-LEVEL PUBLIC API */

/*
 * Clears the buffer.
 */
void disp_clear();

/*
 * Clears just a single line.
 * lineNum: The line number, 0-3, to use.
 */
void disp_clear_line(int lineNum);

/*
 * Sets the row/column of the current memory cursor.
 * row: Row 0-3
 * col: Column 0-39
 */
void disp_set_cursor(int row, int col);

/*
 * Writes text at the current memory location.
 * data: Data to write
 * len: Number of characters to write if >= 0; IF -1, automatically writes to null terminator.
 */
void disp_write_text(const char* data, int len);

/*
 * Writes a single character to the cursor location.
 * data: The single character to write.
 */
void disp_write_char(char data);

/*
 * Writes a line of data to the display, automatically choosing a controller. You should avoid spanning lines.
 * row: Row 0-3
 * col: Column 0-39
 * data: Data to write
 * len: Number of characters to write if >= 0; IF -1, automatically writes to null terminator.
 */
void disp_write_text_at(int row, int col, const char* data, int len);

/*
 * Flushes only a specific line, 0-3
 */
void disp_flush_line(int line);

/*
 * Fluses unsaved changes to the buffer.
 */
void disp_flush();

/*
 * Writes an 8-byte glyph to CGRAM. Assumes 5x8 characters.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * index: The character index to use. There are 8 available slots.
 * data: Character pixel data.
 */
void disp_upload_font(int ctrlE, int index, uint64_t data);

/*
 * Renders a test page to the display.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * iterations: Number of times to run the function, or -1 to run indefinately
 * delay: Delay between each page, in milliseconds; May be 0
 */
void disp_render_test(int ctrlE, int iterations, int delay);

/*
 * Initializes the display to a known-good initial configuration.
 * ctrlE: E1/E2 (or both) defining controller to interact with.
 * warm: Set to skip first time initialization if this is believed to be a warm reset
 */
void disp_init(int ctrlE, int warm);

/*
 * Saves the current display to a file. Returns a FS_ERROR_* value.
 * outputFileIndex: Set to the FAT file index. Only valid if FS_ERROR_OK is returned
 */
int disp_capture_screenshot(int* outputFileIndex);

/*
 * Captures a new screenshot and displays a message
 */
void disp_capture_screenshot_interactive();

#endif /* INC_CORE_UTIL_DISPLAY_H_ */
