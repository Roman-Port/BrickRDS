#include <core/util/menu.h>
#include <core/util/display.h>
#include <core/util/charset.h>
#include <core/core.h>
#include <core/views.h>
#include <string.h>

void view_menu_init();
void view_menu_tick();

view_info_t view_menu = (view_info_t) {
	.cb_open = view_menu_init,
	.cb_frame = view_menu_tick,
	.cb_close = 0
};

typedef struct menu_stack_entry {

	const char* name;
	int selected_index;
	int item_count;
	const menu_item_t* items;

} menu_stack_entry_t;

static view_info_t* menu_prev_view; // The previous view to display when the stack runs out
static menu_stack_entry_t menu_stack[8];
static int menu_stack_index = -1;
static int menu_selected_item = -1;
static int menu_icon_buffer = 0; // 0/1 for double buffering of menu item icons
static int force_menu_reinit = 0; // Set to 1 to force reinitialization on next tick

static const uint64_t icon_checkbox_checked = 0b0000000000000000000111110001101100010101000110110001111100000000;
static const uint64_t icon_checkbox_unchecked = 0b0000000000000000000111110001000100010001000100010001111100000000;

static uint64_t menu_icons[] = {
		0, // 0: Default blank icon
		0b0000000000001000000111000000101000000111000010100001110000001000, // 1: Gears
		0b0000000000010000000100000000100000001000000001100000010100000010, // 2: Wrench
		0b0000000000011111000100010001111100000000000110000001111100011000, // 3: Screwdriver
		0b0000000000011111000100010001000100011111000100010001111100000000, // 4: Toolbox
		0b0000000000000100000001000000101000010001000101010001000100001110, // 5: Pin
		0b0000000000011110000100000001001000010111000100100001001000011110, // 6: Reset
		0b0000000000001110000100010001110100010001000101110001000100001110, // 7: Sponsor block
};

/*
 * Shows the previous menu.
 */
void menu_reshow() {
	//Switch views
	core_change_view(&view_menu);
}

/*
 * Shows a new menu item, pushing it to the stack. Shows the menu if not open.
 * name: The display name.
 * items: Items.
 */
void menu_show(const char* name, const menu_item_t* items) {
	//If bottom of stack, save current view
	if (menu_stack_index == -1) {
		menu_prev_view = current_view;
	}

	//Increment stack cursor
	menu_stack_index++;

	//Determine number of items
	int itemCount = 0;
	while (items[itemCount].type != MENU_ITEM_TYPE_END) {
		itemCount++;
	}

	//Set up stack entry
	menu_stack[menu_stack_index].name = name;
	menu_stack[menu_stack_index].selected_index = 0;
	menu_stack[menu_stack_index].item_count = itemCount;
	menu_stack[menu_stack_index].items = items;

	//Enqueue re-init
	force_menu_reinit = 1;

	//Switch views
	core_change_view(&view_menu);
}

/*
 * Go back a menu.
 */
void menu_back() {
	//Remove the back
	menu_stack_index--;

	//If end of stack, return to previous view
	if (menu_stack_index == -1) {
		//Change views
		core_change_view(menu_prev_view);
	} else {
		//Enqueue reinit
		force_menu_reinit = 1;
	}
}

/*
 * Clears the bottom 3 rows
 */
static void clear_content() {
	for (int r = 1; r < 4; r++) {
		disp_set_cursor(r, 0);
		for (int i = 0; i < 40; i++)
			disp_write_char(' ');
	}
}

/*
 * Renders the icon for a menu item.
 * item: Menu item to render
 * iconSlot: The menu item to use; Display controller chosen automatically
 * row: The row to render at.
 */
static void render_menu_item(const menu_item_t* item, uint8_t iconSlot, int row) {
	//Get the icon to render, handling special cases
	uint64_t icon;
	switch (item->type) {
	case MENU_ITEM_TYPE_CHECKBOX:
		if (item->info.checkbox.read(item->user_context))
			icon = icon_checkbox_checked;
		else
			icon = icon_checkbox_unchecked;
		break;
	case MENU_ITEM_TYPE_BACK:
		icon = 0b0000000000000100000011000001111100001101000001010000000100000001; // Back
		break;
	case MENU_ITEM_TYPE_DIRECTCHECK:
		if (*item->info.direct_check.value)
			icon = icon_checkbox_checked;
		else
			icon = icon_checkbox_unchecked;
		break;
	default:
		//Index into the icon array
		icon = menu_icons[item->icon];
	}

	//Determine display controller
	int disp = DISP_PIN_E1;
	if (row > 1)
		disp = DISP_PIN_E2;

	//Upload to display
	disp_upload_font(disp, iconSlot, icon);

	//Render...
	disp_set_cursor(row, 2);
	disp_write_char(iconSlot);
	disp_write_char(' ');
	disp_write_text(item->text, -1);
}

/*
 * Performs click on the menu item.
 */
static void execute_menu_item(const menu_item_t* item) {
	switch (item->type) {
	case MENU_ITEM_TYPE_BTN:
		item->info.btn.callback(item->user_context);
		menu_selected_item = -1; // force redraw
		break;
	case MENU_ITEM_TYPE_CHECKBOX:
		item->info.checkbox.set(item->user_context, !item->info.checkbox.read(item->user_context));
		menu_selected_item = -1; // force redraw
		break;
	case MENU_ITEM_TYPE_SUBMENU:
		menu_show(item->info.submenu.menu_name, item->info.submenu.items);
		break;
	case MENU_ITEM_TYPE_BACK:
		menu_back();
		break;
	case MENU_ITEM_TYPE_VIEW:
		core_change_view(item->info.view.view);
		break;
	case MENU_ITEM_TYPE_DIRECTCHECK:
			(*item->info.direct_check.value) = !(*item->info.direct_check.value);
			menu_selected_item = -1; // force redraw
			if (item->info.direct_check.callback != 0)
				item->info.direct_check.callback(item->user_context);
			break;
	}
}

void view_menu_init() {
	//Clear display
	disp_clear();
	disp_flush();

	//Upload up arrow to top display controller, and down arrow to bottom controller
	disp_upload_font(DISP_PIN_E1, 0x07, 0b0000000000000100000001000000010000000100000111110000111000000100);
	disp_upload_font(DISP_PIN_E2, 0x07, 0b0000000000000100000011100001111100000100000001000000010000000100);

	//Create the menu title
	char title[40];
	int titleIndex = 0;
	for (int i = 0; i <= menu_stack_index; i++) {
		//Add the " > "
		if (i != 0) {
			title[titleIndex++] = ' ';
			title[titleIndex++] = '>';
			title[titleIndex++] = ' ';
		}

		//Copy the menu list name
		int nameIndex = 0;
		while (menu_stack[i].name[nameIndex] != 0)
			title[titleIndex++] = menu_stack[i].name[nameIndex++];
	}

	//Write the menu title
	disp_write_text_at(0, 0, title, titleIndex);

	//Reset selected item to force redraw
	menu_selected_item = -1;

	//Clear flag
	force_menu_reinit = 0;
}

void view_menu_tick() {
	//Check if reinit is forced
	if (force_menu_reinit)
		view_menu_init();

	//Check inputs
	if (core_get_button(BTN_LEFT) && menu_stack[menu_stack_index].selected_index > 0)
		menu_stack[menu_stack_index].selected_index--;
	else if (core_get_button(BTN_RIGHT))
		menu_stack[menu_stack_index].selected_index++;
	else if (core_get_button(BTN_BACK)) {
		//Go back
		menu_back();

		//Bail out
		return;
	}
	else if (core_get_button(BTN_CLEAR)) {
		//Execute current menu
		execute_menu_item(&menu_stack[menu_stack_index].items[menu_stack[menu_stack_index].selected_index]);

		//Bail out
		return;
	}

	//If current selection is not equal to last, redraw
	if (menu_selected_item != menu_stack[menu_stack_index].selected_index) {
		//Clear content
		clear_content();

		//Set top arrow
		if (menu_stack[menu_stack_index].selected_index > 0) {
			disp_set_cursor(1, 0);
			disp_write_char(0x07); // up arrow
		}

		//Set middle arrow
		disp_set_cursor(2, 0);
		disp_write_char('>');

		//Set bottom arrow
		if (menu_stack[menu_stack_index].selected_index < menu_stack[menu_stack_index].item_count - 1) {
			disp_set_cursor(3, 0);
			disp_write_char(0x07); // down arrow
		}

		//Update state
		menu_selected_item = menu_stack[menu_stack_index].selected_index;

		//Clear display charsets
		charset_mark_dirty(CHARSET_DISPLAY_UPPER);
		charset_mark_dirty(CHARSET_DISPLAY_LOWER);

		//Get offset for charset for double buffering
		uint8_t iconOffset = 0;
		if (menu_icon_buffer)
			iconOffset = 2;
		menu_icon_buffer = !menu_icon_buffer;

		//Render menu items icons
		if (menu_stack[menu_stack_index].selected_index > 0)
			render_menu_item(&menu_stack[menu_stack_index].items[menu_stack[menu_stack_index].selected_index - 1], iconOffset + 0x00, 1);
		render_menu_item(&menu_stack[menu_stack_index].items[menu_stack[menu_stack_index].selected_index], iconOffset + 0x00, 2);
		if (menu_stack[menu_stack_index].selected_index < menu_stack[menu_stack_index].item_count - 1)
			render_menu_item(&menu_stack[menu_stack_index].items[menu_stack[menu_stack_index].selected_index + 1], iconOffset + 0x01, 3);
	}
}
