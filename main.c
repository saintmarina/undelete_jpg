#include <err.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "undelete_jpg.h"
#include "status_bar.h"


int main(int argc, char *argv[])
{
  int fd;
  u8 *fptr;
  ssize_t size;
  int img_count;

  if (argc != 2)
    errx(1, "Usage: ./undelete_jpg /dev/device");

  printf("Recovering from: %s\n", argv[1]);

  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    warn("Error opening %s", argv[1]);
    return 1;
  }

  size = get_file_size(fd);
  if (size == -1)
    return 1;
  
  fptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  if (fptr == MAP_FAILED) {
    img_count = undelete_jpg_read(fd, size);
  } else {
    img_count = undelete_jpg_mmap(fptr, size);
  }

  close(fd);
  
  printf("\n%i images recovered\n", img_count);

  return 0;
}