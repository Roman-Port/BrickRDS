#ifndef SRC_CORE_UTIL_RDS_RDS_INTERNAL_H_
#define SRC_CORE_UTIL_RDS_RDS_INTERNAL_H_

/* Internal use RDS code */

#include <stdint.h>
#include <core/util/rds.h>

#define RDS_RX_BLOCK_SIZE 8 // number of 8-bit bytes of symbols read from rds_rx.c; Remember to update in rds_rx.c

/*
 * Reads RDS_RX_BLOCK_SIZE samples into block. Returns 1 on success, otherwise 0.
 */
int rds_rx_symbols(uint8_t* block);

/*
 * Decodes an entire group.
 */
void rds_decode();

#endif /* SRC_CORE_UTIL_RDS_RDS_INTERNAL_H_ */
