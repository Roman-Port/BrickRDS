/* RDS syncing code - Much of this is modified from SDR++ */

#include "rds_internal.h"
#include <core/util/settings.h>
#include <core/util/tuner.h>
#include <assert.h>

#define BER_SAMPLE_SUBSAMPLES 1 // Number of times a single BER sample is added to (calculated with 256/BLOCK_SIZE)
#define BER_SAMPLES 64 // The number of BER samples to store
#define BER_SAMPLE_MAX (BLOCK_LEN * BER_SAMPLE_SUBSAMPLES) // The number of bits a single sample holds

#define bool int

static uint32_t shiftReg = 0;
int rds_sync = 0;
uint16_t rds_blocks[_BLOCK_TYPE_COUNT];
int rds_blocks_avail[_BLOCK_TYPE_COUNT];
int rds_blocks_corrected[_BLOCK_TYPE_COUNT];
uint8_t rds_loss[40];
int rds_total_bits = 0;
int rds_total_bits_bad = 0;

static int skip = 0;
static int lastType = BLOCK_TYPE_A;
static int syncLastType = BLOCK_TYPE_A;
static int contGroup = 0;
static int last_channel = -1;

static const uint16_t LFSR_POLY = 0b0110111001;
static const uint16_t IN_POLY   = 0b1100011011;

static const int BLOCK_LEN = 26;
static const int DATA_LEN = 16;
static const int POLY_LEN = 10;

static const uint16_t syndromes_map[] = {
		0b1111011000, // BLOCK_TYPE_A
		0b1111010100, // BLOCK_TYPE_B
		0b1001011100, // BLOCK_TYPE_C
		0b1111001100, // BLOCK_TYPE_CP -- Should always be the PI code and is seldom used
		0b1001011000  // BLOCK_TYPE_D
};

static const uint16_t offsets_map[] = {
		0b0011111100, // BLOCK_TYPE_A
		0b0110011000, // BLOCK_TYPE_B
		0b0101101000, // BLOCK_TYPE_C
		0b1101010000, // BLOCK_TYPE_CP -- Should always be the PI code and is seldom used
		0b0110110100  // BLOCK_TYPE_D
};

/*
 * Shifts a 1 or 0 into the loss bit rendered on the status screen.
 */
static void shift_loss_bit(int b) {
	uint8_t temp = b << 3;
	for (int i = 0; i < 40; i++) {
		temp = (rds_loss[i] << 1) | ((temp >> 3) & 1);
		rds_loss[i] = temp;
	}
}

/*
 * Checks for index within the syndrome map. Returns -1 if not found.
 * syndrome: THe syndrome to search for.
 */
static int detect_syndrome(uint16_t syndrome) {
	for (int i = 0; i < sizeof(syndromes_map) / sizeof(syndromes_map[0]); i++) {
		if (syndromes_map[i] == syndrome)
			return i;
	}
	return -1;
}

/*
 * Calculates syndrome and returns it.
 * block: The data to read.
 */
static uint16_t calc_syndrome(uint32_t block) {
	uint16_t syn = 0;

	// Calculate the syndrome using a LFSR
	for (int i = BLOCK_LEN - 1; i >= 0; i--) {
		// Shift the syndrome and keep the output
		uint8_t outBit = (syn >> (POLY_LEN - 1)) & 1;
		syn = (syn << 1) & 0b1111111111;

		// Apply LFSR polynomial
		syn ^= LFSR_POLY * outBit;

		// Apply input polynomial.
		syn ^= IN_POLY * ((block >> i) & 1);
	}

	return syn;
}

/*
 * Checks for errors and corrects them if DX mode is on.
 * block: Block
 * type: Block group type
 * recordered: Boolean set if OK
 */
static uint32_t handle_errors(uint32_t block, int type, int* recovered, int* errors) {
	// Subtract the offset from block
	block ^= (uint32_t)offsets_map[type];
	uint32_t out = block;

	// Calculate syndrome of corrected block
	uint16_t syn = calc_syndrome(block);

	//Check if errors are present
	if (syn) {
		//Check if error correcting is on
		if (user_settings.use_dx) {
			// Use the syndrome register to do error correction if errors are present
			uint8_t errorFound = 0;
			for (int i = DATA_LEN - 1; i >= 0; i--) {
				// Check if the 5 leftmost bits are all zero
				errorFound |= !(syn & 0b11111);

				// Write output
				uint8_t outBit = (syn >> (POLY_LEN - 1)) & 1;
				out ^= (errorFound & outBit) << (i + POLY_LEN);

				// Shift syndrome
				syn = (syn << 1) & 0b1111111111;
				syn ^= LFSR_POLY * outBit * !errorFound;
			}

			//Set recovered state
			(*recovered) = !(syn & 0b11111);
		} else {
			//Mark the block as bad and move on
			(*recovered) = 0;
		}

		//Count error bits
		(*errors) = 0;
		for (int i = 0; i < BLOCK_LEN; i++) {
			if ((syn >> i) & 1)
				(*errors)++;
		}
	} else {
		//Block OK as-is; Clear error flag and set available
		(*errors) = 0;
		(*recovered) = 1;
	}

	return out;
}

/*
 * Returns 1 if type order agrees with how it should be.
 * As in A->B, B->C/C1, -> C/C1 -> D, D-> E/A etc
 */
static int is_type_order_sensible(int prevType, int newType) {
	switch (prevType) {
	case BLOCK_TYPE_A:
		return newType == BLOCK_TYPE_B;
	case BLOCK_TYPE_B:
		return newType == BLOCK_TYPE_C || newType == BLOCK_TYPE_CP;
	case BLOCK_TYPE_C:
	case BLOCK_TYPE_CP:
		return newType == BLOCK_TYPE_D;
	case BLOCK_TYPE_D:
		return newType == BLOCK_TYPE_A;
	}
	return 0;
}

static int bits_until_loss_update = 0; // Used for loss stats; Counts down til a new item is submitted
static int last_block_ok = 0; // Used for loss status; Stores if the current block is OK (1) or not (0)
static int bad_bits_since_loss_update = BLOCK_LEN; // Counter for good bits decoded since last update
static int current_ber_sample = 0; // The current sample in the list of BER samples to use
static int current_ber_sample_subsample = 0; // The number of "subsamples" within a BER sample
static int ber_samples_available = 0; // The number of samples currently available
static uint8_t ber_samples[BER_SAMPLES]; // Stores the number of error bits; Used as a circular buffer

/*
 * Decodes a raw symbol (1/0) from the decoder IC.
 * sym: Symbol to process
 */
static void rds_sync_process_symbol(uint8_t sym) {
	// Shift in the bit
	shiftReg = ((shiftReg << 1) & 0x3FFFFFF) | (sym & 1);

	//Every BLOCK_LEN bits, push a group loss value
	if (bits_until_loss_update <= 0) {
		//Shift lost in
		shift_loss_bit(last_block_ok);

		//Update the total/good bits counter
		rds_total_bits += BLOCK_LEN;
		rds_total_bits_bad += bad_bits_since_loss_update;

		//Update BER
		ber_samples[current_ber_sample] += bad_bits_since_loss_update;
		if (++current_ber_sample_subsample >= BER_SAMPLE_SUBSAMPLES) {
			//Update count
			ber_samples_available++;
			if (ber_samples_available >= BER_SAMPLES)
				ber_samples_available = BER_SAMPLES - 1;

			//Advance to next sample
			current_ber_sample = (current_ber_sample + 1) % BER_SAMPLES;
			ber_samples[current_ber_sample] = 0;
			current_ber_sample_subsample = 0;
		}

		//Clear state
		last_block_ok = 0;
		bad_bits_since_loss_update = BLOCK_LEN;
		bits_until_loss_update = BLOCK_LEN;
	}
	bits_until_loss_update--;

	// Skip if we need to shift in new data
	if (--skip > 0) { return; }

	// Calculate the syndrome and update sync status
	uint16_t syn = calc_syndrome(shiftReg);
	int type = detect_syndrome(syn);
	int knownSyndrome = type >= 0;

	//Update sync; On good block, increment 0->1 but only 1->2 if the order makes sense
	if (knownSyndrome) {
		//Incremnt
		if (rds_sync == 1) { // handle first block going into sync specially
			//Check if the type makes sense from the previous one
			if (is_type_order_sensible(syncLastType, type))
				rds_sync++;
			else
				rds_sync--;
		} else if (rds_sync < 5) { // cap at 5 (4 usable)
			rds_sync++;
		}

		//Save last type
		syncLastType = type;
	} else if (rds_sync > 0) {
		rds_sync--;
		syncLastType = -1;
	}

	//If starting to get sync, update skip
	if (rds_sync > 0)
		skip = BLOCK_LEN;

	//If no sync, count up to mark loss and then bail out for next
	if (rds_sync <= 1) {
		/*loss_bit_counter++;
		if (loss_bit_counter == (BLOCK_LEN * 1)) {
			shift_loss_bit(0);
			loss_bit_counter = 0;
		}*/
		return;
	}

	//If not a known syndrome, increment last type
	if (!knownSyndrome) {
		// Increment around C/CP
		switch (lastType) {
		case BLOCK_TYPE_A:
			type = BLOCK_TYPE_B;
			break;
		case BLOCK_TYPE_B:
			//Check if the group version is A or B
			if ((rds_blocks[BLOCK_TYPE_B] >> 11) & 1)
				type = BLOCK_TYPE_CP; // version B
			else
				type = BLOCK_TYPE_C; // version A
			break;
		case BLOCK_TYPE_C:
		case BLOCK_TYPE_CP: // <-- is this right? this was previously forgotten resulting in type = -1
			type = BLOCK_TYPE_D;
			break;
		case BLOCK_TYPE_D:
			type = BLOCK_TYPE_A;
			break;
		}
	}
	syncLastType = type;

	//If DX is on, attempt correction, otherwise discard bad blocks
	assert(type >= 0);
	assert(type <= _BLOCK_TYPE_COUNT);
	rds_blocks[type] = handle_errors(shiftReg, type, &rds_blocks_avail[type], &rds_blocks_corrected[type]) >> 10;

	//Force next run to push to statistics
	bits_until_loss_update = 0;
	last_block_ok = rds_blocks_avail[type];
	bad_bits_since_loss_update = rds_blocks_corrected[type];

	//Test reception of full group; Reset counter at start
	if (type == BLOCK_TYPE_A) {
		//Reset at start
		contGroup = 0;
	} else if (contGroup >= 0) {
		//Check that the steps make sense
		if (is_type_order_sensible(lastType, type))
			contGroup++;
		else
			contGroup = -1; // fail til next start of group

		//If it is 3 it means we just recieved block D; Process
		if (contGroup == 3) {
			//Check if all blocks are available to count a full group in statistics
			/*if (rds_blocks[BLOCK_TYPE_A] && rds_blocks[BLOCK_TYPE_B] && (rds_blocks[BLOCK_TYPE_C] || rds_blocks[BLOCK_TYPE_CP]) && rds_blocks[BLOCK_TYPE_D])
				shift_loss_bit(1);*/

			//Decode
			rds_decode();

			//Reset for next
			contGroup = -1;
		}
	}

	//Save last type
	lastType = type;
}

/*
 * Processes incoming data from the IC
 */
void rds_process() {
	//Initiate reset on channel change
	int currentChannel = tuner_current_channel;
	if (user_settings.use_clear_on_channel && last_channel != currentChannel && last_channel != -1)
		rds_reset();
	last_channel = currentChannel;

	//Process blocks
	uint8_t block[RDS_RX_BLOCK_SIZE];
	uint8_t bits;
	while (rds_rx_symbols(block)) {
		for (int b = 0; b < RDS_RX_BLOCK_SIZE; b++) {
			bits = block[b];
			for (int i = 0; i < 8; i++) {
				rds_sync_process_symbol(bits & 1);
				bits >>= 1;
			}
		}
	}
}

/*
 * Calculates the bit error ratio.
 * total: Set to the total number of bits clocked.
 * errors: Set to the number of error bits.
 */
void rds_calc_ber(int* total, int* errors) {
	//Clear
	*total = ber_samples_available * BER_SAMPLE_MAX;
	*errors = 0;

	//Calculate all proceeding samples before the current one
	int read = (current_ber_sample - ber_samples_available) + BER_SAMPLES;
	for (int i = 0; i < ber_samples_available; i++)
		(*errors) += ber_samples[read++ % BER_SAMPLES];
}
