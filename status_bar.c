#include "status_bar.h"

date_usec_t tv_to_usec(struct timeval *t)
{
  return (USEC_PER_SEC * (int64_t)t->tv_sec) + t->tv_usec;
}

static int fill_time(char *buf, struct human_time_span *t)
{
  char *start = buf;
  if (t->days > 0) {
    buf += sprintf(buf, "%dd ", t->days);
    buf += sprintf(buf, "%02dh", t->hours);
  } else if (t->hours > 0) {
    buf += sprintf(buf, "%02dh ", t->hours);
    buf += sprintf(buf, "%02dm", t->minutes);
  } else {
    buf += sprintf(buf, "%02dm ", t->minutes);
    buf += sprintf(buf, "%02ds", t->seconds);
  }
  return buf - start;
}

static void humanize_time(struct human_time_span *t, long sec)
{
  t->days = sec / (24 * 3600);
  sec = sec % (24 * 3600); 

  t->hours = sec / 3600;
  sec %= 3600;

  t->minutes = sec / 60;
  sec %= 60; 

  t->seconds = sec; 
}

static int fill_eta(char *buf, int64_t b_left, int b_per_second)
{
  struct human_time_span t;
  char *start = buf;

  buf += sprintf(buf, "eta ");

  if (b_per_second == 0 || b_per_second == NO_SPEED)
    return sprintf(buf, "??");
  
  humanize_time(&t, b_left/b_per_second);
  buf += fill_time(buf, &t);
  return buf - start;
}

static int fill_human_bytes(char *buf, size_t bytes, int unit)
{
  return sprintf(buf, "%.1f", (float)bytes/unit);
}

static int fill_human_byte_unit(char *buf, int unit)
{
  const char *s;
  switch(unit) {
    case GB_UNIT: s = "GB"; break;
    case MB_UNIT: s = "MB"; break;
    case KB_UNIT: s = "KB"; break;
    default: s = "";
  }
  return sprintf(buf, "%s", s);
}

static int get_best_human_unit(size_t bytes)
{   
  if (bytes < ONE_MB)
    return KB_UNIT;
  if (bytes < ONE_GB)
    return MB_UNIT;

  return GB_UNIT;
}

static int fill_bytes_per_sec(char *buf, size_t b_per_second)
{
  if ((ssize_t)b_per_second == NO_SPEED)
    return sprintf(buf, "\?\?/s");

  char *start = buf;

  int unit = get_best_human_unit(b_per_second);
  buf += fill_human_bytes(buf, b_per_second, unit);
  buf += fill_human_byte_unit(buf, unit);
  buf += sprintf(buf, "/s");
  return buf - start;
}

static int fill_bytes_processed(char *buf, size_t b_done, size_t b_total)
{
  char *start = buf;
  int unit = get_best_human_unit(b_total);

  buf += fill_human_bytes(buf, b_done, unit);
  buf += sprintf(buf, "/");
  buf += fill_human_bytes(buf, b_total, unit);
  buf += fill_human_byte_unit(buf, unit);

  return buf - start;
}

static int fill_progress_bar(char *buf, int percent)
{
  int bar_progress = (percent*(PBAR_LEN-2))/100;

  int i = 0;

  buf[i++] = '[';

  for (; i < bar_progress; i++)
    buf[i] = '=';

  buf[i++] = '>';

  for (; i < PBAR_LEN-1; i++)
    buf[i] = ' ';
  
  buf[i++] = ']';
  buf[i++] = '\0';

  return PBAR_LEN;
}

static int fill_percent(char *status_bar, int percent)
{
  return sprintf(status_bar, "%3d%%", percent);
}

static size_t get_speed(date_usec_t start_time, date_usec_t now_time, int64_t b_done)
{
  span_usec_t time_passed = now_time - start_time;

  if (time_passed == 0) {
    return NO_SPEED; 
  }
  return (b_done * USEC_PER_SEC) / time_passed;
}

static int get_percent(size_t size, size_t bytes_left)
{
  return 100 - ((bytes_left * 100) / size);
}

static void print_status_bar(date_usec_t start_time, date_usec_t now_time, 
                      size_t b_done, size_t b_total)
{
  char status_bar[300];
  char *p = status_bar; 
  
  size_t b_left = b_total - b_done;

  int percent = get_percent(b_total, b_left);
  size_t b_per_second = get_speed(start_time, now_time, b_done);
  
  p += fill_percent(p, percent);
  p += sprintf(p, " ");
  p += fill_progress_bar(p, percent);
  p += sprintf(p, " ");
  p += fill_bytes_processed(p, b_done, b_total);
  p += sprintf(p, " ");
  p += fill_bytes_per_sec(p, b_per_second);
  p += sprintf(p, "  ");
  p += fill_eta(p, b_left, b_per_second);

  printf("\33[2K\r%s", status_bar);
  fflush(stdout);
}


void maybe_update_status_bar(date_usec_t start_time,
                             size_t b_done, size_t b_total,
                             bool force)
{
  static int64_t last_time_printed_status = 0;

  struct timeval tvnow;
  gettimeofday(&tvnow, NULL);
  date_usec_t now_time = tv_to_usec(&tvnow);

  if (last_time_printed_status + PBAR_REFRESH_USEC < now_time || force) {
    print_status_bar(start_time, now_time, b_done, b_total);
    last_time_printed_status = now_time;
  }
}