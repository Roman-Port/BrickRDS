#ifndef INC_CORE_UTIL_MENU_H_
#define INC_CORE_UTIL_MENU_H_

#include <stdint.h>
#include <core/util/view.h>

#define MENU_ITEM_TYPE_END 0 // Special item that's not rendered that ends the list of items
#define MENU_ITEM_TYPE_BTN 1 // A menu item that calls a function
#define MENU_ITEM_TYPE_CHECKBOX 2 // A checkbox that is toggleable
#define MENU_ITEM_TYPE_SUBMENU 3 // Goes to another menu
#define MENU_ITEM_TYPE_BACK 4 // Button that goes to the previous menu
#define MENU_ITEM_TYPE_VIEW 5 // Button that goes to another view
#define MENU_ITEM_TYPE_DIRECTCHECK 6 // A checkbox that directly reads from a memory address rather than using callbacks

#define MENU_ITEM_ICON_DEFAULT 0 // Blank
#define MENU_ITEM_ICON_GEARS 1
#define MENU_ITEM_ICON_WRENCH 2
#define MENU_ITEM_ICON_SCREWDRIVER 3
#define MENU_ITEM_ICON_TOOLBOX 4
#define MENU_ITEM_ICON_PIN 5
#define MENU_ITEM_ICON_RESET 6
#define MENU_ITEM_ICON_SPONSOR_BLOCK 7

typedef void (*menu_item_btn_func)(void* userCtx);

typedef int (*menu_item_checkbox_read)(void* userCtx);
typedef void (*menu_item_checkbox_set)(void* userCtx, int value);

typedef struct menu_item {

	uint8_t type;
	uint8_t icon;
	void* user_context;
	const char* text;

	union {
		struct {
			menu_item_btn_func callback;
		} btn;

		struct {
			menu_item_checkbox_read read;
			menu_item_checkbox_set set;
		} checkbox;

		struct {
			const char* menu_name;
			const struct menu_item* items;
		} submenu;

		struct {
			view_info_t* view;
		} view;

		struct {
			uint8_t* value;
			menu_item_btn_func callback; // optional
		} direct_check;
	} info;

} menu_item_t;

/*
 * Shows a new menu item, pushing it to the stack. Shows the menu if not open.
 * name: The display name.
 * items: Items.
 */
void menu_show(const char* name, const menu_item_t* items);

/*
 * Go back a menu.
 */
void menu_back();

/*
 * Shows the previous menu.
 */
void menu_reshow();

#endif /* INC_CORE_UTIL_MENU_H_ */
