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

static uint32_t rds_fifo;
static int rds_fifo_avail; // bits available in FIFO

uint32_t rds_start_time; // Start tick of RDS RX
int rds_dropped_symbols = 0; // Counter of dropped symbols going into the circular buffer
int rds_received_symbols = 0; // Total symbols recieved counter
int rds_clk_errors = 0; // Times when the clock was incorrect

#ifdef RDS_DMA_SUPPORTED

/* Called when a transfer buffer is completed and writes to circular buffer
 *
 * */
static void transfer_complete(const uint8_t* src, int count) {
	//Update counter
	rds_received_symbols += count;

	//Loop until we consume all or run out of room
	while (count > 0 && rds_circ_buffer_free > 0) {
		//Copy to buffer
		rds_circ_buffer[rds_circ_buffer_write] = *src;

		//Update circular buffer state
		rds_circ_buffer_write = (rds_circ_buffer_write + 1) % CIRC_BUFFER_SIZE;
		rds_circ_buffer_free--;
		rds_circ_buffer_avail++;

		//Increment pointer and state
		src++;
		count--;
	}

	//Add any remaining to dropped
	rds_dropped_symbols += count;
}

/* Called via interrupt when the last half of the buffer has data */
static void transfer_b1_complete_callback(DMA_HandleTypeDef *hdma)
{
	transfer_complete(&rds_dma_buffer[DMA_BUFFER_SIZE / 2], DMA_BUFFER_SIZE / 2);
}

/* Called via interrupt when the first half of the buffer has data */
static void transfer_b0_complete_callback(DMA_HandleTypeDef *hdma)
{
	transfer_complete(rds_dma_buffer, DMA_BUFFER_SIZE / 2);
}

static void transfer_error(DMA_HandleTypeDef *hdma) {
	rds_received_symbols = -1;
}
#endif

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

	/*for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
		rds_dma_buffer[i] = RDS_DAT_GPIO_Port->IDR & 0b11;
		int clk = rds_dma_buffer[i] & 0b10;
		while ((RDS_DAT_GPIO_Port->IDR & 0b10) == clk);
	}*/

	//Reset state
	rds_dropped_symbols = 0;
	rds_received_symbols = 0;
	rds_fifo = 0;
	rds_fifo_avail = 32;
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

#ifdef RDS_DMA_SUPPORTED
	//Configure callbacks
	/*hdma_dma_generator0.XferCpltCallback = transfer_b1_complete_callback;
	hdma_dma_generator0.XferHalfCpltCallback = transfer_b0_complete_callback;
	hdma_dma_generator0.XferErrorCallback = transfer_error;

	//Enable DMA reading from the input data register for the GPIO port to the DMA buffer
	HAL_StatusTypeDef result = HAL_DMAEx_EnableMuxRequestGenerator(&hdma_dma_generator0);
	if (result != HAL_OK) {
			rds_received_symbols = -2;
		}
	result = HAL_DMA_Start_IT(&hdma_dma_generator0, (uint32_t)&RDS_DAT_GPIO_Port->IDR, (uint32_t)rds_dma_buffer, 2);
	if (result != HAL_OK) {
		rds_received_symbols = -1;
	}*/
#else
#endif
}

/* Raised on interrupt of clocking a bit */
void rds_clock_it() {


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

/*
 * Reads a block of 8 symbols.
 */
int rds_read_cued_symbols(uint8_t* sym) {
	//Briefly disable interrupts while performing operations
	__disable_irq();

	/*//Check if avail
	int avail = rds_circ_buffer_avail;
	if (avail) {
		(*sym) = rds_circ_buffer[rds_circ_buffer_read];
		rds_circ_buffer_read = (rds_circ_buffer_read + 1) % CIRC_BUFFER_SIZE;
		rds_circ_buffer_avail--;
		rds_circ_buffer_free++;
	}*/

	//Re-enable interrupts
	__enable_irq();

	return 0;
}

/* Disables recepetion of data from IC. */
void rds_disable() {
	//Kill DMA
	HAL_SPI_DMAStop(&hspi2);

#ifdef RDS_DMA_SUPPORTED
	//Kill DMA
	HAL_DMA_Abort_IT(&hdma_dma_generator0);
#else
#endif

	//Clear set
	rds_is_enabled = 0;
}
