#ifndef INC_CORE_UTIL_CHARSET_H_
#define INC_CORE_UTIL_CHARSET_H_

// Displays
#define CHARSET_DISPLAY_UPPER 0
#define CHARSET_DISPLAY_LOWER 1

// Charset indicies
#define CHARSET_MAIN_UPPER 0
#define CHARSET_MAIN_LOWER 1
#define CHARSET_BACKLIGHT 2

/*
 * Updates the current charset of the display controller.
 * upperCharsetIndex: Charset index for upper controller OR -1 for no change
 * lowerCharsetIndex: Charset index for lower controller OR -1 for no change
 */
void charset_change(int upperCharsetIndex, int lowerCharsetIndex);

/*
 * Marks the specified display as an invalid charset so next time a change is requested it will always be uploaded.
 * Will not modify display data.
 */
void charset_mark_dirty(int display);

#endif /* INC_CORE_UTIL_CHARSET_H_ */
