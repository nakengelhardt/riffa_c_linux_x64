#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include "virtmem.h"

struct fpga_t
{
        int fd;
        int id;
};

int keep_open[NUM_FPGAS];
pthread_t virtmem_servicethread[NUM_FPGAS];
int flush_req[NUM_FPGAS];
sem_t flush_done[NUM_FPGAS];

void* fetchdata(void* arg) {
	fpga_t* fpga = (fpga_t*) arg;
	int i;
	int cmd_chnl = 0;
	int data_chnl = 1;
	int recvd;
	void ** recvBuffer;


	// Malloc the arrays
	size_t sendBuffer_size = 4096;
	size_t recvBuffer_size = 32 * sizeof(unsigned int);

	recvBuffer = (void **)malloc(recvBuffer_size);
	if (recvBuffer == NULL) {
		fprintf(stderr, "Could not malloc memory for recvBuffer\n");
		exit(-1);
    }

	for (i = 0; i < recvBuffer_size/sizeof(void*); i++){
		recvBuffer[i] = (void*) 0xdeadbeefdeadbeef;
	}

    while(keep_open[fpga->id]){
		recvd = fpga_recv(fpga, cmd_chnl, recvBuffer, recvBuffer_size / 4, 100);
		if(recvd>0){
			if(recvd != 4){
				fprintf(stderr, "received more than one transaction: recvd=%d\n", recvd);
			}
			// fprintf(stderr, "%d words received\n", recvd);
			if(recvBuffer[1] == (void*)0x6E706E706E706E70) {
				unsigned int * addr = recvBuffer[0];
				fprintf(stderr, "Received address %p, sending page (size %lu)... ", addr, sendBuffer_size / 4);
				fpga_send(fpga, data_chnl, addr, sendBuffer_size / 4, 0, 1, 1000);
				fprintf(stderr, "done.\n");
			}
			else if(recvBuffer[1] == (void*)0x61B061B061B061B0) {
				unsigned int * addr = recvBuffer[0];
				int num_wb = 0;
				fprintf(stderr, "Received address %p, writing back page... ", addr);
				num_wb = fpga_recv(fpga, data_chnl, addr, sendBuffer_size / 4, 1000);
				fprintf(stderr, "done (size %u).\n", num_wb);
				}
			else if(recvBuffer[1] == (void*)0xD1DF1005D1DF1005) {
				fprintf(stderr, "Flush finish notification: %lu\n", (unsigned long int) recvBuffer[0]);
				if(flush_req[fpga->id]){
					flush_req[fpga->id] = 0;
					sem_post(&flush_done[fpga->id]);
				}
			}
			else {
				fprintf(stderr, "Received unknown command: %p.\n", recvBuffer[1]);
			}
		}
	}

	free(recvBuffer);

	return NULL;
}

void virtmem_flush(fpga_t* fpga){
	flush_req[fpga->id] = 1;
	int sendBuffer[4] = {0xF1005,0,0,0};
	int sent = fpga_send(fpga, 0, sendBuffer, 4, 0, 1, 5000);
	fprintf(stderr, "Flush command sent: %d words\n", sent);
	sem_wait(&flush_done[fpga->id]);
}


fpga_t * virtmem_open(int id)
{
	fpga_t * fpga;

	// Open FPGA
	fpga = fpga_open(id);
	if (fpga == NULL)
		return NULL;

	// Launch the service thread
	keep_open[id] = 1;
	flush_req[id] = 0;
	sem_init(&flush_done[id], 0, 0);

	pthread_create(&virtmem_servicethread[id], NULL, &fetchdata, (void *) fpga);

	return fpga;
}

void virtmem_close(fpga_t * fpga)
{
	//tell the FPGA to invalidate all pages
	int sendBuffer[4] = {0xC105E,0,0,0};
    fpga_send(fpga, 0, sendBuffer, 4, 0, 1, 5000);

    //stop helper thread
	keep_open[fpga->id] = 0;
	pthread_join(virtmem_servicethread[fpga->id], NULL);

	//close fd
	fpga_close(fpga);
}

