#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILESIZE (1 << 22)
#define NR_FILE 8

struct stat st = {0};
unsigned char buffer[FILESIZE];
char filename[50];

int main()
{
    srand(time(NULL));
if (stat("./MyFiles", &st) == -1) {
    mkdir("./MyFiles", 0777);
}
    for(int j=0;j<NR_FILE;++j)
    {
        snprintf(filename, sizeof(filename), "./MyFiles/file%d.txt",j);

        FILE* wr = fopen(filename,"w");
        for(int i=0;i<FILESIZE;++i)
        {
            buffer[i] = 'A' + (rand()%26);
        }
        fwrite(buffer,sizeof(unsigned char),FILESIZE,wr);
        fclose(wr);
    }
    return 0;
}
