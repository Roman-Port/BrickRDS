#ifndef INC_CORE_UTIL_RDS_H_
#define INC_CORE_UTIL_RDS_H_

#include <stdint.h>

#define BLOCK_TYPE_A 0
#define BLOCK_TYPE_B 1
#define BLOCK_TYPE_C 2
#define BLOCK_TYPE_CP 3
#define BLOCK_TYPE_D 4
#define _BLOCK_TYPE_COUNT 5

#define GROUP_VER_A 0
#define GROUP_VER_B 1

#define RDS_STATUS_PTYN_READY (1 << 0) // PTYN is received and ready
#define RDS_STATUS_RT_READY (1 << 1) // RT is received and ready
#define RDS_STATUS_RT_INCOMING (1 << 2) // New RT is being recieved
#define RDS_STATUS_RT_BANK_B (1 << 3) // Set when the current fully recieved bank is bank B
#define RDS_STATUS_CT_READY (1 << 4) // Clock/time is recieved
#define RDS_STATUS_RTP_RECEIVED (1 << 5) // RT+ has ever been received
#define RDS_STATUS_RTP_TOGGLE (1 << 6) // The toggle bit was set on the last message
#define RDS_STATUS_RTP_RUNNING (1 << 7) // The running bit was set on the last message
#define RDS_STATUS_RTP_READY (1 << 8) // Cleared when a new RT message finishes reception and set when RTP data is recieved

#define RDS_DI_TP (1 << 0)
#define RDS_DI_TA (1 << 1)
#define RDS_DI_MS (1 << 2)
#define RDS_DI_STEREO (1 << 3)
#define RDS_DI_ARTIFICIAL_HEAD (1 << 4)
#define RDS_DI_COMPRESSED (1 << 5)
#define RDS_DI_DYNAMIC_PTY (1 << 6)

#define RDS_MAX_ODAS 4

typedef struct rds_info_oda {

	uint8_t group;
	uint16_t message;
	uint16_t aid;

} rds_info_oda_t;

typedef struct rds_info_rtp_tag {

	uint8_t content_type;
	uint8_t start;
	uint8_t length;

} rds_info_rtp_tag_t;

typedef struct rds_info {

	uint16_t pi; // PI code
	int pty; // Program type; -1 if not avail
	int status; // RDS_STATUS_* flags
	int di; // Decoder information from group 0 (as well as TP)
	int di_avail; // Decoder information from group 0 (as well as TP) that has been recieved; Same mask as di

	char ps_partial[8]; // Raw PS as it's loaded in
	char ps[8]; // The full PS

	char rt_partial[64]; // Partial RT
	char rt[64]; // Full RT after it has finished rx

	char ptyn_partial[8]; // Raw PTYN
	char ptyn[8]; // Fully recieved PTYN

	int ct_timestamp; // Clock/time timestamp in minutes (UTC)
	int ct_offset; // Clock/time offset in minutes
	uint32_t ct_captured_at; // The HAL tick time the CT was recieved at

	rds_info_oda_t oda[RDS_MAX_ODAS]; // Defined ODAs
	int oda_count; // The number of ODAs defined, up to 4

	rds_info_rtp_tag_t rtp_tags[2]; // RT+ tags
	int rtp_rt_bank; // The RT bank associated with the RT+ info; 0=A, 1=B, -1=Not Received, -2=Waiting for next RT packet

	int group_counter[16]; // Counts the groups recieved; Doesn't include version

} rds_info_t;

typedef struct rds_datetime {

	int year;
	int month;
	int day;
	int hour;
	int minute;

} rds_datetime_t;

/* VARS */

extern int rds_dropped_symbols; // Counter of dropped symbols going into the circular buffer reciving from IC
extern int rds_received_symbols; // Total symbols recieved
extern int rds_clk_errors; // Times when the clock was incorrect
extern uint32_t rds_start_time; // Start time of RDS RX, for debugging

extern uint16_t rds_blocks[_BLOCK_TYPE_COUNT]; // Block data
extern int rds_blocks_avail[_BLOCK_TYPE_COUNT]; // Set to 1 if block is available
extern int rds_blocks_corrected[_BLOCK_TYPE_COUNT]; // Has a bit set if the block was corrected
extern int rds_sync;
extern uint8_t rds_loss[40]; // Holds info for packet loss in format the display uses. That is, only the lower 3 bits are used of each byte

extern rds_info_t rds_state;

extern int rds_total_bits;
extern int rds_total_bits_bad;

/* FUNCTIONS */

/* Begins reciving RDS data from IC. */
void rds_enable();

/* Disables recepetion of data from IC. */
void rds_disable();

/*
 * Copies an RDS field to destination for display, fltering illegal chars and adding a terminator.
 * src: Source to copy from
 * dst: Destination to copy to
 * len: The length of items to copy; Destination should be len+1, as a null terminator is written.
 */
void copy_display_rds_field(const char* src, char* dst, int len);

/*
 * Processes incoming data from the IC
 */
void rds_process();

/*
 * Clears current state.
 */
void rds_reset();

/*
 * Utility function to convert from timestamp to a date/time.
 */
rds_datetime_t rds_convert_timestamp(int timestamp);

/*
 * Calculates a percentage of BER between 0-max
 */
int rds_calc_ber_percent(int max);

#endif /* INC_CORE_UTIL_RDS_H_ */
