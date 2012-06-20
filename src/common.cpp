#include "common.h"
#define PRINTFILE "/tmp/trace2"

void printrarread(const char *buf,size_t size, size_t offset) {
  FILE *f = fopen(PRINTFILE,"a");
  fprintf(f,"RAR_READ %lld @ %lld\n",size,offset);
  printf("RAR_READ %lld @ %lld\n",size,offset);
  for(int i=0;i<size;i++) {
    fprintf(f,"%02x ", buf[i] & 0xff);
  }
  fprintf(f,"\n");
  fclose(f);
  sleep(1);
}

void printrarseek(size_t nr, int type, size_t offset) {
  FILE *f = fopen(PRINTFILE,"a");
  size_t newoffset = offset;
  switch(type) {
    case SEEK_SET: newoffset = nr; break;
    case SEEK_CUR: newoffset += nr; break;
    case SEEK_END: newoffset = 999999; break;
  }
  fprintf(f,"RAR_SEEK %lld > %lld (%d)\n", offset, newoffset, type);
  printf("RAR_SEEK %lld > %lld (%d)\n", offset, newoffset, type);
  fclose(f);
  sleep(1);
}