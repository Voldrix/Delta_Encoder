#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "delta_head.h"

int srcSize, modSize, dltaSize, newSize;



int delta_decode(unsigned char *srcFile, unsigned char *dltaFile, unsigned char *newFile) {
  unsigned char *newFileHead = newFile, *dltaEnd = &dltaFile[dltaSize];
  dltaFile += 4; //file type header

  while(dltaFile < dltaEnd) {
    switch(*dltaFile) {
      case SAME_RUN:
          memcpy(newFile, srcFile, dltaFile[1]); //same  cp src
          newFile += dltaFile[1];
          srcFile += dltaFile[1];
          dltaFile += 2;
          break;
      case DEL_RUN:
          srcFile += dltaFile[1]; //del - skip src
          dltaFile += 2;
          break;
      case INS_RUN: //ins - cp dlta
      case REPL_RUN:
          memcpy(newFile, &dltaFile[2], dltaFile[1]); //repl - cp dlta
          newFile += dltaFile[1];
          srcFile += (*dltaFile == 4) * dltaFile[1]; //inc src for repl only
          dltaFile += 2 + dltaFile[1];
          break;
      case REMAINING_RUN:
          dltaFile += 1;
          memcpy(newFile, dltaFile, dltaEnd - dltaFile); //ins all remaining
          newFile += dltaEnd - dltaFile;
          dltaFile = dltaEnd; //trunc any remaining src
          break;
      case TRUNCATE_RUN:
          dltaFile = dltaEnd; //del rest of src
          break;
      default: return 0;
    }
  }

  return newFile - newFileHead;
}



int main(int argc, char const* argv[]) {
  if(argc < 3) {
    write(STDOUT_FILENO, "\ndelta decode\n\nUsage: ", 22);
    write(STDOUT_FILENO, argv[0], strlen(argv[0]));
    write(STDOUT_FILENO, " <src file> <.dlta file> [save filename]\nsaves to new file name, or prints to stdout if omitted\n\n", 97);
    return 1;
  }

  if(access(argv[1], F_OK|R_OK|W_OK) != 0) return 7;
  if(access(argv[2], F_OK|R_OK|W_OK) != 0) return 6;

  int fd = open(argv[1], O_RDONLY); //src file
  if(fd == -1) return 5;
  struct stat sb;
  fstat(fd, &sb);
  srcSize = sb.st_size;

  void* srcFile = malloc(srcSize); //src buff
  int rc = 0;
  while(rc < srcSize)
    rc += read(fd, srcFile + rc, srcSize - rc);
  close(fd);

  fd = open(argv[2], O_RDONLY); //patch file
  if(fd == -1) return 4;
  fstat(fd, &sb);
  dltaSize = sb.st_size;
  if(srcSize < 12 || dltaSize < 12) return 3;

  void* dltaFile = malloc(dltaSize); //patch buff
  rc = 0;
  while(rc < dltaSize)
    rc += read(fd, dltaFile + rc, dltaSize - rc);
  close(fd);

  newSize = srcSize + dltaSize; //max possible size
  void* newFile = malloc(newSize); //mod buff

  newSize = delta_decode(srcFile, dltaFile, newFile);
  if(!newSize) goto cleanup_exit;

  if(argc > 3) { //save to file
    umask(S_IWOTH);
    fd = open(argv[3], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP); //perms 660
    if(fd == -1) return 2;
    rc = 0;
    while(rc < newSize)
      rc += write(fd, newFile + rc, newSize - rc);
    close(fd);
  }
  else { //print to stdout
    write(STDOUT_FILENO, newFile, newSize);
  }

  cleanup_exit:
  free(newFile);
  free(dltaFile);
  free(srcFile);
  return !newSize;
}
