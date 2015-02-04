#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "riffa_virtmem.h"



typedef struct {
	uint32_t* array_base;
	uint64_t array_size;
} fpga_args;

typedef struct {
	uint32_t count;
} fpga_ret;



int main (int argc, char *argv[]) {
	if(argc < 2){
		printf("Usage: %s <array_size>\n", argv[0]);
		return 1;
	}

	int array_size = atoi(argv[1]);
	if(array_size < 1){
		printf("Invalid array size.\n");
		return 1;
	}

	long page_size = sysconf(_SC_PAGESIZE);

	uint32_t* a;
	size_t size_a = page_size * ((array_size * sizeof(uint32_t) + page_size - 1) / page_size);

	a = memalign(page_size, size_a);

	fpga_t * fpga;
	int id = 0;
	int chnl = 2;

	fpga = virtmem_open(id);
	if (fpga == NULL) {
		fprintf(stderr, "Could not get FPGA %d\n", id);
		return -1;
	}

	fpga_args* arg_struct;

	arg_struct = memalign(4, sizeof(fpga_args));

	arg_struct->array_base = a;
	arg_struct->array_size = array_size;

	fpga_ret* recv_struct;

	recv_struct = memalign(4, sizeof(fpga_ret));

	int num_retries = 3;

	for(int i = 0; i < num_retries; i++){
		int sent = 0;
		sent = fpga_send(fpga, chnl, arg_struct, sizeof(fpga_args) / 4, 0, 1, 5000);
		if(sent < sizeof(fpga_args) / 4){
			printf("Could not send arguments to FPGA.\n");
			fpga_reset(fpga);
			virtmem_close(fpga);
			return -1;
		}

		int recvd = 0;
		recvd = fpga_recv(fpga, chnl, recv_struct, ( sizeof(fpga_ret) >> 2) + 1, 5000);
		if(!recvd){

			printf("Waiting for FPGA response timed out.\n");
			fpga_reset(fpga);
			virtmem_close(fpga);

			return -1;
		}

		printf("FPGA counted to %u!\n", recv_struct->count);

		virtmem_flush(fpga);

		int num_errors = 0;
		for(int i=0; i < array_size; i++){
			if(a[i] != i){
				num_errors++;
				if(num_errors < 10){
					printf("a[%d]: %d\n", i, a[i]);
				}
			}
		}
		if(num_errors > 0){
			printf("%d errors total.\n", num_errors);
		} else {
			printf("No errors.\n");
		}
	}

	fpga_reset(fpga);
	
	virtmem_close(fpga);

	return 0;
}