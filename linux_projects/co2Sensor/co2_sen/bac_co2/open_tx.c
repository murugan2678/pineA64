#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
  // int open(const char *pathname, int flags);
  int fp = open("km.txt", O_CREAT, 0644);
  if (fp == -1)
  {
    perror("error open file descripted");
    exit(-1);
  }
  else
  {
    printf("files are created successfully : %d\n", fp);
  }
}
