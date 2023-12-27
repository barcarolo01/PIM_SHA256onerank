#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <dpu.h>
#include <time.h>
#include <dpu_log.h>

#ifndef DPU_EXE
#define DPU_EXE "./SHA256DPU"
#endif


#define NUM NRDPU*NR_TASKLETS
char* msg;
uint32_t digests[NRDPU*NR_TASKLETS*8];


static inline double my_clock(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC_RAW, &t);
  return (1.0e-9 * t.tv_nsec + t.tv_sec);
}

int main()
{
	msg = malloc(32*16*NRDPU*MESSAGE_SIZE);
	double FileTimeDPU=0,CPUtoDPU=0,DPUtime=0,DPUtoCPU=0;
	int processedFileDPU=0,processedFileHost=0;
	struct dpu_set_t set, dpu;
	double tmpTimer[5],initTime,endTime;
	uint32_t each_dpu;
	uint32_t* digests_DPU = malloc(8*16*NRDPU*sizeof(uint32_t));
	uint32_t* digests_HOST = malloc(8*16*NRDPU*sizeof(uint32_t));
	DPU_ASSERT(dpu_alloc(NRDPU, NULL, &set));	//Allocating DPUs
	printf("--Allocated %d DPUs\n",NRDPU);
	printf("--Using %d tasklets\n",NR_TASKLETS);
//	printf("Number of messages: %d\t MESSAGE_SIZE: %d kb\t TOTAL DATA SIZE = %d Mb\n",NUM_MSG,MESSAGE_SIZE/1024,(NUM_MSG*MESSAGE_SIZE)/(1024*1024));
	DPU_ASSERT(dpu_load(set, DPU_EXE, NULL));	//Loading DPU program

	//INIT PIM HASHING
	initTime= my_clock();	//START Measuring performance
	tmpTimer[2] = my_clock();
	DPU_FOREACH(set,dpu,each_dpu)
	{
		for(int j=0;j<NR_TASKLETS;++j)
		{
			readFile(msg+MESSAGE_SIZE*j,processedFileDPU);
			uint32_t t=padding(msg+MESSAGE_SIZE*j,MESSAGE_SIZE-9);
			processedFileDPU++;
		}
		DPU_ASSERT(dpu_copy_to(dpu,"msgs",0,msg,MESSAGE_SIZE*NR_TASKLETS));	
	}
	CPUtoDPU += my_clock() - tmpTimer[2];
	
	tmpTimer[3] = my_clock();
	DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
	DPUtime = my_clock() - tmpTimer[3];
		
	tmpTimer[4]=my_clock();
	DPU_FOREACH(set,dpu,each_dpu)
	{
		DPU_ASSERT(dpu_copy_from(dpu,"hash_digests",0,digests_DPU+8*NR_TASKLETS*each_dpu,8*NR_TASKLETS*sizeof(uint32_t)));	
	}
	DPUtoCPU = my_clock() - tmpTimer[4];
	endTime = my_clock();	
	//END PIM HASHING
	
	
	
	
	tmpTimer[3] = my_clock();
	for(int i=0;i<NRDPU*NR_TASKLETS;++i)
	{
		readFile(msg,processedFileHost);
		uint32_t paddedLength = padding(msg,MESSAGE_SIZE-9);
		SHA256_t(msg,digests_HOST+i*8,MESSAGE_SIZE,0);
		processedFileHost++;
		//printf("HOST %d:\n",i); for(int j=0;j<8;++j){printf("%08X",digests_HOST[i*8+j]);} printf("\n");
	}
	tmpTimer[4] = my_clock();
	DPU_ASSERT(dpu_free(set));
	
	
	int z=0, err=0;
	for(z=0;z<8*NRDPU*NR_TASKLETS;++z){ if(digests_DPU[z]!=digests_HOST[z]) {err = 1;} }
	if(err==0){ printf("[\033[1;32mOK\033[0m] Digests are equals\n\n"); }
	else{ printf("[\033[1;31mERROR\033[0m] Digests are NOT equals: z=%d\n\n",z); }
	
	
	printf("HOST: %d DPU: %d\t Each DPU hashed %d kB\n\n",processedFileHost,processedFileDPU,(processedFileDPU*MESSAGE_SIZE)/(1024*NRDPU));
	printf("-CPU-DPU transfer time: %.1f ms.\n", 1000*(CPUtoDPU));
	printf("-DPU kernel time: %.1f ms.\n", 1000*(DPUtime));
	printf("-DPU-CPU kernel time: %.1f ms.\n", 1000*(DPUtoCPU));
	printf("--Total host elapsed time: %.1f ms.\n", 1000*(endTime-initTime));
	printf("--Hashing time on HOST CPU: %.1f ms.\n", 1000*(tmpTimer[4]-tmpTimer[3]));
	printf("-------------------------------------------------\n");
    return 0;
}
