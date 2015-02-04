#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include "riffa_virtmem.h"

#define DATA_SZ 4096
unsigned int __attribute__ ((aligned (4096))) data[DATA_SZ];

int min(int a, int b){
	return a > b ? b : a;
}

typedef struct { 
	void* data_start;
	uint64_t xy_range;
} whoosh_args;


int main(int argc, char** argv) {
	fpga_t * fpga;
	fpga_info_list info;
	int option;
	int i;
	int id;
	int chnl;
	int sent;
	int recvd;

	whoosh_args* sendBuffer;
	unsigned int * recvBuffer;

	if (argc < 2) {
		printf("Usage: %s <option>\n", argv[0]);
		return -1;
	}

	option = atoi(argv[1]);

	if (option == 0) {	// List FPGA info
		// Populate the fpga_info_list struct
		if (fpga_list(&info) != 0) {
			fprintf(stderr, "Error populating fpga_info_list\n");
			return -1;
		}
		printf("Number of devices: %d\n", info.num_fpgas);
		for (i = 0; i < info.num_fpgas; i++) {
			printf("%d: id:%d\n", i, info.id[i]);
			printf("%d: num_chnls:%d\n", i, info.num_chnls[i]);
			printf("%d: name:%s\n", i, info.name[i]);
			printf("%d: vendor id:%04X\n", i, info.vendor_id[i]);
			printf("%d: device id:%04X\n", i, info.device_id[i]);
		}
	}
	else if (option == 1) { // Reset FPGA
		if (argc < 3) {
			printf("Usage: %s %d <fpga id>\n", argv[0], option);
			return -1;
		}

		id = atoi(argv[2]);

		// Get the device with id
		fpga = fpga_open(id);
		if (fpga == NULL) {
			fprintf(stderr, "Could not get FPGA %d\n", id);
			return -1;
	    }

		// Reset
		fpga_reset(fpga);

		// Done with device
        fpga_close(fpga);
	}
	else if (option == 2) { // Send data, receive data

		id = 0;
		chnl = 2; // channel 2 for user requests, channel 0,1 for fpga requests

		// Get the device with id
		fpga = virtmem_open(id);
		if (fpga == NULL) {
			fprintf(stderr, "Could not get FPGA %d\n", id);
			return -1;
	    }

		// Malloc the arrays
		size_t sendBuffer_size = sizeof(whoosh_args);
		size_t recvBuffer_size = 4 * sizeof(unsigned int);
		// fprintf(stderr, "sizeof(unsigned int *) = %d, sizeof(unsigned int **) = %d\n", sizeof(unsigned int *), sizeof(unsigned int **));
		sendBuffer = (whoosh_args *)malloc(sendBuffer_size);
		if (sendBuffer == NULL) {
			fprintf(stderr, "Could not malloc memory for sendBuffer\n");
			virtmem_close(fpga);
			return -1;
	    }
		recvBuffer = (unsigned int *)malloc(recvBuffer_size);
		if (recvBuffer == NULL) {
			fprintf(stderr, "Could not malloc memory for recvBuffer\n");
			free(sendBuffer);
			virtmem_close(fpga);
			return -1;
	    }

		// Initialize the data
	    for (int i = 0; i < DATA_SZ; ++i)
	    {
	    	data[i] = i;
	    }
		
		sendBuffer->data_start = data;
		sendBuffer->xy_range = DATA_SZ;
		fprintf(stderr, "Array 'data' at %p\n", &(data[0]));

		// Send the data
		sent = fpga_send(fpga, chnl, sendBuffer, sendBuffer_size / 4, 0, 1, 5000);
		fprintf(stderr, "Start command sent: %d words\n", sent);

		recvd = 0;
		while (!recvd) {
			// Recv the data
			recvd = fpga_recv(fpga, chnl, &(recvBuffer[recvd]), (recvBuffer_size / 4) - recvd, 5000);
		}
		fprintf(stderr, "Calculation finish notification: %d words processed\n", recvBuffer[0]);

		//send flush command
		virtmem_flush(fpga);

		// Done with device
        virtmem_close(fpga);

		// Check the data
		int num_errors = 0;
		if (recvd != 0) {
			for (i = 0; i < DATA_SZ - 2; i++) {
				if(data[i] != (i ^ (i+1) ^ (i+2))){
					num_errors++;
					if (num_errors < 10) {
						fprintf(stderr, "data[%d]: %d (should be %d).\n", i, data[i], i ^ (i+1) ^ (i+2));
					}
				}
			}
			i = DATA_SZ - 2;
			if(data[i] != i){
				num_errors++;
				if (num_errors < 10) {
					fprintf(stderr, "data[%d]: %d (should be %d).\n", i, data[i], i);
				}
			}
			i++; //i = DATA_SZ - 1;
			if(data[i] != i){
				num_errors++;
				if (num_errors < 10) {
					fprintf(stderr, "data[%d]: %d (should be %d).\n", i, data[i], i);
				}
			}

			if(num_errors>0){
				if(num_errors > 10){
					fprintf(stderr, "Further errors suppressed.\n");
				}
				fprintf(stderr, "%d error(s) detected in total.\n", num_errors);
			} else {
				fprintf(stderr, "No errors detected.\n");
			}

		}
	}

	return 0;
}
