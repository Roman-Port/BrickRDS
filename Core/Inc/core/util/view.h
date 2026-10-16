#ifndef INC_CORE_UTIL_VIEW_H_
#define INC_CORE_UTIL_VIEW_H_

typedef void (*view_info_func)();

typedef struct view_info {

	// Optional. Function called when the view is opened every time.
	view_info_func cb_open;

	// Optional. Function called every frame this view is active.
	view_info_func cb_frame;

	// Optional. Function called when the view is switched away from.
	view_info_func cb_close;

} view_info_t;

#endif /* INC_CORE_UTIL_VIEW_H_ */
