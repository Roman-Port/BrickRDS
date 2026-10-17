/* RDS ingest from IC */

#include <core/util/rds.h>
#include "rds_internal.h"
#include "main.h"

#define DMA_BUFFER_SIZE 16
#define DMA_BUFFER_SIZE_HALF 8 // must match RDS_RX_BLOCK_SIZE
#define CIRC_BUFFER_SIZE_BUFFERS 16 // number of RDS_RX_BLOCK_SIZE buffers in rds_circ_buffer
#define CIRC_BUFFER_SIZE_BYTES 128 // CIRC_BUFFER_SIZE_BUFFERS * RDS_RX_BLOCK_SIZE

//static uint8_t rds_dma_buffer[DMA_BUFFER_SIZE]); // intermediate buffer used for DMA transfers
static uint8_t rds_circ_buffer[CIRC_BUFFER_SIZE_BYTES];
static int rds_circ_buffer_avail = 0; // Buffers available to read
static int rds_circ_buffer_free = CIRC_BUFFER_SIZE_BUFFERS; // Buffers available to write
static int rds_circ_buffer_read = 0; // BYTE read pointer; MUST be aligned to RDS_RX_BLOCK_SIZE
static int rds_circ_buffer_write = 0; // BYTE write pointer; MUST be aligned to RDS_RX_BLOCK_SIZE
static int rds_is_enabled = 0; // 1 if enabled

static uint8_t rds_dma_buffer[DMA_BUFFER_SIZE]; // buffer that dma writes into

uint32_t rds_start_time; // Start tick of RDS RX
int rds_dropped_symbols = 0; // Counter of dropped symbols going into the circular buffer
int rds_received_symbols = 0; // Total symbols recieved counter
int rds_clk_errors = 0; // Times when the clock was incorrect

static void transfer_complete(const uint8_t* src) {
	//Update counter
	rds_received_symbols += RDS_RX_BLOCK_SIZE * 8;

	//Check if there's room
	if (rds_circ_buffer_free <= 0) {
		rds_dropped_symbols += RDS_RX_BLOCK_SIZE * 8;
		return;
	}

	//Write
	for (int i = 0; i < RDS_RX_BLOCK_SIZE; i++)
		rds_circ_buffer[rds_circ_buffer_write++] = *(src++);

	//Update state
	rds_circ_buffer_write %= CIRC_BUFFER_SIZE_BYTES;
	rds_circ_buffer_free--;
	rds_circ_buffer_avail++;
}

/* Raised by DMA; First half of transfer completed. */
static void transfer_b0_complete(SPI_HandleTypeDef* spi) {
	transfer_complete(rds_dma_buffer);
}

/* Raised by DMA; Second half of transfer completed. */
static void transfer_b1_complete(SPI_HandleTypeDef* spi) {
	transfer_complete(&rds_dma_buffer[DMA_BUFFER_SIZE_HALF]);
}

/* Raised by DMA; Transfer error */
static void transfer_error(SPI_HandleTypeDef* spi) {
	rds_received_symbols = -1;
}

/* Begins reciving RDS data from IC. */
void rds_enable() {
	//If already active, don't do anything
	if (rds_is_enabled)
		return;
	rds_is_enabled = 1;

	//Reset state
	rds_dropped_symbols = 0;
	rds_received_symbols = 0;
	rds_circ_buffer_avail = 0;
	rds_circ_buffer_free = CIRC_BUFFER_SIZE_BUFFERS;
	rds_circ_buffer_read = 0;
	rds_circ_buffer_write = 0;

	//Reset main state
	rds_reset();

	//Record starting time for debugging
	rds_start_time = HAL_GetTick();

	//Set up SPI DMA
	hspi2.RxHalfCpltCallback = transfer_b0_complete;
	hspi2.RxCpltCallback = transfer_b1_complete;
	hspi2.ErrorCallback = transfer_error;

	//Start DMA transfer
	HAL_SPI_Receive_DMA(&hspi2, rds_dma_buffer, DMA_BUFFER_SIZE);
}

/*
 * Reads RDS_RX_BLOCK_SIZE samples into block. Returns 1 on success, otherwise 0.
 */
int rds_rx_symbols(uint8_t* block) {
	//Check if block is available
	if (rds_circ_buffer_avail <= 0)
		return 0;

	//Read
	for (int i = 0; i < RDS_RX_BLOCK_SIZE; i++)
		*(block++) = rds_circ_buffer[rds_circ_buffer_read++];

	//Update state
	rds_circ_buffer_read %= CIRC_BUFFER_SIZE_BYTES;
	rds_circ_buffer_avail--;
	rds_circ_buffer_free++;

	return 1;
}

/* Disables recepetion of data from IC. */
void rds_disable() {
	//Kill DMA
	HAL_SPI_DMAStop(&hspi2);

	//Clear flag
	rds_is_enabled = 0;
}
