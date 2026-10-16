#include <core/views/view_baserds.h>
#include <core/views/view_rds_rt.h>
#include <core/util/textarea.h>
#include <core/util/rds.h>
#include <core/util/display.h>
#include <core/util/settings.h>
#include <core/menus.h>
#include <core/views.h>
#include <core/core.h>

static textarea_data_t ta_rt;
static textarea_data_t ta_rtp_a;
static textarea_data_t ta_rtp_b;

view_info_t view_rds_rt = (view_info_t) {
	.cb_open = view_rds_rt_init,
	.cb_frame = view_rds_rt_tick,
	.cb_close = 0
};

/*
 * Init function for RDS RT, which shows raw RT
 */
void view_rds_rt_init() {
	view_baserds_init(&view_rds_loss, &view_rds_details);

	//Init text area for RT
	textarea_init(&ta_rt, 2, 0, 40, 2);

	//Init RT+ areas
	textarea_init(&ta_rtp_a, 2, 0, 40, 1);
	textarea_init(&ta_rtp_b, 3, 0, 40, 1);
}

/*
 * Tick function for RDS RT
 */
void view_rds_rt_tick() {
	//Create RT, filtering illegal chars
	char rt[65];
	copy_display_rds_field(user_settings.use_partial ? rds_state.rt_partial : rds_state.rt, rt, 64);

	//If RT+ is disabled or not recieved, use regular RT
	if (!user_settings.use_rtp || !(rds_state.status & RDS_STATUS_RTP_RECEIVED)) {
		//Copy directly
		textarea_copy_text(&ta_rt, rt);
	} else {
		//Get the current active RT bank
		int activeRtBank = (rds_state.status & RDS_STATUS_RT_BANK_B) == RDS_STATUS_RT_BANK_B;

		//Check if RTP is downloaded for this bank, the message is ready, and the active bank matches
		int rtpValid = rds_state.rtp_rt_bank == activeRtBank;
		if (!user_settings.use_partial)
			rtpValid = rtpValid && (rds_state.status & RDS_STATUS_RTP_READY) && !(rds_state.status & RDS_STATUS_RT_INCOMING);

		//Check which are valid
		int rtpAValid = rtpValid && rds_state.rtp_tags[0].content_type > 0;
		int rtpBValid = rtpValid && rds_state.rtp_tags[1].content_type > 0;

		//If A is valid and B is not, use the full content area for A
		if (rtpAValid && !rtpBValid) {
			//Copy to main area
			textarea_copy_text_length(&ta_rt, &rt[rds_state.rtp_tags[0].start], rds_state.rtp_tags[0].length);
		} else {
			//Render first RT+
			if (rtpAValid)
				textarea_copy_text_length(&ta_rtp_a, &rt[rds_state.rtp_tags[0].start], rds_state.rtp_tags[0].length);
			else
				textarea_clear(&ta_rtp_a);

			//Render second RT+
			if (rtpBValid)
				textarea_copy_text_length(&ta_rtp_b, &rt[rds_state.rtp_tags[1].start], rds_state.rtp_tags[1].length);
			else
				textarea_clear(&ta_rtp_b);
		}
	}

	//If RT is currently in progress, show indicator
	if (rds_state.status & RDS_STATUS_RT_INCOMING) {
		disp_set_cursor(3, 39);
		disp_write_char(0x07);
	}

	//Run everything else
	view_baserds_tick();
}
