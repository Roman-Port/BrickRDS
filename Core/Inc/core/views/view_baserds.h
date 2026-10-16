#ifndef INC_CORE_VIEWS_VIEW_BASERDS_H_
#define INC_CORE_VIEWS_VIEW_BASERDS_H_

#include <core/util/view.h>

/*
 * Init function for baserds, which provides the upper portion scrolling PS/callsign
 */
void view_baserds_init(view_info_t* prevView, view_info_t* nextView);

/*
 * Tick function for baserds
 */
void view_baserds_tick();

#endif /* INC_CORE_VIEWS_VIEW_BASERDS_H_ */
