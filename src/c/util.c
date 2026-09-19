#include "app.h"

void fmt_duration(char *buf, size_t n, int secs) {
  if (secs < 0) secs = 0;
  if (secs >= 3600) {
    snprintf(buf, n, "%d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
  } else {
    snprintf(buf, n, "%d:%02d", secs / 60, secs % 60);
  }
}

void fmt_minutes(char *buf, size_t n, int mins) {
  if (mins < 0) mins = 0;
  if (mins >= 60) {
    snprintf(buf, n, "%d h %d min", mins / 60, mins % 60);
  } else {
    snprintf(buf, n, "%d min", mins);
  }
}

void fmt_date(char *buf, size_t n, time_t t) {
  struct tm *tm = localtime(&t);
  int a = tm->tm_mday, b = tm->tm_mon + 1;
  const char *sep = ".";
  if (tr(S_DATE_MONTH_FIRST)[0] == '1') {
    a = tm->tm_mon + 1;
    b = tm->tm_mday;
    sep = "/";
  }
  snprintf(buf, n, "%s %02d%s%02d %02d:%02d", tr(S_WD0 + tm->tm_wday), a, sep, b, tm->tm_hour,
           tm->tm_min);
}

// Laufende Nummer des lokalen Kalendertags
int32_t local_day_index(time_t t) {
  struct tm *tm = localtime(&t);
  return (int32_t)((t - tm->tm_hour * 3600 - tm->tm_min * 60 - tm->tm_sec) / SECONDS_PER_DAY);
}

void fmt_kcal(char *buf, size_t n, int kcal_x10) {
  snprintf(buf, n, "%d%s%d kcal", kcal_x10 / 10, tr(S_DECIMAL), kcal_x10 % 10);
}
