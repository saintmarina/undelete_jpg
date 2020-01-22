#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <stdbool.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>

#define ONE_GB (1024*1024*1024)
#define ONE_MB (1024*1024)
#define ONE_KB 1024

#define USEC_PER_SEC 1000000L
#define PBAR_REFRESH_USEC (1*USEC_PER_SEC/2)
#define NO_SPEED -1

#define PBAR_LEN 30

#define NO_UNIT -1
#define GB_UNIT ONE_GB
#define MB_UNIT ONE_MB
#define KB_UNIT ONE_KB

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX_JPG (50*ONE_MB)

typedef int64_t date_usec_t;
typedef int64_t span_usec_t;

struct human_time_span {
  int days;
  int hours;
  int minutes;
  int seconds;
};

typedef int64_t date_usec_t;
typedef int64_t span_usec_t;

extern void maybe_update_status_bar(date_usec_t start_time,
                             size_t b_done, size_t b_total,
                             bool force);

extern date_usec_t tv_to_usec(struct timeval *t);
#endif
