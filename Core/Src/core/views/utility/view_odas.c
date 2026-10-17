#include <core/views/utility/view_odas.h>
#include <core/util/menu.h>
#include <core/util/display.h>
#include <core/util/rds.h>
#include <core/core.h>
#include <core/views.h>
#include <stdio.h>

view_info_t view_utility_odas = (view_info_t) {
	.cb_open = view_utility_odas_init,
	.cb_frame = view_utility_odas_tick,
	.cb_close = 0
};

static void render_oda(int col, rds_info_oda_t* oda) {
	//Create message bits
	char msgBits[17];
	for (int i = 0; i < 16; i++)
		msgBits[i] = ((oda->message >> (15 - i)) & 1) ? '1' : '0';
	msgBits[16] = 0;

	//Get group and version
	int group = oda->group >> 1;
	char ver = (oda->group & 1) ? 'B' : 'A';

	//Create line
	char buffer[40];
	sprintf(buffer, "%02i%c: %04X [%s]", group, ver, oda->aid, msgBits);

	//Render
	disp_write_text_at(col, 0, buffer, -1);
}

/*
 * Init function
 */
void view_utility_odas_init() {

}

/*
 * Tick function
 */
void view_utility_odas_tick() {
	//Clear
	disp_clear();

	//If no ODAs are defined, show message
	if (rds_state.oda_count == 0) {
		disp_write_text_at(0, 0, "No ODAs are defined yet.", -1);
	}

	//Render ODAs
	for (int i = 0; i < rds_state.oda_count && i < 4; i++)
		render_oda(i, &rds_state.oda[i]);

	//Handle input
	if (core_get_button(BTN_ENTER))
		rds_reset();
	if (core_get_button(BTN_BACK))
		menu_reshow();
}
