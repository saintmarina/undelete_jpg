#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>

#ifdef __MACH__
#include <sys/ioctl.h>
#include <sys/disk.h>
#endif

#include "status_bar.h"
#include "undelete_jpg.h"

#ifdef DEBUG
static const char *marker_name(int type)
{
  switch (type) {
    case MARKER_SOI:  return "MARKER_SOI ";
    case MARKER_APPn: return "MARKER_APPn";
    case MARKER_DQT:  return "MARKER_DQT ";
    case MARKER_SOF0: return "MARKER_SOF0";
    case MARKER_DHT:  return "MARKER_DHT ";
    case MARKER_SOF2: return "MARKER_SOF2";
    case MARKER_DRI:  return "MARKER_DRI ";
    case MARKER_RSTn: return "MARKER_RSTn";
    case MARKER_COM:  return "MARKER_COM ";
    case MARKER_SOS:  return "MARKER_SOS ";
    case MARKER_EOI:  return "MARKER_EOI ";
    default: return "No marker";
  }
}
#endif

static int set_marker(struct marker *m, int type, int payload, size_t size)
{
  m->type = type;
  m->len = payload + 2;
  return m->len <= size;
}

static int _scan_marker(struct marker *m, u8 *p, size_t size)
{
  u16 header;
  size_t var_len;

  if (size < 2) 
    return 0;

  header = (p[0] << 8) | p[1];

  switch (header) {
    case 0xffd8: return set_marker(m, MARKER_SOI, 0, size); 
    case 0xffda: return set_marker(m, MARKER_SOS, 0, size);
    case 0xffd9: return set_marker(m, MARKER_EOI, 0, size);
  }

  if (size < 4) 
    return 0;

  var_len = ((p[2] << 8) | p[3]);

  switch (header) {
    case 0xfffe: return set_marker(m, MARKER_COM,  var_len, size);
    case 0xffc0: return set_marker(m, MARKER_SOF0, var_len, size);
    case 0xffc4: return set_marker(m, MARKER_DHT,  var_len, size);
    case 0xffc2: return set_marker(m, MARKER_SOF2, var_len, size);
    case 0xffdb: return set_marker(m, MARKER_DQT,  var_len, size);
    case 0xffdd: return set_marker(m, MARKER_DRI,        4, size);
  }

  if ((header & 0xfff0) == 0xffe0)
    return set_marker(m, MARKER_APPn, var_len, size);

  if ((header & 0xfff8) == 0xffd0)
    return set_marker(m, MARKER_RSTn, var_len, size);

  return 0;
}

static int scan_marker(struct marker *m, u8 *p, size_t size)
{
  static u8 *start=0;
  if (start == 0)
    start = p;

  int ret = _scan_marker(m, p, size);

#ifdef DEBUG
  if (ret)
    printf("Type:%s offset: %08lx Len: %zu\n", marker_name(m->type), p-start, m->len);
#endif

  return ret;
}

static u8 *get_end_of_jpg(u8 *offset, size_t size)
{
  struct marker marker;
  u8 * start = offset;

  /* Step 1: Make sure the SOI marker is present */
  if (!scan_marker(&marker, offset, size))
    return NULL;

  if (marker.type != MARKER_SOI)
    return NULL;

  offset += marker.len;
  size -= marker.len;

  /* Step 2: Go through all markers until MARKER_SOS */
  while (size > 0) {
    if (!scan_marker(&marker, offset, size))
      return NULL;

    offset += marker.len;
    size -= marker.len;

    if (marker.type == MARKER_SOS)
      break;
  }

  /*
   * After start of scan, the raw data of the image
   * is present. We don't know where it ends. There's
   * no length indicator.
   */

  /* Step 3: Search for MARKER_EOI */
  while (size > 0) {
    if (scan_marker(&marker, offset, size) &&
        marker.type == MARKER_EOI) {
      return offset + marker.len;
    }

    /*
     * Note: We do byte by byte because the EOI marker
     * can be anywhere
     */
    offset+=1;
    size-=1;

    /* if we looked through 50MB of data, give up */
    if ((offset - start) >= MAX_JPG)
      break;
  }

  return NULL;
}

static int create_empty_jpg() {
  static unsigned int img_num = 0;
  char file_name[64];
  int fd;

  sprintf(file_name, "%03u.jpg", img_num);
  fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
      warn("Error opening output file");
      return -1;
  }
  img_num++;
  return fd;
}

static int recover_jpg(u8 *start, u8 *end) {
    int ret;
    int dst_fd;
    size_t num_bytes;

    dst_fd = create_empty_jpg();
    if (dst_fd == -1)
        return -1;

    num_bytes = end - start;

    ret = write(dst_fd, start, num_bytes);
    close(dst_fd);

    if ((size_t)ret != num_bytes) {
      warn("Error while writing into output file");
      return -1;
    }

    return 0;
}

static int maybe_recover_jpg(u8 *offset, size_t total_size)
{
  u8 *end_of_jpg = get_end_of_jpg(offset, total_size);
  if (end_of_jpg == NULL)
    return 0;
  if (recover_jpg(offset, end_of_jpg) == -1)
    return -1;

  return 1;
}


static u8 *locate_jpg_header(u8 *buffer, size_t size)
{
  u8 *ret = memchr(buffer+1, 0xd8, size-1);
  if (ret == NULL)
    return ret;

  return ret-1;
}


static int undelete_jpg_buf(u8 *offset, size_t size_scan,
                     size_t size_buf, bool show_status_bar)
{
  u8 *start = offset;
  size_t size_scan_orig = size_scan;

  struct timeval t;
  date_usec_t start_time;

  if (show_status_bar) {
    gettimeofday(&t, NULL);
    start_time = tv_to_usec(&t);
  }

  int img_count = 0;

  while (size_scan > 1) {
    /*
     * Status bar updates twice a second
     * Alter refrest rate by chaning PBAR_REFRESH_USEC
     */
    if (show_status_bar)
      maybe_update_status_bar(start_time, offset-start, size_scan_orig, false);
    /* Speed optimizations:
     * if reading byte by byte: on MacOS 150 MB/s, on linux 850MB/s
     * if reading by every 512 bytes (standard FAT partition block size):
     * on Mac OS 1.6 GB/s, on linux 1.6 GB/s -- Note: not seeing all images
     * if reading by 10kb with memchr: on MacOS 1.7 GB/s, on linux 2.0 GB/s
     * When reading with pages already present in memory, we get up to 10GB/s.
     * Every jpg starts with 0xffd8 (Start Of Image "SOI" marker)
     * We use memchr function for speed optimization.
     * We are looking for 0xd8 in every 10kb
     * If memchr finds 0xd8, we check if it's an image.
     */

    size_t n = MIN(size_scan, ONE_KB*10);
    u8 *ret = locate_jpg_header(offset, n);
    if (ret != NULL) {
      int img = maybe_recover_jpg(ret, size_buf - (ret - offset));
      if (img == -1)
        return -1;
      img_count += img;
      n = (ret - offset) + 1;
    }
    size_scan -= n;
    size_buf -= n;
    offset += n;
  }
  /* Update status bar before the program reached the end of the file */
  if (show_status_bar)
    maybe_update_status_bar(start_time, offset-start, size_scan_orig, true);

#ifdef DEBUG
  printf("Reached the end of the file\n");
#endif

  return img_count;
}

int undelete_jpg_mmap(u8 *offset, size_t size)
{
  return undelete_jpg_buf(offset, size, size, true);
}

ssize_t bread(int fd, u8 *buf, ssize_t size)
{
  if (size == 0)
    return 0;

  ssize_t count = 0;

  while (count < size) {
    ssize_t ret = read(fd, buf + count, size - count);
    if (ret <= 0) {
      warn("Error reading file");
      return -1;
    }
    count += ret;
  }
  return count;
}

int undelete_jpg_read(int fd,  size_t size)
{
  u8 *buf = malloc(2*MAX_JPG);
  if (buf == NULL) {
    warn("Can't allocate memory");
    return -1;
  }
  // Both buf1 and buf2 are MAX_JPG len.
  u8 *buf1 = buf;
  u8 *buf2 = buf+MAX_JPG;
  ssize_t buf1_size, buf2_size;

  int img_count = 0;
  size_t b_left = size;

  struct timeval t;
  date_usec_t start_time;

  gettimeofday(&t, NULL);
  start_time = tv_to_usec(&t);

  buf2_size = 0;

  do {
    maybe_update_status_bar(start_time, size-b_left, size, false);

    //copy last 50MB of buf into the beginning of the buf
    memcpy(buf1, buf2, buf2_size);
    buf1_size = buf2_size;

    //read fresh 50MB into second part of buf
    buf2_size = bread(fd, buf2, MIN(MAX_JPG, b_left));
    if (buf2_size < 0) {
      free(buf);
      return -1;
    }
    b_left -= buf2_size;
    img_count += undelete_jpg_buf(buf1, buf1_size, buf1_size+buf2_size, false);
  } while (buf2_size > 0);

  /* Update status bar before the program reached the end of the file */
  
  maybe_update_status_bar(start_time, size-b_left, size, true);

  free(buf);
  return img_count;
}

ssize_t get_file_size(int fd)
{
#ifdef __MACH__
  struct stat stat;

  if (fstat(fd, &stat) == -1) {
    warn("fstat failed");
    return -1;
  }

  if ((stat.st_mode & S_IFMT) == S_IFCHR ||
      (stat.st_mode & S_IFMT) == S_IFBLK) {
    uint64_t sector_count = 0;
    uint32_t sector_size = 0;
    
    // Query the number of sectors on the disk
    if (ioctl(fd, DKIOCGETBLOCKCOUNT, &sector_count) == -1) {
      warn("Ioctl failed when getting sector_count");
      return -1;
    }

    // Query the size of each sector
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &sector_size) == -1) {
      warn("Ioctl failed when getting sector_size");
      return -1;
    }

    return sector_count * sector_size;
  }
#endif

  ssize_t size = lseek(fd, 0, SEEK_END);
  if (size == -1) {
    warn("Lseek failed\n");
    return -1;
  }

  if (lseek(fd, 0, SEEK_SET) == -1) {
    warn("Lseek failed\n");
    return -1;
  }

  if (size == 0) {
    warnx("File empty, or unable to retrieve size of the device");
    return -1;
  }

  return size;
}