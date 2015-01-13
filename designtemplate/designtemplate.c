#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "riffa.h"


typedef struct {
	...
} fpga_args;

typedef struct {
	...
} fpga_ret;

int main (int argc, char *argv[]) {
	fpga_t * fpga;
	int id = 0;
	int chnl = 2;

	fpga = fpga_open(id);
	if (fpga == NULL) {
		fprintf(stderr, "Could not get FPGA %d\n", id);
		return -1;
	}

	fpga_args* arg_struct;

	arg_struct = memalign(4, sizeof(fpga_args));

	//TODO: initialize arg_struct

	int sent = 0;
	sent = fpga_send(fpga, chnl, arg_struct, sizeof(fpga_args) / 4, 0, 1, 5000);
	if(sent < sizeof(fpga_args) / 4){
		printf("Could not send arguments to FPGA.\n");
		return -1;
	}

	fpga_ret* recv_struct;

	recv_struct = memalign(4, sizeof(fpga_ret));

	int recvd = 0;
	recvd = fpga_recv(fpga, chnl, recv_struct, (sizeof(fpga_ret) >> 2) + 1, 5000);
	if(!recvd){

		printf("Waiting for FPGA response timed out.\n");
		fpga_reset(fpga);
		fpga_close(fpga);

		return -1;
	}

	//TODO: read and check recv_struct

	fpga_flush(fpga);

	fpga_reset(fpga);
	
	fpga_close(fpga);

	return 0;
}