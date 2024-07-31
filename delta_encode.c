#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "delta_head.h"

int srcSize, modSize, dltaSize;



int delta_encode(unsigned char *srcFile, unsigned char *modFile, unsigned char *dltaFile) {
  int cnt, s, m, ss, mm, sm, minM;
  unsigned char *srcPtr = srcFile, *modPtr = modFile, *dltaPtr = dltaFile;

  //write file type header
  *dltaPtr++ = 'd'; *dltaPtr++ = 'l'; *dltaPtr++ = 't'; *dltaPtr++ = 'a';

  //start run
  while(*srcPtr && *modPtr) {
    if(*srcPtr == *modPtr) { //same run
      cnt = 0;
      while(*srcPtr && *modPtr && *srcPtr == *modPtr && cnt < MAX_RUN) {
        srcPtr += 1;
        modPtr += 1;
        cnt += 1;
      }
      *dltaPtr++ = SAME_RUN;
      *dltaPtr++ = cnt;
    }
    else { //determine del, ins, repl
      //find rejoin point
      s = ss = mm = 0;
      minM = sm = INT_MAX;
      while(srcPtr[s]) {
        m = 0;
        while(modPtr[m]) {
          if(srcPtr[s] == modPtr[m]) {
            cnt = 0;
            for(int i = 0; i < MIN_UNBREAK_LEN; i++)
              cnt += (srcPtr[s+i] != 0) & (modPtr[m+i] != 0) & (srcPtr[s+i] == modPtr[m+i]);
            if(cnt == MIN_UNBREAK_LEN) {
              if(sm > s + m) {
                sm = s + m;
                ss = s;
                mm = m;
              }
              minM = (minM > m) ? m : minM;
              if(minM <= s) goto rejoin;
            }
          }
          m += 1;
        }
        s += 1;
      }

      //rejoin not found
      break; //goto tail

      //rejoin found
      rejoin:
      s = ss; m = mm;
      srcPtr += s;
      modPtr += m;

      if(s == m) { //replace
        while(s > 0) {
          *dltaPtr++ = REPL_RUN;
          *dltaPtr = (s > MAX_RUN) ? MAX_RUN : s;
          memcpy(&dltaPtr[1], &modPtr[-s], *dltaPtr);
          s -= MAX_RUN;
          dltaPtr += 1 + *dltaPtr;
        }
      }
      else if(s == 0) { //insert
        while(m > 0) {
          *dltaPtr++ = INS_RUN;
          *dltaPtr = (m > MAX_RUN) ? MAX_RUN : m;
          memcpy(&dltaPtr[1], &modPtr[-m], *dltaPtr);
          m -= MAX_RUN;
          dltaPtr += 1 + *dltaPtr;
        }
      }
      else if(m == 0) { //delete
        while(s > 0) {
          *dltaPtr++ = DEL_RUN;
          *dltaPtr++ = (s > MAX_RUN) ? MAX_RUN : s;
          s -= MAX_RUN;
        }
      }
      else { //del + ins
        while(s > 0) {
          *dltaPtr++ = DEL_RUN;
          *dltaPtr++ = (s > MAX_RUN) ? MAX_RUN : s;
          s -= MAX_RUN;
        }
        while(m > 0) {
          *dltaPtr++ = INS_RUN;
          *dltaPtr = (m > MAX_RUN) ? MAX_RUN : m;
          memcpy(&dltaPtr[1], &modPtr[-m], *dltaPtr);
          m -= MAX_RUN;
          dltaPtr += 1 + *dltaPtr;
        }
      }
    }
  }

  //tail

  if(*modPtr) { //append mod tail (and truncate src)
    cnt = strlen((char *)modPtr);
    *dltaPtr++ = REMAINING_RUN;
    memcpy(dltaPtr, modPtr, cnt);
    dltaPtr += cnt;
  }

  else if(*srcPtr) { //del src tail
    *dltaPtr++ = TRUNCATE_RUN;
  }


  return dltaPtr - dltaFile;
}



int main(int argc, char const* argv[]) {
  if(argc != 4) {
    write(STDOUT_FILENO, "\ndelta encode\n\nUsage: ", 22);
    write(STDOUT_FILENO, argv[0], strlen(argv[0]));
    write(STDOUT_FILENO, " <src file> <modified file> <patch file>\nsaves .dlta patch file that converts src to modified\n\n", 95);
    return 1;
  }

  if(access(argv[1], F_OK|R_OK|W_OK) != 0) return 7;
  if(access(argv[2], F_OK|R_OK|W_OK) != 0) return 6;

  int fd = open(argv[1], O_RDONLY); //src file
  if(fd == -1) return 5;
  struct stat sb;
  fstat(fd, &sb);
  srcSize = sb.st_size;

  unsigned char* srcFile = malloc(srcSize); //src buff
  int rc = 0;
  while(rc < srcSize)
    rc += read(fd, srcFile + rc, srcSize - rc);
  close(fd);

  fd = open(argv[2], O_RDONLY); ///mod file
  if(fd == -1) return 4;
  fstat(fd, &sb);
  modSize = sb.st_size;
  if(srcSize < 14 || modSize < 14) return 3;

  unsigned char* modFile = malloc(modSize); //mod buff
  rc = 0;
  while(rc < modSize)
    rc += read(fd, modFile + rc, modSize - rc);
  close(fd);

  dltaSize = srcSize + modSize; //max possible size
  unsigned char* dltaFile = malloc(dltaSize); //patch buff

  dltaSize = delta_encode(srcFile, modFile, dltaFile);
  if(dltaSize < 6) goto cleanup_exit;

  if(dltaSize > modSize) { //just copy modFile, if patch is larger
    dltaFile[4] = REMAINING_RUN;
    memcpy(&dltaFile[5], modFile, modSize);
    dltaSize = modSize + 5;
  }

  //save patch file
  umask(S_IWOTH);
  fd = open(argv[3], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP); //perms 660
  if(fd == -1) return 2;
  rc = 0;
  while(rc < dltaSize)
    rc += write(fd, dltaFile + rc, dltaSize - rc);
  close(fd);

  cleanup_exit:
  free(dltaFile);
  free(modFile);
  free(srcFile);
  return (dltaSize < 6);
}
