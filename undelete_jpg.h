#ifndef UNDELETE_JPG_H
#define UNDELETE_JPG_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


struct marker {
  int type; /* MARKER_SOI */
  size_t len;
};

enum {
  MARKER_SOI,  /*Start of Image*/
  MARKER_APPn,
  MARKER_DQT,
  MARKER_SOF0,
  MARKER_DHT,
  MARKER_SOF2,
  MARKER_DRI,
  MARKER_RSTn,
  MARKER_COM,
  MARKER_SOS, /*Start of Scan*/
  MARKER_EOI  /*End of Image*/
};


/* opens and fills the addr and size */
int undelete_jpg(uint8_t *offset, size_t size);
ssize_t get_file_size(int fd);
int undelete_jpg_read(int fd,  size_t size);
int undelete_jpg_mmap(uint8_t *offset, size_t size);

#endif
