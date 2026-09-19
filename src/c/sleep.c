#include "app.h"

// Schlaf zählt nur als "danach", wenn er innerhalb dieses Fensters beginnt
#define SLEEP_ONSET_WINDOW (6 * SECONDS_PER_HOUR)
// Lücke zwischen Schlafblöcken, ab der die Nacht als beendet gilt
#define SLEEP_MAX_GAP SECONDS_PER_HOUR
// So lange wach nach dem letzten Schlafblock, bevor die Nacht final ausgewertet wird
#define SLEEP_SETTLE SECONDS_PER_HOUR
// Danach hält die Uhr keine Minutendaten mehr vor
#define SLEEP_HISTORY_LIMIT (7 * SECONDS_PER_DAY)

#if defined(PBL_HEALTH)

typedef struct {
  time_t after;
  time_t first_start;
  time_t night_end;
  int32_t total;
  bool found;
  bool too_late;
} NightCtx;

static bool night_cb(HealthActivity activity, time_t start, time_t end, void *context) {
  NightCtx *c = context;
  if (activity != HealthActivitySleep) return true;
  if (end <= c->after) return true;

  if (!c->found) {
    if (start > c->after + SLEEP_ONSET_WINDOW) {
      c->too_late = true;
      return false;
    }
    c->found = true;
    c->first_start = start < c->after ? c->after : start;
    c->night_end = end;
    c->total = end - c->first_start;
    return true;
  }

  if (start - c->night_end > SLEEP_MAX_GAP) return false;
  c->total += end - start;
  c->night_end = end;
  return true;
}

typedef struct {
  time_t from, to;
  int32_t total;
} DeepCtx;

static bool deep_cb(HealthActivity activity, time_t start, time_t end, void *context) {
  DeepCtx *c = context;
  if (activity != HealthActivityRestfulSleep) return true;
  if (start >= c->to) return false;
  if (start < c->from) start = c->from;
  if (end > c->to) end = c->to;
  if (end > start) c->total += end - start;
  return true;
}

bool sleep_evaluate(Session *s) {
  if (s->sleep_state != SLEEP_PENDING) return false;

  time_t now = time(NULL);
  time_t after = s->start + s->duration_s;

  if (now - after > SLEEP_HISTORY_LIMIT) {
    s->sleep_state = SLEEP_UNAVAILABLE;
    return true;
  }

  HealthServiceAccessibilityMask mask =
      health_service_any_activity_accessible(HealthActivitySleep, after, now);
  if (mask & HealthServiceAccessibilityMaskNoPermission) {
    s->sleep_state = SLEEP_UNAVAILABLE;
    return true;
  }

  NightCtx night = {.after = after};
  health_service_activities_iterate(HealthActivitySleep, after - SECONDS_PER_HOUR,
                                    after + SLEEP_ONSET_WINDOW + 18 * SECONDS_PER_HOUR,
                                    HealthIterationDirectionFuture, night_cb, &night);

  if (!night.found) {
    if (night.too_late || now - after > SLEEP_ONSET_WINDOW) {
      s->sleep_state = SLEEP_NONE;
      return true;
    }
    return false;  // noch warten
  }

  // Nacht erst werten, wenn der/die Nutzer:in eine Weile wach ist
  if (now - night.night_end < SLEEP_SETTLE) return false;

  DeepCtx deep = {.from = night.first_start, .to = night.night_end};
  health_service_activities_iterate(HealthActivityRestfulSleep, night.first_start, night.night_end,
                                    HealthIterationDirectionFuture, deep_cb, &deep);

  s->sleep_latency_min = (night.first_start - after) / 60;
  s->sleep_total_min = night.total / 60;
  s->sleep_deep_min = deep.total / 60;

  time_t day = time_start_of_today();
  HealthValue typical = health_service_sum_averaged(HealthMetricSleepSeconds, day,
                                                    day + SECONDS_PER_DAY,
                                                    HealthServiceTimeScopeDaily);
  s->sleep_delta_min = typical > 0 ? (int16_t)(s->sleep_total_min - typical / 60) : 0;
  s->sleep_state = SLEEP_DONE;
  return true;
}

#else

bool sleep_evaluate(Session *s) {
  if (s->sleep_state != SLEEP_PENDING) return false;
  s->sleep_state = SLEEP_UNAVAILABLE;
  return true;
}

#endif

void sleep_evaluate_all(void) {
  for (int i = 0; i < storage_count(); i++) {
    if (sleep_evaluate(storage_get(i))) storage_save(i);
  }
}
