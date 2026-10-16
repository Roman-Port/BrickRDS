#ifndef INC_CORE_UTIL_TEXTAREA_H_
#define INC_CORE_UTIL_TEXTAREA_H_

#define TEXTAREA_BUFFER_SIZE 80
#define TEXTAREA_FLAG_ALIGN_RIGHT (1 << 0)
#define TEXTAREA_FLAG_ALIGN_CENTER (1 << 1)

// Data for a text area
typedef struct textarea_data {

	int row;
	int col;
	int width;
	int height;
	int flags;

	char text[TEXTAREA_BUFFER_SIZE];
	int text_length;

} textarea_data_t;

/*
 * Initializes a text area.
 * info: Pointer to the (uninitialized) textarea struct.
 * row: Row to render on
 * col: Column to render on
 * width: Width of textarea
 * height: Height of textarea
 */
void textarea_init(textarea_data_t* info, int row, int col, int width, int height);

/*
 * Sets the text of a textarea from a string. Copies data into the struct.
 * info: Pointer to a textarea struct
 * data: The data to copy in, up to TEXTAREA_BUFFER_SIZE or null terminator.
 */
void textarea_copy_text(textarea_data_t* info, const char* data);

/*
 * Sets the text of a textarea from a string using a specified length. Copies data into the struct.
 * info: Pointer to a textarea struct
 * data: The data to copy in, up to TEXTAREA_BUFFER_SIZE or length;
 * len: The length to copy.
 */
void textarea_copy_text_length(textarea_data_t* info, const char* data, int len);

/*
 * Clears text in the textarea buffer.
 */
void textarea_clear(textarea_data_t* info);

/*
 * Renders the textarea to the display.
 * info: Pointer to a textarea struct
 */
void textarea_render(textarea_data_t* info);


#endif /* INC_CORE_UTIL_TEXTAREA_H_ */
