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


#define NUM_MSG 1024

#define NUM NRDPU*NR_TASKLETS
char msg[MESSAGE_SIZE*24];
char digests[16*NRDPU*NR_TASKLETS];


static inline double my_clock(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC_RAW, &t);
  return (1.0e-9 * t.tv_nsec + t.tv_sec);
}

void readFile(unsigned char* msg, int index)
{
	char filename[50];
	snprintf(filename, sizeof(filename), "./MyFiles/file%d.txt",index);
	FILE* fp = fopen(filename,"r");
	fread(msg,sizeof(unsigned char),MESSAGE_SIZE-9,fp); //'-9' required in order to get a total size of 1kB (with the padding bytes)
	fclose(fp);
}

int main()
{
	double FileTimeDPU=0,CPUtoDPU=0,DPUtime=0,DPUtoCPU=0;
	int d=0;
	int processedFileDPU=0;
	int processedFileHost=0;
	struct dpu_set_t set, dpu;
	double tmpTimer[5];
	uint32_t each_dpu,nr_instructions,transferTime,numberOfTransfer,clockPerSec,SHAtimeDPU;
	uint32_t digests_DPU[8*24*64];
	uint32_t digests_HOST[8*24*64];
	DPU_ASSERT(dpu_alloc(NRDPU, NULL, &set));	//Allocating DPUs
	printf("--Allocated %d DPUs\n",NRDPU);
	printf("--Using %d tasklets\n",NR_TASKLETS);
	printf("Number of messages: %d\t MESSAGE_SIZE: %d kb\t TOTAL DATA SIZE = %d Mb\n",NUM_MSG,MESSAGE_SIZE/1024,(NUM_MSG*MESSAGE_SIZE)/(1024*1024));
        DPU_ASSERT(dpu_load(set, DPU_EXE, NULL));	//Loading DPU program

	double initTime= my_clock();	//START Measuring performance
	for(processedFileDPU=0;processedFileDPU<NUM_MSG;d++)
	{
		DPU_FOREACH(set,dpu,each_dpu)
		{
			tmpTimer[0] = my_clock();
			for(int j=0;j<NR_TASKLETS && processedFileDPU < NUM_MSG;++j)
			{
				readFile(msg+MESSAGE_SIZE*j,processedFileDPU%8);
				uint32_t t=padding(msg+MESSAGE_SIZE*j,MESSAGE_SIZE-9);
				processedFileDPU++;
			}
			FileTimeDPU += my_clock() - tmpTimer[0];

			tmpTimer[2] = my_clock();
			DPU_ASSERT(dpu_copy_to(dpu,"msgs",0,msg,MESSAGE_SIZE*NR_TASKLETS));
			CPUtoDPU += my_clock() - tmpTimer[2];
		}

			tmpTimer[3] = my_clock();
			DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
			DPUtime += my_clock() - tmpTimer[3];
		DPU_FOREACH(set,dpu,each_dpu)
		{
			tmpTimer[4] = my_clock();
			DPU_ASSERT(dpu_copy_from(dpu,"hash_digests",0,digests_DPU+8*NR_TASKLETS*each_dpu+d*NRDPU*8*NR_TASKLETS,8*NR_TASKLETS*sizeof(uint32_t)));	
			DPUtime += my_clock() - tmpTimer[4];
		}
	}
	double endTime = my_clock();


	DPU_FOREACH(set,dpu,each_dpu)
	{
		//DPU_ASSERT(dpu_log_read(dpu, stdout));
		if(each_dpu==0)
		{
			DPU_ASSERT(dpu_copy_from(dpu,"nr_instructions",0,&nr_instructions,sizeof(uint32_t)));
			DPU_ASSERT(dpu_copy_from(dpu,"transferTime",0,&transferTime,sizeof(uint32_t)));
			DPU_ASSERT(dpu_copy_from(dpu,"numberOfTransfer",0,&numberOfTransfer,sizeof(uint32_t)));
			DPU_ASSERT(dpu_copy_from(dpu,"CLOCKS_PER_SEC",0,&clockPerSec,sizeof(uint32_t)));
			DPU_ASSERT(dpu_copy_from(dpu,"SHAtime",0,&SHAtimeDPU,sizeof(uint32_t)));
		}
		//DPU_ASSERT(dpu_copy_from(dpu,"hash_digests",0,digests_DPU+8*NR_TASKLETS*each_dpu,8*NR_TASKLETS*sizeof(uint32_t)));
	}

	
	tmpTimer[3] = my_clock();
	for(int i=0;i<NUM_MSG;++i)
	{
		readFile(msg,processedFileHost%8);
		uint32_t paddedLength = padding(msg,MESSAGE_SIZE-9);
		SHA256_t(msg,digests_HOST+i*8,MESSAGE_SIZE,0);
		processedFileHost++;
		//printf("HOST %d:\n",i); for(int j=0;j<8;++j){printf("%08X",digests_HOST[i*8+j]);} printf("\n");
	}
	tmpTimer[4] = my_clock();
	
	int z=0, err=0;
	for(z=0;z<8*NUM_MSG;++z){ if(digests_DPU[z]!=digests_HOST[z]) {err = 1;} }
	if(err==0){ printf("[\033[1;32mOK\033[0m] Digests are equals\n\n"); }
	else{ printf("[\033[1;31mERROR\033[0m] Digests are NOT equals: z=%d\n\n",z); }
	
	
	DPU_ASSERT(dpu_free(set));
	printf("HOST: %d DPU: %d\n\n",processedFileHost,processedFileDPU);
	
	printf("-CPU-DPU transfer time: %.1f ms.\n", 1000*(CPUtoDPU));
	printf("-DPU kernel time: %.1f ms.\n", 1000*(DPUtime));
/*		printf("\t-Number of MRAM-WRAM transfer: %d\n",numberOfTransfer);
		printf("\t-One MRAM-WRAM transfer time:%.1f ms\t Estimated total=%.1f ms\n",1000.0*transferTime/clockPerSec,(1000.0*transferTime/clockPerSec)*numberOfTransfer);
		printf("\t-One SHA256 cache hashing time:%.1f ms\t Estimated total=%.1f ms\n",1000.0*SHAtimeDPU/clockPerSec,(1000.0*SHAtimeDPU/clockPerSec)*numberOfTransfer);*/
	printf("-DPU-CPU kernel time: %.1f ms.\n", 1000*(DPUtoCPU));
	printf("--Total host elapsed time: %.1f ms.\n", 1000*(endTime-initTime));
	printf("--Hashing time on HOST CPU: %.1f ms.\n", 1000*(tmpTimer[4]-tmpTimer[3]));

	printf("-------------------------------------------------\n");
    return 0;
}
