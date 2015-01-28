#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "riffa.h"
#include "timer.h"

void initialize_matrix(uint16_t* m, int rows, int cols){
	int i,j;
	for (i = 0; i < rows; i++){
		for (j = 0; j < cols; j++)
		{
			m[i * cols + j] = i * cols + j;
		}
	}
}

void multiply_matrices(uint16_t* m1, uint16_t* m2, uint16_t* res, int resrows, int rescols, int m1cols){
	int i, j, k;
	for (i = 0; i < resrows; i++){
		for (j = 0; j < rescols; j++)
		{
			res[i * rescols + j] = 0;
			for (k = 0; k < m1cols; k++)
			{
				res[i * rescols + j] += m1[i * m1cols + k] * m2[k * rescols + j];
			}
		}
	}
}

typedef struct {
	uint16_t* a;
	uint16_t* b;
	uint16_t* c;
	uint64_t dim_i;
	uint64_t dim_j;
	uint64_t dim_k;
} fpga_args;

typedef struct {
	uint64_t cycles;
} fpga_ret;

int multiply_matrices_on_fpga(uint16_t* m1, uint16_t* m2, uint16_t* res, int resrows, int rescols, int m1cols){
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

	arg_struct->a = m1;
	arg_struct->b = m2;
	arg_struct->c = res;
	arg_struct->dim_i = resrows;
	arg_struct->dim_j = rescols;
	arg_struct->dim_k = m1cols;

	int sent = 0;
	sent = fpga_send(fpga, chnl, arg_struct, sizeof(fpga_args) / 4, 0, 1, 5000);
	if(sent < sizeof(fpga_args) / 4){
		printf("Could not send arguments to FPGA.\n");
		return -1;
	}

	fpga_ret* recv_struct;

	recv_struct = memalign(4, sizeof(fpga_ret));

	int recvd = 0;
	recvd = fpga_recv(fpga, chnl, recv_struct, ( sizeof(fpga_ret) >> 2) + 1, 5000);
	if(!recvd){

		printf("Waiting for FPGA response timed out.\n");
		fpga_reset(fpga);
		fpga_close(fpga);

		return -1;
	}

	printf("FPGA reports matrix multiply completed in %lu cycles.\n", recv_struct->cycles);

	fpga_flush(fpga);

	fpga_reset(fpga);
	
	fpga_close(fpga);

	return 0;
}

int count_differences(uint16_t* m1, uint16_t* m2, int rows, int cols){
	int diff = 0;
	int i, j;
	for (i = 0; i < rows; ++i)
	{
		for (j = 0; j < cols; ++j)
		{
			if(m1[i * cols + j] != m2[i * cols + j]){
				diff++;
			}
		}
	}
	return diff;
}

#define dim_i 62                 /* number of rows in matrix A */
#define dim_k 15                 /* number of columns in matrix A */
#define dim_j 7                  /* number of columns in matrix B */

uint16_t*	a,           /* matrix A to be multiplied */
		*	b,           /* matrix B to be multiplied */
		*	c,           /* result matrix C */
	 * gold_c;           /* comparison matrix C calculated on CPU*/


int main (int argc, char *argv[]) {
	GET_TIME_INIT(3);

	long page_size = sysconf(_SC_PAGESIZE);

	//size_t size_a = page_size * ((dim_i * dim_k * sizeof(uint16_t) + page_size - 1) / page_size);
	//size_t size_b = page_size * ((dim_k * dim_j * sizeof(uint16_t) + page_size - 1) / page_size);
	//size_t size_c = page_size * ((dim_i * dim_j * sizeof(uint16_t) + page_size - 1) / page_size);

	size_t size_a = dim_i * dim_k * sizeof(uint16_t); 
	size_t size_b = dim_k * dim_j * sizeof(uint16_t);
	size_t size_c = dim_i * dim_j * sizeof(uint16_t);

	a = memalign(page_size, size_a);
	b = memalign(page_size, size_b);
	c = memalign(page_size, size_c);
	gold_c = memalign(page_size, size_c);

	printf("Adresses: a = %p, b = %p, c = %p, gold_c = %p\n", a, b, c, gold_c);
	
	initialize_matrix(a, dim_i, dim_k);
	initialize_matrix(b, dim_k, dim_j);

	GET_TIME_VAL(0);
	int error = multiply_matrices_on_fpga(a, b, c, dim_i, dim_j, dim_k);
	GET_TIME_VAL(1);

	if(error){
		return -1;
	}

	GET_TIME_VAL(2);
	multiply_matrices(a, b, gold_c, dim_i, dim_j, dim_k);
	GET_TIME_VAL(3);

	int differences = count_differences(c, gold_c, dim_i, dim_j);

	if(differences){
		printf("FAIL: %d values differ between matrices.\n", differences);
		return -1;
	}

	printf("SUCCESS: matrices are identical.\n");

	printf("FPGA: %f s\n", ((TIME_VAL_TO_MS(1) - TIME_VAL_TO_MS(0))/1000.0));
	printf("CPU:  %f s\n", ((TIME_VAL_TO_MS(3) - TIME_VAL_TO_MS(2))/1000.0));
	return 0;
}
