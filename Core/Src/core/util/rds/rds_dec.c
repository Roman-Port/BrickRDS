/* Handles RDS decoding after sync */

#include "stm32c0xx_hal.h"
#include "rds_internal.h"
#include <core/util/settings.h>
#include <string.h>

rds_info_t rds_state;

static int ps_seg_cont = -1; // The number of PS segments sent in order
static uint16_t last_pi;
static int last_pi_cont = 0; // Number of times the previous PI has been sent continously

/*
 * Decodes data in group A
 */
void rds_decode_group_a() {
	//Bail out if block is not available
	if (!rds_blocks_avail[BLOCK_TYPE_A])
		return;

	//Bail out if block was corrected; Any errors here will result in annoying clearing of data
	if (rds_blocks_corrected[BLOCK_TYPE_A])
		return;

	//Get pi
	uint16_t pi = rds_blocks[BLOCK_TYPE_A] & 0xFFFF;

	//Check if PI code has changed
	if (pi != last_pi) {
		last_pi_cont = 0;
	} else {
		last_pi_cont++;
	}
	last_pi = pi;

	//If the PI has changed for multiple packets in a row, proceed
	if (rds_state.pi != last_pi && last_pi_cont >= 3) {
		//Only reset if not controlled by freq
		if (!user_settings.use_clear_on_channel)
			rds_reset();

		//Update PI
		rds_state.pi = pi;
	}
}

/*
 * Decodes group 0A and 0B - Basic tuning info
 */
static void decode_group_0(int ver) {
	//Clear DI bits we are about to set
	rds_state.di &= RDS_DI_TA | RDS_DI_MS;

	//Mark these bits as received
	rds_state.di_avail |= RDS_DI_TA | RDS_DI_MS;

	//Set the TA/MS bits
	if ((rds_blocks[BLOCK_TYPE_B] >> 4) & 1)
		rds_state.di |= RDS_DI_TA;
	if ((rds_blocks[BLOCK_TYPE_B] >> 3) & 1)
		rds_state.di |= RDS_DI_MS;

	//Get other info in group B
	int di = (rds_blocks[BLOCK_TYPE_B] >> 2) & 1;
	int ctrl = rds_blocks[BLOCK_TYPE_B] & 0b11;

	//Use the control to number to set the DI
	int diMask = RDS_DI_STEREO << di;
	rds_state.di_avail |= diMask;
	if (di)
		rds_state.di |= diMask;
	else
		rds_state.di &= ~diMask;

	//If have group D, handle PS
	if (rds_blocks_avail[BLOCK_TYPE_D]) {
		//Decode the partial PS segments
		char seg[2];
		int segIndex = ctrl * 2;
		seg[0] = (rds_blocks[BLOCK_TYPE_D] >> 8) & 0xFF;
		seg[1] = rds_blocks[BLOCK_TYPE_D] & 0xFF;

		//If this segment has already been received, make sure it matches
		if (ctrl == ps_seg_cont && rds_state.ps_partial[segIndex] != seg[0] && rds_state.ps_partial[segIndex + 1] != seg[1]) {
			ps_seg_cont = -1; // Mark this as invalid
		}

		//Copy into buffer
		rds_state.ps_partial[segIndex] = seg[0];
		rds_state.ps_partial[segIndex + 1] = seg[1];

		//Update the counter of segments
		if (ctrl == 0)
			ps_seg_cont = 0; // Reset
		else if (ctrl == ps_seg_cont || ctrl == ps_seg_cont + 1)
			ps_seg_cont = ctrl; // Increment or leave unaltered
		else
			ps_seg_cont = -1; // Clear til next one

		//If segment is complete, copy to the complete PS
		if (ps_seg_cont == 3) {
			//Copy
			memcpy(rds_state.ps, rds_state.ps_partial, 8);

			//Reset for next segment
			ps_seg_cont = -1;
		}
	}
}

static int last_rt_flag = -1;
static uint16_t rt_received_segments = 0;

/*
 * Decodes group 2A and 2B - RadioText
 */
static void decode_group_2(int ver) {
	//Get flags and offsets
	int textBank = (rds_blocks[BLOCK_TYPE_B] >> 4) & 1;
	int textSeg = rds_blocks[BLOCK_TYPE_B] & 0b1111;

	//If RT+ bank switch is pending, save it
	if (rds_state.rtp_rt_bank == -2)
		rds_state.rtp_rt_bank = textBank;

	//Read semgnets for version
	int charIndex;
	int charLength;
	char newChars[4];
	if (!ver) {
		//Group A - Has 4 chars

		//Check that we have both groups
		if (!rds_blocks_avail[BLOCK_TYPE_C] || !rds_blocks_avail[BLOCK_TYPE_D])
			return;

		//Set up vars
		charIndex = textSeg * 4;
		charLength = 4;

		//Copy characters
		newChars[0] = (rds_blocks[BLOCK_TYPE_C] >> 8) & 0xFF;
		newChars[1] = rds_blocks[BLOCK_TYPE_C] & 0xFF;
		newChars[2] = (rds_blocks[BLOCK_TYPE_D] >> 8) & 0xFF;
		newChars[3] = rds_blocks[BLOCK_TYPE_D] & 0xFF;
	} else {
		//Group B - Has 2 chars

		//Set up vars
		charIndex = textSeg * 2;
		charLength = 2;

		//Check that we have this group
		if (!rds_blocks_avail[BLOCK_TYPE_D])
			return;

		//Copy characters
		newChars[0] = (rds_blocks[BLOCK_TYPE_D] >> 8) & 0xFF;
		newChars[1] = rds_blocks[BLOCK_TYPE_D] & 0xFF;
	}

	//Check if any of the incoming characters conflict with what we already have. If so, force a reset
	int forceReset = textBank != last_rt_flag;
	for (int i = 0; i < charLength; i++)
		forceReset = forceReset || (rds_state.rt_partial[charIndex + i] != 0 && rds_state.rt_partial[charIndex + i] != newChars[i]);

	//Force reset if requested (or if the flag changed)
	if (forceReset) {
		//Zero out buffer
		memset(rds_state.rt_partial, 0, sizeof(rds_state.rt_partial));

		//Reset state
		rt_received_segments = 0;
		last_rt_flag = textBank;
		rds_state.status |= RDS_STATUS_RT_INCOMING;
		rds_state.status &= ~RDS_STATUS_RTP_READY;
	}

	//Set bit in segments counter
	rt_received_segments |= 1 << textSeg;

	//Copy new segment in
	for (int i = 0; i < charLength; i++)
		rds_state.rt_partial[charIndex + i] = newChars[i];

	//Determine if fully received either by checking the recieved count to equal maximum or by searching for a carriage return
	int isFullyReceived = rt_received_segments == 0xFFFF;
	int curChar = 0;
	for (int seg = 0; seg < 0b1111 && (rt_received_segments & (1 << seg)) != 0; seg++) {
		for (int c = 0; c < charLength; c++) {
			if (rds_state.rt_partial[curChar++] == 0x0D)
				isFullyReceived = 1;
		}
	}

	//If fully recieved, copy
	if (isFullyReceived) {
		//Copy text
		memcpy(rds_state.rt, rds_state.rt_partial, 64);

		//Clear state flags
		rds_state.status &= ~(RDS_STATUS_RT_INCOMING | RDS_STATUS_RT_BANK_B);

		//Update state
		rds_state.status |= RDS_STATUS_RT_READY;
		if (last_rt_flag == 1)
			rds_state.status |= RDS_STATUS_RT_BANK_B;
	}
}

/*
 * Decodes group 3A - ODA definition
 */
static void decode_group_3a() {
	//Check that we have all groups
	if (!rds_blocks_avail[BLOCK_TYPE_B] || !rds_blocks_avail[BLOCK_TYPE_C] || !rds_blocks_avail[BLOCK_TYPE_D])
		return;

	//Decode group
	uint8_t group = rds_blocks[BLOCK_TYPE_B] & 0b11111;

	//Find if this ODA is defined already
	int odaIndex = rds_state.oda_count;
	for (int i = 0; i < odaIndex; i++) {
		if (rds_state.oda[i].group == group) {
			odaIndex = i;
			break;
		}
	}

	//Bail out if out of ODA slots
	if (odaIndex >= RDS_MAX_ODAS)
		return;

	//Save data to ODA definitions
	rds_state.oda[odaIndex].group = group;
	rds_state.oda[odaIndex].message = rds_blocks[BLOCK_TYPE_C];
	rds_state.oda[odaIndex].aid = rds_blocks[BLOCK_TYPE_D];

	//Update count
	odaIndex++;
	if (odaIndex > rds_state.oda_count)
		rds_state.oda_count = odaIndex;
}

/*
 * Decodes group 4A - Clock/time
 */
static void decode_group_4a() {
	//Check that we have all groups
	if (!rds_blocks_avail[BLOCK_TYPE_B] || !rds_blocks_avail[BLOCK_TYPE_C] || !rds_blocks_avail[BLOCK_TYPE_D])
		return;

	//Decode parts
	int day = ((rds_blocks[BLOCK_TYPE_B] & 0b11) << 15) | (rds_blocks[BLOCK_TYPE_C] >> 1);
	int hour = ((rds_blocks[BLOCK_TYPE_C] & 1) << 4) | (rds_blocks[BLOCK_TYPE_D] >> 12);
	int minute = (rds_blocks[BLOCK_TYPE_D] >> 6) & 0b111111;
	int localOffset = rds_blocks[BLOCK_TYPE_D] & 0b11111;
	if ((rds_blocks[BLOCK_TYPE_D] >> 5) & 1) // sign bit
		localOffset = -localOffset;

	//Convert local offset to minutes
	int localOffsetMins = localOffset * 30;

	//Check that values are sensible
	if (hour > 23)
		return;
	if (minute > 59)
		return;

	//Convert into timestamp expressed in minutes
	int timestamp = (day * (60 * 24)) + (hour * 60) + minute;

	//Update state
	rds_state.status |= RDS_STATUS_CT_READY;
	rds_state.ct_timestamp = timestamp;
	rds_state.ct_offset = localOffsetMins;
	rds_state.ct_captured_at = HAL_GetTick();
}

static int last_ptyn_flag = -1;
static uint8_t ptyn_received_segments = 0;

/*
 * Decodes group 10A - PTYN
 */
static void decode_group_10a() {
	//Check that we have both groups
	if (!rds_blocks_avail[BLOCK_TYPE_C] || !rds_blocks_avail[BLOCK_TYPE_D])
		return;

	//Get info
	int textBank = (rds_blocks[BLOCK_TYPE_B] >> 4) & 1;
	int textSeg = rds_blocks[BLOCK_TYPE_B] & 1;

	//If bank changed, reset
	if (textBank != last_ptyn_flag) {
		//Clear out partial buffer
		memset(rds_state.ptyn_partial, 0, sizeof(rds_state.ptyn_partial));

		//Reset state
		ptyn_received_segments = 0;
		last_ptyn_flag = textBank;
	}

	//Read into PTYN partial
	rds_state.ptyn_partial[(textSeg * 4) + 0] = (rds_blocks[BLOCK_TYPE_C] >> 8) & 0xFF;
	rds_state.ptyn_partial[(textSeg * 4) + 1] = rds_blocks[BLOCK_TYPE_C] & 0xFF;
	rds_state.ptyn_partial[(textSeg * 4) + 2] = (rds_blocks[BLOCK_TYPE_D] >> 8) & 0xFF;
	rds_state.ptyn_partial[(textSeg * 4) + 3] = rds_blocks[BLOCK_TYPE_D] & 0xFF;

	//Set the bit in the segments
	ptyn_received_segments |= 1 << textSeg;

	//If both are recieved, copy to the finaly PTYN
	if (ptyn_received_segments == 0b11) {
		//Copy
		memcpy(rds_state.ptyn, rds_state.ptyn_partial, sizeof(rds_state.ptyn));

		//Set flag
		rds_state.status |= RDS_STATUS_PTYN_READY;
	}
}

/*
 * Decodes ODA 0x4BD7 for RT+
 */
static void decode_oda_rtp() {
	//Check that we have all groups
	if (!rds_blocks_avail[BLOCK_TYPE_B] || !rds_blocks_avail[BLOCK_TYPE_C] || !rds_blocks_avail[BLOCK_TYPE_D])
		return;

	//Read the toggle bit
	int toggle = (rds_blocks[BLOCK_TYPE_B] >> 4) & 1;

	//Check if the toggle bit changed OR this is the first RT+ packet recieved
	if(!(rds_state.status & RDS_STATUS_RTP_RECEIVED) || (((rds_state.status & RDS_STATUS_RTP_TOGGLE) == RDS_STATUS_RTP_TOGGLE) != toggle)) {
		//Set a flag that associates the NEXT RT packet recieved's bank with this RT+ info
		rds_state.rtp_rt_bank = -2;
	}

	//Set the recieved and ready indicator
	rds_state.status |= RDS_STATUS_RTP_RECEIVED | RDS_STATUS_RTP_READY;

	//Clear the two bits in the status that we are about to set
	rds_state.status &= ~(RDS_STATUS_RTP_TOGGLE | RDS_STATUS_RTP_RUNNING);

	//Read the two control bits and write into info
	if (toggle)
		rds_state.status |= RDS_STATUS_RTP_TOGGLE;
	if ((rds_blocks[BLOCK_TYPE_B] >> 3) & 1)
		rds_state.status |= RDS_STATUS_RTP_RUNNING;

	//Read the first tag
	rds_state.rtp_tags[0].content_type = ((rds_blocks[BLOCK_TYPE_B] & 0b111) << 3) | ((rds_blocks[BLOCK_TYPE_C] >> 13) & 0b111);
	rds_state.rtp_tags[0].start = (rds_blocks[BLOCK_TYPE_C] >> 7) & 0b111111;
	rds_state.rtp_tags[0].length = ((rds_blocks[BLOCK_TYPE_C] >> 1) & 0b111111) + 1;

	//Read the second tag
	rds_state.rtp_tags[1].content_type = ((rds_blocks[BLOCK_TYPE_C] & 1) << 5) | ((rds_blocks[BLOCK_TYPE_D] >> 11) & 0b1111);
	rds_state.rtp_tags[1].start = (rds_blocks[BLOCK_TYPE_D] >> 5) & 0b111111;
	rds_state.rtp_tags[1].length = (rds_blocks[BLOCK_TYPE_D] & 0b11111) + 1; // intentionally only 5 bits
}

/*
 * Decodes an entire group.
 */
void rds_decode() {
	//Process group A
	rds_decode_group_a();

	//Bail out if block is not available
	if (!rds_blocks_avail[BLOCK_TYPE_B])
		return;

	//Decode TP
	rds_state.di_avail |= RDS_DI_TP;
	if ((rds_blocks[BLOCK_TYPE_B] >> 10) & 1)
		rds_state.di |= RDS_DI_TP;
	else
		rds_state.di &= ~RDS_DI_TP;

	//Decode PTY
	rds_state.pty = (rds_blocks[BLOCK_TYPE_B] >> 5) & 0b11111;

	//Get the group info
	int combinedGroup = (rds_blocks[BLOCK_TYPE_B] >> 11) & 0b11111;
	int group = combinedGroup >> 1;
	int ver = combinedGroup & 1;

	//Increment group counter
	rds_state.group_counter[group]++;

	//Switch on group type
	switch (group) {
	case 0b0000:
		decode_group_0(ver);
		break;
	case 0b0010:
		decode_group_2(ver);
		break;
	case 0b0011:
		if (ver == 0)
			decode_group_3a();
		break;
	case 0b1010:
		if (ver == 0)
			decode_group_10a();
		break;
	case 0b0100:
		if (ver == 0)
			decode_group_4a();
		break;
	}

	//Scan for ODAs
	for (int i = 0; i < rds_state.oda_count; i++) {
		//Match group
		if (rds_state.oda[i].group == combinedGroup) {
			//Look for recognized applications
			switch (rds_state.oda[i].aid) {
			case 0x4BD7:
				decode_oda_rtp();
				break;
			}
		}
	}
}

/*
 * Clears current state.
 */
void rds_reset() {
	//Clear out the state
	rds_state.pi = 0;
	rds_state.pty = -1;
	rds_state.status = 0;
	rds_state.di = 0;
	rds_state.di_avail = 0;
	rds_state.oda_count = 0;
	rds_state.rtp_rt_bank = -1;
	memset(&rds_state.ps_partial, 0, sizeof(rds_state.ps_partial));
	memset(&rds_state.ps, 0, sizeof(rds_state.ps));
	memset(&rds_state.rt_partial, 0, sizeof(rds_state.rt_partial));
	memset(&rds_state.rt, 0, sizeof(rds_state.rt));
	memset(&rds_state.ptyn_partial, 0, sizeof(rds_state.ptyn_partial));
	memset(&rds_state.ptyn, 0, sizeof(rds_state.ptyn));
	memset(&rds_state.group_counter, 0, sizeof(rds_state.group_counter));

	//Clear internal state
	ps_seg_cont = -1;
	last_rt_flag = -1;
	rt_received_segments = 0;
	last_ptyn_flag = -1;
	ptyn_received_segments = 0;
}
