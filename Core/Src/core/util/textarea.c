#include <core/util/textarea.h>
#include <core/util/display.h>
#include <assert.h>

#define TEXTAREA_MAX_WIDTH 40

static char textarea_buffer[TEXTAREA_MAX_WIDTH]; // Working buffer

/*
 * Initializes a text area.
 * info: Pointer to the (uninitialized) textarea struct.
 * row: Row to render on
 * col: Column to render on
 * width: Width of textarea
 * height: Height of textarea
 */
void textarea_init(textarea_data_t* info, int row, int col, int width, int height) {
	//Set defaults
	info->row = row;
	info->col = col;
	info->width = width;
	info->height = height;
	info->flags = 0;

	//Validate
	assert(width <= TEXTAREA_MAX_WIDTH);

	//Clear out data
	for (int i = 0; i < TEXTAREA_BUFFER_SIZE; i++)
		info->text[i] = 0;
	info->text_length = 0;
}

/*
 * Sets the text of a textarea from a string. Copies data into the struct.
 * info: Pointer to a textarea struct
 * data: The data to copy in, up to TEXTAREA_BUFFER_SIZE or null terminator.
 */
void textarea_copy_text(textarea_data_t* info, const char* data) {
	//Calculate length, maxing out at TEXTAREA_BUFFER_SIZE
	int len = 0;
	while (data[len] != 0 && len < TEXTAREA_BUFFER_SIZE)
		len++;

	//Perform copy
	textarea_copy_text_length(info, data, len);
}

/*
 * Sets the text of a textarea from a string using a specified length. Copies data into the struct.
 * info: Pointer to a textarea struct
 * data: The data to copy in, up to TEXTAREA_BUFFER_SIZE or length;
 * len: The length to copy.
 */
void textarea_copy_text_length(textarea_data_t* info, const char* data, int len) {
	//Cap length
	if (len > TEXTAREA_BUFFER_SIZE)
		len = TEXTAREA_BUFFER_SIZE;

	//Copy data and set length
	info->text_length = len;
	for (int i = 0; i < len; i++)
		info->text[i] = data[i];

	//Render
	textarea_render(info);
}

/*
 * Clears text in the textarea buffer.
 */
void textarea_clear(textarea_data_t* info) {
	//Set length to 0
	info->text_length = 0;

	//Render
	textarea_render(info);
}

/*
 * Utility function that clears out len of the buffer with spaces.
 */
static void clear_textarea_buffer(int len) {
	for (int i = 0; i < len; i++)
		textarea_buffer[i] = ' ';
}

/*
 * Scans up to remaining for the next space. Returns the position after the space
 */
static int scan_for_next_word(const char* buffer, int remaining) {
	for (int i = 0; i < remaining; i++) {
		if (buffer[i] == ' ')
			return i + 1;
	}
	return remaining;
}

/*
 * Renders the textarea to the display.
 * info: Pointer to a textarea struct
 */
void textarea_render(textarea_data_t* info) {
	//Chop up into lines
	int charIndex = 0;
	for (int line = 0; line < info->height; line++) {
		//Clear out buffers with spaces
		clear_textarea_buffer(info->width);

		//Skip leading spaces
		while (info->text[charIndex] == ' ' && charIndex < info->text_length)
			charIndex++;

		//Fill up buffer with text as long there is text available
		if (charIndex < info->text_length) {
			//Find words that'll fit
			int lineLen = 0;
			int lineRemaining = info->width;
			while (1) {
				//Find the next space
				int scanOffset = charIndex + lineLen;
				if (scanOffset >= info->text_length)
					break;
				int wordLen = scan_for_next_word(&info->text[scanOffset], info->text_length - scanOffset);

				//Check if it will fit
				if (wordLen <= lineRemaining) {
					//Add word; update state
					lineLen += wordLen;
					lineRemaining -= wordLen;
				} else if (lineLen == 0) {
					//While we technically should break, this is the first word, so cut it
					lineLen += lineRemaining;
					lineRemaining = 0;
				} else {
					//Break to next line
					break;
				}
			}

			//Trim trailing spaces for sake of rendering
			while (lineLen > 0 && info->text[charIndex + lineLen - 1] == ' ')
				lineLen--;

			//From alignment determine the start write location
			int lineStart = 0; // defaults to left
			if (info->flags & TEXTAREA_FLAG_ALIGN_RIGHT)
				lineStart = info->width - lineLen;
			else if (info->flags & TEXTAREA_FLAG_ALIGN_CENTER)
				lineStart = (info->width - lineLen) / 2;

			//Copy into text buffer, respecting alignemnt
			for (int i = 0; i < lineLen; i++)
				textarea_buffer[lineStart + i] = info->text[charIndex + i];

			//Update state
			charIndex += lineLen;
		}

		//Render to display
		disp_write_text_at(info->row + line, info->col, textarea_buffer, info->width);
	}
}
