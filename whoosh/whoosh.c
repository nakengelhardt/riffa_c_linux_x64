#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include "riffa.h"

#define DATA_SZ 4096
unsigned int __attribute__ ((aligned (4096))) data[DATA_SZ];

int min(int a, int b){
	return a > b ? b : a;
}

typedef struct {
	fpga_t* fpga;
	int numTransactions;
} fetchdata_args_t;

void* fetchdata(void* v_args) {
	fetchdata_args_t* args = (fetchdata_args_t*) v_args;
	fpga_t* fpga = args->fpga;
	int i;
	int cmd_chnl = 0;
	int data_chnl = 1;
	// int numTransactions = args->numTransactions;
	int recvd, total_recvd;
	unsigned int * sendBuffer;
	void ** recvBuffer;


	// Malloc the arrays
	size_t sendBuffer_size = 4096;
	size_t recvBuffer_size = 4 * sizeof(unsigned int);
	sendBuffer = (unsigned int*)malloc(sendBuffer_size);
	if (sendBuffer == NULL) {
		fprintf(stderr, "Could not malloc memory for sendBuffer\n");
		fpga_close(fpga);
		exit(-1);
    }

	recvBuffer = (void **)malloc(recvBuffer_size);
	if (recvBuffer == NULL) {
		fprintf(stderr, "Could not malloc memory for recvBuffer\n");
		free(sendBuffer);
		fpga_close(fpga);
		exit(-1);
    }

    for (i = 0; i < sendBuffer_size/sizeof(unsigned int); i++) {
		sendBuffer[i] = 31337; //(rand() % DATA_SZ);
	}
	for (i = 0; i < recvBuffer_size/sizeof(void*); i++){
		recvBuffer[i] = (void*) 0xdeadbeefdeadbeef;
	}


	total_recvd = 0;
    while(1){ // can't really predict how many pages will be req'd but this is max: total_recvd < numTransactions * 16 / sizeof(void*)
		recvd = fpga_recv(fpga, cmd_chnl, recvBuffer, 4, 25000);
		if(recvd>0){
			total_recvd += recvd;
			// fprintf(stderr, "%d words received\n", recvd);
			if(recvBuffer[1] == (void*)0x6E706E706E706E70) {
				unsigned int * addr = recvBuffer[0];
				if(addr < data || addr > data + DATA_SZ - 1024){
					fprintf(stderr, "Received fetch for address %p not in array 'data' (%p).\n", addr, data);
					fpga_send(fpga, data_chnl, sendBuffer, sendBuffer_size / 4, 0, 1, 25000);
				} else {
					fprintf(stderr, "Received address %p, sending page (size %lu).\n", addr, sendBuffer_size / 4);
					fpga_send(fpga, data_chnl, addr, sendBuffer_size / 4, 0, 1, 25000);
				}
			}
			if(recvBuffer[1] == (void*)0x61B061B061B061B0) {
				unsigned int * addr = recvBuffer[0];
				if(addr < data || addr > data + DATA_SZ - 1024){
					fprintf(stderr, "Received writeback for address %p not in array 'data' (%p).\n", addr, data);
					fpga_recv(fpga, data_chnl, recvBuffer, recvBuffer_size / 4, 25000);
				} else {
					fprintf(stderr, "Received address %p, writing back page (size %lu).\n", addr, sendBuffer_size / 4);
					fpga_recv(fpga, data_chnl, addr, sendBuffer_size / 4, 25000);
				}
			}
			if(recvBuffer[1] == (void*)0xD1DF1005D1DF1005) {
				free(sendBuffer);
				free(recvBuffer);
				pthread_exit(v_args);
			}
		}
	}

	free(sendBuffer);
	free(recvBuffer);

	return v_args;
}

typedef struct { 
	void* data_start;
	uint32_t x_range;
	uint32_t y_range;
} whoosh_args;


int main(int argc, char** argv) {
	fpga_t * fpga;
	fpga_info_list info;
	int option;
	int i;
	int id;
	int chnl;
	size_t numTransactions;
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
		if (argc < 4) {
			printf("Usage: %s %d <fpga id> <num words to transfer>\n", argv[0], option);
			return -1;
		}

		id = atoi(argv[2]);
		numTransactions = atoi(argv[3]);
		chnl = 2; // channel 2 for user requests, channel 0,1 for fpga requests

		// Get the device with id
		fpga = fpga_open(id);
		if (fpga == NULL) {
			fprintf(stderr, "Could not get FPGA %d\n", id);
			return -1;
	    }

	    // //create the thread that will answer data requests on channel 1
	    fetchdata_args_t f_args;
		f_args.fpga = fpga;
		f_args.numTransactions = numTransactions;
	    pthread_t answerthread;
	    pthread_create(&answerthread, NULL, &fetchdata, (void*)&f_args);

		// Malloc the arrays
		size_t sendBuffer_size = sizeof(whoosh_args);
		size_t recvBuffer_size = 4 * sizeof(unsigned int);
		// fprintf(stderr, "sizeof(unsigned int *) = %d, sizeof(unsigned int **) = %d\n", sizeof(unsigned int *), sizeof(unsigned int **));
		sendBuffer = (whoosh_args *)malloc(sendBuffer_size);
		if (sendBuffer == NULL) {
			fprintf(stderr, "Could not malloc memory for sendBuffer\n");
			fpga_close(fpga);
			return -1;
	    }
		recvBuffer = (unsigned int *)malloc(recvBuffer_size);
		if (recvBuffer == NULL) {
			fprintf(stderr, "Could not malloc memory for recvBuffer\n");
			free(sendBuffer);
			fpga_close(fpga);
			return -1;
	    }

		// Initialize the data
	    for (int i = 0; i < DATA_SZ; ++i)
	    {
	    	data[i] = i;
	    }
		
		sendBuffer->data_start = data;
		sendBuffer->x_range = 32;
		sendBuffer->y_range = 32;
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
		sendBuffer->data_start = (void*)0xF1005;
		sendBuffer->x_range = 0;
		sendBuffer->y_range = 0;
		sent = fpga_send(fpga, chnl, sendBuffer, sendBuffer_size / 4, 0, 1, 5000);
		fprintf(stderr, "Flush command sent: %d words\n", sent);

		// // we're done, stop the answering thread
		// pthread_cancel(answerthread);
		pthread_join(answerthread, NULL);

		//clean up whatever might be stuck in the design
		fpga_reset(fpga);

		// Done with device
        fpga_close(fpga);

		// Check the data
		int num_errors = 0;
		if (recvd != 0) {
			for (i = 0; i < 32*32 - 3; i++) {
				if(data[i] != (i ^ (i+1) ^ (i+2))){
					num_errors++;
					if (num_errors < 10) {
						fprintf(stderr, "data[%d]: %d (should be %d).\n", i, data[i], i ^ (i+1) ^ (i+2));
					}
				}
			}
			//i = 32*32 - 2
			if(data[i] != i){
				num_errors++;
				if (num_errors < 10) {
					fprintf(stderr, "data[%d]: %d (should be %d).\n", i, data[i], i);
				}
			}
			i++; //i = 32*32 - 1
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
