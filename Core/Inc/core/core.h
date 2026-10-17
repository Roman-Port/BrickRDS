#ifndef INC_CORE_CORE_H_
#define INC_CORE_CORE_H_

#include <core/util/view.h>

#define BTN_LEFT (1 << 0)
#define BTN_RIGHT (1 << 1)
#define BTN_ENTER (1 << 2)
#define BTN_BACK (1 << 3)

extern view_info_t* current_view;

/*
 * Changes the active view to this, deactivating any old view and activating this view.
 * view: New view; May be null.
 */
void core_change_view(view_info_t* view);

/*
 * Called at program launch from main.
 */
void core_init();

/*
 * Returns 1 if this button was pressed since the last frame.
 */
int core_get_button(int btn);

/*
 * Called on an endless loop from main.
 */
void core_tick();

#endif /* INC_CORE_CORE_H_ */
