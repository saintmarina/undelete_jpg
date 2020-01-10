#ifndef _UNDELETE_JPG_H_
#define _UNDELETE_JPG_H_

#include <stddef.h>
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

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long int u64;

/* opens and fills the addr and size */
extern int undelete_jpg(u8 *offset, size_t size);
extern ssize_t get_file_size(int fd);
extern int undelete_jpg_read(int fd,  size_t size);
extern int undelete_jpg_mmap(u8 *offset, size_t size);
#endif