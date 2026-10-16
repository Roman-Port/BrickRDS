#include <core/views/utility/view_rtp.h>
#include <core/util/menu.h>
#include <core/util/display.h>
#include <core/util/rds.h>
#include <core/core.h>
#include <core/views.h>
#include <stdio.h>

view_info_t view_utility_rtp = (view_info_t) {
	.cb_open = view_utility_rtp_init,
	.cb_frame = view_utility_rtp_tick,
	.cb_close = 0
};

static const char* rtp_tag_prefixes[] = {
		"ITEM.",
		"INFO.",
		"STATIONNAME.",
		"PROGRAMME.",
		"PHONE.",
		"SMS.",
		"EMAIL.",
		"CHAT",
		"VOTE.",
		"RESERVED.",
		"PRIVATE."
};

/*
 * RTP tag names; First character is index into the prefixes - 0xF0
 * MAX ~26 chars with prefix
 */
static const char* rtp_tags[] = {
		"\xF0""(none)",
		"\xF1""TITLE",
		"\xF1""ALBUM",
		"\xF1""TRACKNUMBER",
		"\xF1""ARTIST",
		"\xF1""COMPOSITION",
		"\xF1""MOVEMENT",
		"\xF1""CONDUCTOR",
		"\xF1""COMPOSER",
		"\xF1""BAND",
		"\xF1""COMMENT",
		"\xF1""GENRE",
		"\xF2""NEWS",
		"\xF2""NEWS.LOCAL",
		"\xF2""STOCKMARKET",
		"\xF2""SPORT",
		"\xF2""LOTTERY",
		"\xF2""HOROSCOPE",
		"\xF2""DAILY_DIVERSION",
		"\xF2""HEALTH",
		"\xF2""EVENT",
		"\xF2""SZENE",
		"\xF2""CINEMA",
		"\xF2""TV",
		"\xF2""DATE_TIME",
		"\xF2""WEATHER",
		"\xF2""TRAFFIC",
		"\xF2""ALARM",
		"\xF2""ADVERTISEMENT",
		"\xF2""URL",
		"\xF2""OTHER",
		"\xF3""SHORT",
		"\xF3""LONG",
		"\xF4""NOW",
		"\xF4""NEXT",
		"\xF4""PART",
		"\xF4""HOST",
		"\xF4""EDITORIAL_STAFF",
		"\xF4""FREQUENCY",
		"\xF4""HOMEPAGE",
		"\xF4""SUBCHANNEL",
		"\xF5""HOTLINE",
		"\xF5""STUDIO",
		"\xF5""OTHER",
		"\xF6""STUDIO",
		"\xF6""OTHER",
		"\xF7""HOTLINE",
		"\xF7""STUDIO",
		"\xF7""OTHER",
		"\xF8""OTHER",
		"\xF9",
		"\xF9"".CENTER", // period is not a bug
		"\xFA""QUESTION",
		"\xFA""CENTER",
		"\xFB""1",
		"\xFB""2",
		"\xFC""1",
		"\xFC""2",
		"\xFC""3",
		"\xF0""PLACE",
		"\xF0""APPOINTMENT",
		"\xF0""IDENTIFIER",
		"\xF0""PURCHASE",
		"\xF0""GET_DATA"
};

/*
 * Checks if the mask is set in RDS status and returns ascii 1 or 0
 */
static char read_bit_from_status(int mask) {
	if (rds_state.status & mask)
		return '1';
	return '0';
}

/*
 * Renders a line.
 */
static void render_rtp_line(int row, int tagIndex, char* buffer) {
	//Clear this line
	disp_set_cursor(row, 0);
	for (int i = 0; i < 40; i++)
		disp_write_char(' ');

	//Get the descriptor
	const char* descriptor = rtp_tags[rds_state.rtp_tags[tagIndex].content_type];

	//Use first character as index into the prefix array
	const char* prefix = "";
	if (descriptor[0] > 0xF0)
		prefix = (rtp_tag_prefixes[descriptor[0] - 0xF1]);

	//Format message
	sprintf(buffer, "%i: %02i+%02i %i/%s%s", tagIndex + 1, rds_state.rtp_tags[tagIndex].start, rds_state.rtp_tags[tagIndex].length, rds_state.rtp_tags[tagIndex].content_type, prefix, &descriptor[1]);

	//Print
	disp_write_text_at(row, 0, buffer, -1);
}

/*
 * Init function
 */
void view_utility_rtp_init() {
	//Clear
	disp_clear();

	//Set title
	disp_write_text_at(0, 0, "RT+ Info", -1);
}

/*
 * Tick function
 */
void view_utility_rtp_tick() {
	char buffer[80];

	//Create status line
	sprintf(buffer, "RECEIVED=%c TOGGLE=%c RT_BANK=%c RUN=%c", read_bit_from_status(RDS_STATUS_RTP_RECEIVED), read_bit_from_status(RDS_STATUS_RTP_TOGGLE), read_bit_from_status(RDS_STATUS_RT_BANK_B), read_bit_from_status(RDS_STATUS_RTP_RUNNING));
	disp_write_text_at(1, 0, buffer, -1);

	//Render tags
	render_rtp_line(2, 0, buffer);
	render_rtp_line(3, 1, buffer);

	//Handle input
	if (core_get_button(BTN_CLEAR))
		rds_reset();
	if (core_get_button(BTN_BACK))
		menu_reshow();
}
