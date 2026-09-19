#include "app.h"

const char *mode_name(uint8_t mode) {
  switch (mode) {
    case MODE_PARTNER: return tr(S_STYLE_PARTNER);
    case MODE_SOLO_RUB: return tr(S_STYLE_RUB);
    case MODE_SOLO_TOY: return tr(S_STYLE_TOY);
    default: return tr(S_STYLE_STROKE);
  }
}

// --- Scrollbare Textansicht (Detail & Statistik) -------------------------

typedef struct {
  Window *window;
  ScrollLayer *scroll;
  TextLayer *text;
  char buf[900];
  int idx;             // Detail: Eintrag, Statistik: -1
  bool confirm_delete;
} TextView;

static TextView s_view;

#define APPEND(...) len += snprintf(buf + len, len < (int)n ? n - len : 0, __VA_ARGS__)

static void build_detail(char *buf, size_t n, int idx) {
  Session *s = storage_get(idx);
  if (!s) {
    snprintf(buf, n, "%s", tr(S_NOT_FOUND));
    return;
  }
  bool changed = sleep_evaluate(s);
  changed |= morning_evaluate(s);
  if (changed) storage_save(idx);

  char date[32], dur[12], act[12], tmp[20], kcal[20];
  fmt_date(date, sizeof(date), s->start);
  fmt_duration(dur, sizeof(dur), s->duration_s);
  fmt_duration(act, sizeof(act), s->active_s);
  fmt_kcal(kcal, sizeof(kcal), s->kcal_x10);

  int len = 0;
  APPEND("%s\n%s%s%s", date, s->mode == MODE_PARTNER ? "" : tr(S_HDR_SOLO),
         s->mode == MODE_PARTNER ? "" : " · ", mode_name(s->mode));
  if (s->partner && partner_name(s->partner)[0]) APPEND(" · %s", partner_name(s->partner));
  APPEND("\n\n");
  APPEND(tr(S_D_DURATION_FMT), dur);
  APPEND("\n");
  APPEND(tr(S_D_ACTIVE_FMT), act);
  APPEND("\n");
  if (s->mode != MODE_SOLO_TOY) {
    APPEND("%s %u\n", tr(s->mode == MODE_SOLO ? S_LBL_STROKES : S_LBL_MOVES), s->strokes);
    APPEND(tr(S_D_RHYTHM_FMT), s->spm_avg, s->spm_max);
    APPEND("\n");
  }
  if (s->climax_s) {
    fmt_duration(tmp, sizeof(tmp), s->climax_s);
    APPEND(tr(S_D_CLIMAX_FMT), tmp);
    APPEND("\n");
  }
  if (s->hr_avg) {
    APPEND(tr(S_D_HR_FMT), s->hr_avg, s->hr_min, s->hr_max);
  } else {
    APPEND("%s", tr(S_D_HR_NONE));
  }
  APPEND("\n");
  APPEND(tr(S_D_ENERGY_FMT), kcal);

  // Stellungen (Partner-Modus)
  int pos_total = 0;
  for (int i = 0; i < POS_COUNT; i++) pos_total += s->pos_pct[i];
  if (pos_total) {
    APPEND("\n\n%s", tr(S_D_POS_HDR));
    for (int i = 0; i < POS_COUNT; i++) {
      if (s->pos_pct[i]) APPEND("\n%s %d%%", pos_name(i), s->pos_pct[i]);
    }
    if (pos_total < 100) APPEND("\n%s %d%%", tr(S_POS_UNKNOWN), 100 - pos_total);
  }

  // Start in den Tag (nur Morgen-Sessions)
  if (s->day_state != DAY_NOT_MORNING) {
    APPEND("\n\n%s\n", tr(S_D_MORNING_HDR));
    if (s->day_state == DAY_PENDING) {
      APPEND("%s", tr(S_DAY_PENDING));
    } else if (s->day_state == DAY_UNAVAILABLE) {
      APPEND("%s", tr(S_DAY_UNAVAILABLE));
    } else {
      APPEND(tr(S_DAY_DONE_FMT), s->day_steps, SIGN(s->day_steps_pct), s->day_steps_pct,
             s->day_active_min);
      if (s->day_hr_delta) {
        APPEND("\n");
        APPEND(tr(S_DAY_HR_FMT), SIGN(s->day_hr_delta), s->day_hr_delta);
      }
    }
    APPEND("\n");
    if (s->mood >= 1 && s->mood <= 5) {
      APPEND(tr(S_MOOD_FMT), s->mood, tr(S_MOOD_1 + s->mood - 1));
    } else {
      APPEND("%s", tr(S_MOOD_NONE));
    }
  }

  // Schlaf danach. Bei Morgen-Sessions nur, wenn tatsächlich geschlafen wurde
  if (s->day_state != DAY_NOT_MORNING && s->sleep_state != SLEEP_DONE) return;
  APPEND("\n\n%s\n", tr(S_D_SLEEP_HDR));
  char a[20], b[20];
  switch (s->sleep_state) {
    case SLEEP_PENDING: APPEND("%s", tr(S_SLEEP_PENDING)); break;
    case SLEEP_NONE: APPEND("%s", tr(S_SLEEP_NONE)); break;
    case SLEEP_UNAVAILABLE: APPEND("%s", tr(S_SLEEP_UNAVAILABLE)); break;
    case SLEEP_DONE: {
      fmt_minutes(a, sizeof(a), s->sleep_total_min);
      fmt_minutes(b, sizeof(b), s->sleep_deep_min);
      int deep_pct = s->sleep_total_min ? s->sleep_deep_min * 100 / s->sleep_total_min : 0;
      APPEND(tr(S_SLEEP_DONE_FMT), s->sleep_latency_min, a, SIGN(s->sleep_delta_min),
             s->sleep_delta_min, b, deep_pct);
      break;
    }
  }
}

static void build_stats(char *buf, size_t n) {
  int count = storage_count();
  if (count == 0) {
    snprintf(buf, n, "%s", tr(S_STATS_EMPTY));
    return;
  }

  time_t week_ago = time(NULL) - 7 * SECONDS_PER_DAY;
  int week = 0, solo = 0;
  uint32_t dur = 0, strokes = 0, kcal = 0, hr_max = 0, hr_n = 0, climax = 0, climax_n = 0;
  int sleep_n = 0, lat = 0, delta = 0, deep_pct = 0;
  int day_n = 0, steps_pct = 0, hr_delta = 0, hr_delta_n = 0, mood = 0, mood_n = 0;
  uint32_t pos_secs[POS_COUNT] = {0};
  int partner_n[PARTNER_COUNT + 1] = {0};

  for (int i = 0; i < count; i++) {
    Session *s = storage_get(i);
    if ((time_t)s->start >= week_ago) week++;
    if (s->mode != MODE_PARTNER) solo++;
    dur += s->duration_s;
    strokes += s->strokes;
    kcal += s->kcal_x10;
    if (s->hr_max) {
      hr_max += s->hr_max;
      hr_n++;
    }
    if (s->climax_s) {
      climax += s->climax_s;
      climax_n++;
    }
    if (s->sleep_state == SLEEP_DONE) {
      sleep_n++;
      lat += s->sleep_latency_min;
      delta += s->sleep_delta_min;
      deep_pct += s->sleep_total_min ? s->sleep_deep_min * 100 / s->sleep_total_min : 0;
    }
    if (s->day_state == DAY_DONE) {
      day_n++;
      steps_pct += s->day_steps_pct;
      if (s->day_hr_delta) {
        hr_delta += s->day_hr_delta;
        hr_delta_n++;
      }
    }
    if (s->mood) {
      mood += s->mood;
      mood_n++;
    }
    for (int p = 0; p < POS_COUNT; p++) pos_secs[p] += (uint32_t)s->pos_pct[p] * s->duration_s;
    if (s->partner <= PARTNER_COUNT) partner_n[s->partner]++;
  }

  char avg_dur[12], total_kcal[20], avg_climax[12];
  fmt_duration(avg_dur, sizeof(avg_dur), dur / count);
  fmt_kcal(total_kcal, sizeof(total_kcal), kcal);

  int len = 0;
  APPEND(tr(S_STATS_ENTRIES_FMT), count, solo, week);
  APPEND(tr(S_STATS_AVG_FMT), avg_dur, (unsigned long)(strokes / count),
         (unsigned long)(kcal / count / 10), (unsigned long)(hr_n ? hr_max / hr_n : 0));
  if (climax_n) {
    fmt_duration(avg_climax, sizeof(avg_climax), climax / climax_n);
    APPEND(tr(S_STATS_CLIMAX_FMT), avg_climax);
  }
  int fav = -1;
  for (int p = 0; p < POS_COUNT; p++) {
    if (pos_secs[p] && (fav < 0 || pos_secs[p] > pos_secs[fav])) fav = p;
  }
  if (fav >= 0) {
    APPEND(tr(S_STATS_POS_FMT), pos_name(fav));
    APPEND("\n");
  }
  // Sessions je Partner (nur bestehende Kürzel)
  bool any_partner = false;
  for (int p = 1; p <= PARTNER_COUNT; p++) {
    if (!partner_n[p] || !partner_name(p)[0]) continue;
    if (!any_partner) APPEND("%s", tr(S_STATS_PARTNER_HDR));
    APPEND("%s", any_partner ? ", " : "");
    APPEND(tr(S_STATS_PARTNER_FMT), partner_name(p), partner_n[p]);
    any_partner = true;
  }
  if (any_partner) APPEND("\n");
  APPEND(tr(S_STATS_TOTAL_FMT), total_kcal);
  if (sleep_n) {
    APPEND(tr(S_STATS_SLEEP_FMT), sleep_n, lat / sleep_n, SIGN(delta), delta / sleep_n,
           deep_pct / sleep_n);
  } else {
    APPEND("%s", tr(S_STATS_SLEEP_NONE));
  }

  APPEND("%s", tr(S_STATS_MORNING_HDR));
  if (day_n) {
    int hr_avg = hr_delta_n ? hr_delta / hr_delta_n : 0;
    APPEND(tr(S_STATS_MORNING_FMT), day_n, SIGN(steps_pct), steps_pct / day_n, SIGN(hr_avg), hr_avg);
  } else {
    APPEND("%s", tr(S_STATS_MORNING_NONE));
  }
  if (mood_n) {
    int m10 = mood * 10 / mood_n;
    APPEND(tr(S_STATS_MOOD_FMT), m10 / 10, tr(S_DECIMAL), m10 % 10);
  }
}

static void view_select_long(ClickRecognizerRef rec, void *ctx) {
  if (s_view.idx < 0) return;
  if (!s_view.confirm_delete) {
    s_view.confirm_delete = true;
    vibes_short_pulse();
    text_layer_set_text(s_view.text, tr(S_DELETE_CONFIRM));
    scroll_layer_set_content_offset(s_view.scroll, GPointZero, false);
    return;
  }
  storage_delete(s_view.idx);
  vibes_double_pulse();
  window_stack_pop(true);
}

static void view_click_config(void *ctx) {
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, view_select_long, NULL);
}

static void view_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int pad = 6;

  if (s_view.idx >= 0) {
    build_detail(s_view.buf, sizeof(s_view.buf), s_view.idx);
  } else {
    build_stats(s_view.buf, sizeof(s_view.buf));
  }

  s_view.scroll = scroll_layer_create(b);
  scroll_layer_set_click_config_onto_window(s_view.scroll, window);
  scroll_layer_set_callbacks(s_view.scroll, (ScrollLayerCallbacks){
                                                .click_config_provider = view_click_config,
                                            });

  GRect text_frame = GRect(pad, 0, b.size.w - 2 * pad, 2000);
  s_view.text = text_layer_create(text_frame);
  text_layer_set_font(s_view.text, fonts_get_system_font(b.size.h >= 200 ? FONT_KEY_GOTHIC_24_BOLD
                                                                         : FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_view.text, s_view.buf);
  GSize size = text_layer_get_content_size(s_view.text);
  text_layer_set_size(s_view.text, GSize(text_frame.size.w, size.h + 10));
  scroll_layer_set_content_size(s_view.scroll, GSize(b.size.w, size.h + 20));
  scroll_layer_add_child(s_view.scroll, text_layer_get_layer(s_view.text));
  layer_add_child(root, scroll_layer_get_layer(s_view.scroll));
}

static void view_unload(Window *window) {
  text_layer_destroy(s_view.text);
  scroll_layer_destroy(s_view.scroll);
  window_destroy(s_view.window);
  s_view.window = NULL;
}

static void view_push(int idx) {
  s_view.idx = idx;
  s_view.confirm_delete = false;
  s_view.window = window_create();
  window_set_window_handlers(s_view.window, (WindowHandlers){
                                                .load = view_load,
                                                .unload = view_unload,
                                            });
  window_stack_push(s_view.window, true);
}

void detail_window_push(int idx) { view_push(idx); }

void stats_window_push(void) { view_push(-1); }

// --- Verlaufsliste -------------------------------------------------------

static Window *s_list_window;
static MenuLayer *s_list_menu;

static uint16_t list_rows(MenuLayer *m, uint16_t section, void *ctx) {
  int c = storage_count();
  return c ? c : 1;
}

static void list_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  Session *s = storage_get(index->row);
  if (!s) {
    menu_cell_basic_draw(g, cell, tr(S_LIST_EMPTY), tr(S_LIST_EMPTY_SUB), NULL);
    return;
  }
  char title[32], dur[12], sub[48];
  fmt_date(title, sizeof(title), s->start);
  fmt_duration(dur, sizeof(dur), s->duration_s);
  if (s->mode == MODE_SOLO_TOY) {
    snprintf(sub, sizeof(sub), "%s · %s · %d kcal", mode_name(s->mode), dur, s->kcal_x10 / 10);
  } else if (s->partner && partner_name(s->partner)[0]) {
    snprintf(sub, sizeof(sub), "%s · %s · %s", partner_name(s->partner), dur, mode_name(s->mode));
  } else {
    snprintf(sub, sizeof(sub), "%s · %s · %u", mode_name(s->mode), dur, s->strokes);
  }
  menu_cell_basic_draw(g, cell, title, sub, NULL);
}

static void list_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  if (storage_count()) detail_window_push(index->row);
}

static void list_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_list_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_list_menu, NULL, (MenuLayerCallbacks){
                                                  .get_num_rows = list_rows,
                                                  .draw_row = list_draw,
                                                  .select_click = list_select,
                                              });
#if defined(PBL_COLOR)
  menu_layer_set_highlight_colors(s_list_menu, GColorFolly, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_list_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_list_menu));
}

static void list_appear(Window *window) { menu_layer_reload_data(s_list_menu); }

static void list_unload(Window *window) {
  menu_layer_destroy(s_list_menu);
  window_destroy(s_list_window);
  s_list_window = NULL;
}

void history_window_push(void) {
  s_list_window = window_create();
  window_set_window_handlers(s_list_window, (WindowHandlers){
                                                .load = list_load,
                                                .appear = list_appear,
                                                .unload = list_unload,
                                            });
  window_stack_push(s_list_window, true);
}
