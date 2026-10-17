#include <stdint.h>
#include "core/util/charset.h"
#include "core/util/display.h"

static int loaded_charsets[2]; // The charsets currently loaded on the display segments.

// The available charsets; Must have all 8 in each
static const uint64_t charsets[][8] = {
		/* CHARSET_MAIN_UPPER - Main charset used for the main PS/RT screen. */
		{
				0b0000000000000100000001000000010000001110000110110001000100011111, // Tower icon
				0b0000000000011011000110110000001100000011000000000000000000000000, // LSignal - 3/3
				0b0000000000011000000110000001100000011000000110000001100000000000, // RSignal - 3/3
				0b0000000000011011000110000000000000000000000000000000000000000000, // LSignal - 1/3
				0b0000000000011011000000000000000000000000000000000000000000000000, // LSignal - 0/3
				0b0000000000011000000000000000000000000000000000000000000000000000, // RSignal - <3/3
				0b0000000000000000000000000000000000000000000000000000000000010101, // Dotted line below the header
				0b0000000000000000000000000000000000000000000000000000000000000000  // Empty
		},
		/* CHARSET MAIN LOWER - Main charset used for lower alf of PS/RT screen */
		{
				0b0000000000000000000000000000000000000000000000000000000000000000,
				0b0000000000000000000000000000000000000000000000000000000000000000,
				0b0000000000011000000010000000100100001000000010010000100000011000,
				0b0000000000001110000110110001001100010011000100110001101100001110,
				0b0000000000001110000110110001100100011001000110010001101100001110,
				0b0000000000000000000000000000000000000000000000000000000000000000,
				0b0000000000000000000000000000000000000000000000000000000000000000,
				0b0000000000011111000110110001000100000000000110110001101100011011
		},
		/* BACKLIGHT CONFIG */
		{
				0b0001111100000000000000000000000000000000000000000000000000000000,
				0b0001110000000100000001110000000000000000000000000000000000000000,
				0b0000000000000000000111110000000000000000000000000000000000000000,
				0b0000000000000000000111000000010000000111000000000000000000000000,
				0b0000000000000000000000000000000000011100000001000000010000000111,
				0b0000000000000000000000000000000000000000000000000000000000011111,
				0b0000000000000100000011100001111100000000000000000000000000000000,
				0b0000000000000000000000000000000000011111000011100000010000000000
		},
		/* TUNER CALIBRATION VIEWER */
		{
				0b0000000000001000000010000000100000001111000010000000100000001000,
				0b0000000000000000000000000000000000011111000000000000000000000000,
				0b0000000000000010000000100000001000011110000000100000001000000010,
				0b0000000000000100000001000000010000011111000001000000010000000100,
				0b0000000000011111000011100000010000011111000001000000010000000100,
				0b0000000000011111000011100000010000011111000000000000000000000000,
				0b0000000000001000000111110000111000001111000011000000100000001000,
				0b0000000000000010000111110000111000011110000001100000001000000010
		}
};

/*
 * Updates a controller without doing any clearing.
 * displayIndex: The display to use, 0-1
 * e: The E pin to assert
 * charsetIndex: The new charset index
 */
static void update_contorller_charset(int displayIndex, int e, int charsetIndex) {
	//Update the stored state
	loaded_charsets[displayIndex] = charsetIndex;

	//If >= 0, load the designated charset
	if (charsetIndex >= 0) {
		for (int i = 0; i < 8; i++)
			disp_upload_font(e, i, charsets[charsetIndex][i]);
	}
}

/*
 * Updates the current charset of the display controller.
 * upperCharsetIndex: Charset index for upper controller OR -1 for no change
 * lowerCharsetIndex: Charset index for lower controller OR -1 for no change
 */
void charset_change(int upperCharsetIndex, int lowerCharsetIndex) {
	//Create the masks of displays to be updated observing changes supplied to this and if the charset is currently correct already
	int dispsToUpdate = 0;
	if (upperCharsetIndex > -1 && loaded_charsets[0] != upperCharsetIndex)
		dispsToUpdate |= 0b01;
	if (lowerCharsetIndex > -1 && loaded_charsets[1] != lowerCharsetIndex)
		dispsToUpdate |= 0b10;

	//Clear displays to be updated automatically to prevent corrupted characters from appearing
	if (dispsToUpdate & 0b01) {
		//Clear+flush upper half
		disp_clear_line(0);
		disp_flush_line(0);
		disp_clear_line(1);
		disp_flush_line(1);
	}
	if (dispsToUpdate & 0b10) {
		//Clear+flush lower half
		disp_clear_line(2);
		disp_flush_line(2);
		disp_clear_line(3);
		disp_flush_line(3);
	}

	//Upload changes
	if (dispsToUpdate & 0b01)
		update_contorller_charset(0, DISP_PIN_E1, upperCharsetIndex);
	if (dispsToUpdate & 0b10)
		update_contorller_charset(1, DISP_PIN_E2, lowerCharsetIndex);
}

/*
 * Marks the specified display as an invalid charset so next time a change is requested it will always be uploaded.
 * Will not modify display data.
 */
void charset_mark_dirty(int display) {
	//Update the stored state
	loaded_charsets[display] = -1;
}
