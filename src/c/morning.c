#include "app.h"

// "Start in den Tag": Nach einer Morgen-Session werden die folgenden 4 Stunden
// mit dem persönlichen Durchschnitt für dasselbe Zeitfenster verglichen
// (Pebble Health unterscheidet dabei Werktage und Wochenende).
#define MORNING_FROM_HOUR 4
#define MORNING_TO_HOUR 12
#define DAY_WINDOW (4 * SECONDS_PER_HOUR)
#define CHECKIN_AFTER (3 * SECONDS_PER_HOUR)
#define CHECKIN_EXPIRES (12 * SECONDS_PER_HOUR)  // danach nicht mehr fragen
#define DAY_HISTORY_LIMIT (7 * SECONDS_PER_DAY)

bool morning_is_morning(time_t t) {
  struct tm *tm = localtime(&t);
  return tm->tm_hour >= MORNING_FROM_HOUR && tm->tm_hour < MORNING_TO_HOUR;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

bool morning_evaluate(Session *s) {
  if (s->day_state != DAY_PENDING) return false;

  time_t now = time(NULL);
  time_t from = s->start + s->duration_s;
  time_t to = from + DAY_WINDOW;
  if (now < to) return false;

#if defined(PBL_HEALTH)
  if (now - to > DAY_HISTORY_LIMIT) {
    s->day_state = DAY_UNAVAILABLE;
    return true;
  }
  HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(HealthMetricStepCount, from, to);
  if (!(mask & HealthServiceAccessibilityMaskAvailable)) {
    s->day_state = DAY_UNAVAILABLE;
    return true;
  }

  HealthValue steps = health_service_sum(HealthMetricStepCount, from, to);
  HealthValue usual = health_service_sum_averaged(HealthMetricStepCount, from, to,
                                                  HealthServiceTimeScopeDailyWeekdayOrWeekend);
  s->day_steps = clamp(steps, 0, UINT16_MAX);
  s->day_steps_pct = usual > 0 ? clamp((steps - usual) * 100 / usual, -100, 127) : 0;
  s->day_active_min = clamp(health_service_sum(HealthMetricActiveSeconds, from, to) / 60, 0, 255);

  s->day_hr_delta = 0;
  if (health_service_metric_aggregate_averaged_accessible(HealthMetricHeartRateBPM, from, to,
                                                          HealthAggregationAvg,
                                                          HealthServiceTimeScopeOnce) &
      HealthServiceAccessibilityMaskAvailable) {
    HealthValue hr = health_service_aggregate_averaged(HealthMetricHeartRateBPM, from, to,
                                                       HealthAggregationAvg,
                                                       HealthServiceTimeScopeOnce);
    HealthValue hr_usual = health_service_aggregate_averaged(
        HealthMetricHeartRateBPM, from, to, HealthAggregationAvg,
        HealthServiceTimeScopeDailyWeekdayOrWeekend);
    if (hr > 0 && hr_usual > 0) s->day_hr_delta = clamp(hr - hr_usual, -127, 127);
  }
  s->day_state = DAY_DONE;
#else
  s->day_state = DAY_UNAVAILABLE;
#endif
  return true;
}

void morning_schedule_checkin(const Session *s) {
  if (!storage_profile()->checkin) return;
  // Cookie = Startzeit, damit die Session beim Aufwachen wiedergefunden wird
  wakeup_schedule(s->start + s->duration_s + CHECKIN_AFTER, (int32_t)s->start, false);
}

// Offene Stimmungsabfrage: jüngste Morgen-Session ohne Bewertung im Zeitfenster
static int due_checkin(void) {
  if (!storage_profile()->checkin) return -1;
  time_t now = time(NULL);
  for (int i = 0; i < storage_count(); i++) {
    Session *s = storage_get(i);
    if (s->day_state == DAY_NOT_MORNING) continue;
    time_t end = s->start + s->duration_s;
    if (s->mood == 0 && now >= end + CHECKIN_AFTER && now < end + CHECKIN_EXPIRES) return i;
    return -1;  // nur die jüngste Morgen-Session zählt
  }
  return -1;
}

// Wakeup, während die App ohnehin offen ist
static void wakeup_handler(WakeupId id, int32_t cookie) {
  int idx = storage_find((uint32_t)cookie);
  if (idx >= 0 && storage_get(idx)->mood == 0) mood_window_push(idx);
}

void morning_init(void) { wakeup_service_subscribe(wakeup_handler); }

bool morning_handle_launch(void) {
  int idx = -1;
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    WakeupId id;
    int32_t cookie;
    if (wakeup_get_launch_event(&id, &cookie)) idx = storage_find((uint32_t)cookie);
  }
  if (idx < 0) idx = due_checkin();
  if (idx < 0) return false;
  mood_window_push(idx);
  return true;
}

// --- Stimmungsabfrage ----------------------------------------------------

static Window *s_window;
static MenuLayer *s_menu;
static TextLayer *s_question;
static uint32_t s_start;  // Session per Startzeit, da sich Indizes verschieben können

static uint16_t mood_rows(MenuLayer *m, uint16_t section, void *ctx) { return 5; }

static void mood_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  // Beste Bewertung oben
  int mood = 5 - index->row;
  char title[8];
  snprintf(title, sizeof(title), "%d", mood);
  menu_cell_basic_draw(g, cell, tr(S_MOOD_1 + mood - 1), title, NULL);
}

static void mood_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  int idx = storage_find(s_start);
  if (idx >= 0) {
    storage_get(idx)->mood = 5 - index->row;
    storage_save(idx);
  }
  vibes_short_pulse();
  window_stack_pop(true);
}

static void mood_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int head = 44;

  s_question = text_layer_create(GRect(4, 0, b.size.w - 8, head));
  text_layer_set_font(s_question, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_question, GTextAlignmentCenter);
  text_layer_set_text(s_question, tr(S_MOOD_Q));
  layer_add_child(root, text_layer_get_layer(s_question));

  s_menu = menu_layer_create(GRect(0, head, b.size.w, b.size.h - head));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
                                             .get_num_rows = mood_rows,
                                             .draw_row = mood_draw,
                                             .select_click = mood_select,
                                         });
#if defined(PBL_COLOR)
  menu_layer_set_highlight_colors(s_menu, GColorFolly, GColorWhite);
#endif
  menu_layer_set_selected_index(s_menu, (MenuIndex){0, 2}, MenuRowAlignCenter, false);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void mood_unload(Window *window) {
  menu_layer_destroy(s_menu);
  text_layer_destroy(s_question);
  window_destroy(s_window);
  s_window = NULL;
}

void mood_window_push(int idx) {
  Session *s = storage_get(idx);
  if (!s || s_window) return;
  s_start = s->start;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = mood_load,
                                           .unload = mood_unload,
                                       });
  window_stack_push(s_window, true);
}
