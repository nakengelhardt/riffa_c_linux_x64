#ifndef VIRTMEM_H
#define VIRTMEM_H

#include "riffa.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Open an FPGA containing a design using virtual memory. To be used instead of fpga_open.
 */
fpga_t * virtmem_open(int id);

/*
 * Close an FPGA containing a design using virtual memory. To be used instead of fpga_close.
 */
void virtmem_close(fpga_t * fpga);

/**
 * Flushes any modifications from the virtual memory cache in the FPGA back to main memory.
 */
void virtmem_flush(fpga_t* fpga);

#ifdef __cplusplus
}
#endif

#endif
