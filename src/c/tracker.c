#include "app.h"

#define SAMPLE_HZ 25
#define BATCH_SIZE 5
#define SPM_WINDOW 10         // Sekunden für den gleitenden Rhythmus
#define MIN_SAVE_SECONDS 20

// Kalorien: MET-Obergrenzen je Modus. Grundlage: Frappier et al. 2013
// (PLoS ONE): Sex ~6 MET (m) / ~5.6 MET (w). Solo deutlich weniger.
#define MET_REST 1.1f

static Window *s_window;
static TextLayer *s_mode_layer, *s_time_layer, *s_count_label, *s_count_layer;
static TextLayer *s_hr_layer, *s_kcal_layer, *s_spm_layer, *s_hint_layer;
static char s_mode_buf[48], s_time_buf[12], s_count_buf[12];
static char s_hr_buf[16], s_kcal_buf[16], s_spm_buf[48];

static SessionMode s_mode;
static Session s_sess;
static Detector s_det;
static bool s_paused;
static float s_kcal;
static uint32_t s_hr_sum, s_hr_n, s_spm_sum, s_spm_n;
static int s_spm;
static int s_last_hr;
static uint16_t s_orient_count[ORIENT_COUNT];
static int32_t s_grav_sum[3];
static uint32_t s_grav_n;
static int s_cur_second;
static uint8_t s_buckets[SPM_WINDOW];
static int s_bucket_idx, s_bucket_filled;
static bool s_finishing;

SessionMode solo_mode(void) { return solo_mode_for(storage_profile()); }

static bool counts_cycles(void) { return s_mode != MODE_SOLO_TOY; }

static void accel_handler(AccelData *data, uint32_t num) {
  if (s_paused) return;
  for (uint32_t i = 0; i < num; i++) {
    if (data[i].did_vibrate) continue;
    if (detector_process(&s_det, data[i].x, data[i].y, data[i].z)) {
      if (s_sess.strokes < UINT16_MAX) s_sess.strokes++;
      s_cur_second++;
    }
    for (int a = 0; a < 3; a++) s_grav_sum[a] += s_det.lp16[a] / 16;
    s_grav_n++;
  }
}

// --- Puls & Kalorien -----------------------------------------------------

static int current_hr(void) {
#if defined(PBL_HEALTH)
  time_t now = time(NULL);
  HealthServiceAccessibilityMask m =
      health_service_metric_accessible(HealthMetricHeartRateBPM, now, now);
  if (m & HealthServiceAccessibilityMaskAvailable) {
    HealthValue v = health_service_peek_current_value(HealthMetricHeartRateBPM);
    if (v > 30 && v < 230) return (int)v;
  }
#endif
  return 0;
}

static float kcal_per_min(int hr, int spm, bool moving) {
  Profile *p = storage_profile();
  float w = p->weight_kg;
  float age = p->age;
  float met_max = (s_mode == MODE_PARTNER) ? (p->sex == SEX_FEMALE ? 5.6f : 6.0f) : 3.5f;
  float met_min = (s_mode == MODE_PARTNER) ? 2.0f : 1.3f;

  // Bewegungsbasiert: Rhythmus skaliert zwischen met_min und ~60 % von met_max
  float met = MET_REST;
  if (moving) {
    int r = spm > 240 ? 240 : spm;
    met = met_min + (met_max * 0.6f - met_min) * r / 240.0f;
  }
  float kcal = met * 3.5f * w / 200.0f;

  // Pulsbasiert (Keytel et al. 2005), gedeckelt, da Erregung den Puls
  // hebt, ohne dass entsprechend Energie verbraucht wird
  if (hr > 0) {
    float male = (-55.0969f + 0.6309f * hr + 0.1988f * w + 0.2017f * age) / 4.184f;
    float female = (-20.4022f + 0.4472f * hr - 0.1263f * w + 0.074f * age) / 4.184f;
    float k = p->sex == SEX_MALE ? male : p->sex == SEX_FEMALE ? female : (male + female) / 2;
    float cap = met_max * 3.5f * w / 200.0f;
    if (k > cap) k = cap;
    if (k > kcal) kcal = k;
  }
  return kcal;
}

// --- UI ------------------------------------------------------------------

static void update_ui(void) {
  const char *hdr = tr(s_mode == MODE_PARTNER ? S_HDR_PARTNER : S_HDR_SOLO);
  if (s_paused) {
    snprintf(s_mode_buf, sizeof(s_mode_buf), "%s · %s", hdr, tr(S_HDR_PAUSE));
  } else if (s_sess.climax_s) {
    char t[12];
    fmt_duration(t, sizeof(t), s_sess.climax_s);
    snprintf(s_mode_buf, sizeof(s_mode_buf), tr(S_CLIMAX_FMT), t);
  } else if (s_mode == MODE_PARTNER) {
    snprintf(s_mode_buf, sizeof(s_mode_buf), "%s", hdr);
  } else {
    snprintf(s_mode_buf, sizeof(s_mode_buf), "%s · %s", hdr, mode_name(s_mode));
  }
  text_layer_set_text(s_mode_layer, s_mode_buf);

  fmt_duration(s_time_buf, sizeof(s_time_buf), s_sess.duration_s);
  text_layer_set_text(s_time_layer, s_time_buf);

  if (counts_cycles()) {
    snprintf(s_count_buf, sizeof(s_count_buf), "%u", s_sess.strokes);
    snprintf(s_spm_buf, sizeof(s_spm_buf), tr(S_RHYTHM_FMT), s_spm);
  } else {
    fmt_duration(s_count_buf, sizeof(s_count_buf), s_sess.active_s);
    s_spm_buf[0] = '\0';
  }
  text_layer_set_text(s_count_layer, s_count_buf);
  text_layer_set_text(s_spm_layer, s_spm_buf);

  if (s_last_hr) {
    snprintf(s_hr_buf, sizeof(s_hr_buf), "%d bpm", s_last_hr);
  } else {
    snprintf(s_hr_buf, sizeof(s_hr_buf), "-- bpm");
  }
  text_layer_set_text(s_hr_layer, s_hr_buf);

  int kx10 = (int)(s_kcal * 10);
  snprintf(s_kcal_buf, sizeof(s_kcal_buf), "%d kcal", kx10 / 10);
  text_layer_set_text(s_kcal_layer, s_kcal_buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (s_paused) return;

  if (s_sess.duration_s < UINT16_MAX) s_sess.duration_s++;

  // Rhythmus über gleitendes Fenster
  s_buckets[s_bucket_idx] = s_cur_second > 255 ? 255 : s_cur_second;
  s_bucket_idx = (s_bucket_idx + 1) % SPM_WINDOW;
  s_cur_second = 0;
  if (s_bucket_filled < SPM_WINDOW) s_bucket_filled++;
  int sum = 0;
  for (int i = 0; i < SPM_WINDOW; i++) sum += s_buckets[i];
  s_spm = sum * 60 / s_bucket_filled;

  bool moving = s_det.moving > SAMPLE_HZ / 3;
  s_det.moving = 0;
  if (moving) {
    s_sess.active_s++;
    s_spm_sum += s_spm;
    s_spm_n++;
    if (s_bucket_filled == SPM_WINDOW && s_spm > s_sess.spm_max) {
      s_sess.spm_max = s_spm > 255 ? 255 : s_spm;
    }
  }

  int hr = current_hr();
  if (hr) {
    s_last_hr = hr;
    s_hr_sum += hr;
    s_hr_n++;
    if (hr > s_sess.hr_max) s_sess.hr_max = hr;
    if (s_sess.hr_min == 0 || hr < s_sess.hr_min) s_sess.hr_min = hr;
  }

  s_kcal += kcal_per_min(hr, s_spm, moving) / 60.0f;

  // Orientierung: dominante Schwerkraftachse
  int g[3] = {s_det.lp16[0] / 16, s_det.lp16[1] / 16, s_det.lp16[2] / 16};
  int ax = 0;
  for (int a = 1; a < 3; a++) {
    int cur = g[a] < 0 ? -g[a] : g[a];
    int best = g[ax] < 0 ? -g[ax] : g[ax];
    if (cur > best) ax = a;
  }
  s_orient_count[ax * 2 + (g[ax] < 0 ? 1 : 0)]++;

  update_ui();
}

// Fensterwechsel erst nach dem Klick-Handler: Würde das Fenster (samt Klick-Erkennung)
// noch im Handler zerstört, greift die Firmware danach auf freigegebenen Speicher zu.
static void close_window(void *saved) {
  Window *w = s_window;
  if (saved) detail_window_push(0);
  window_stack_remove(w, !saved);
}

static void finish(void) {
  if (s_finishing) return;
  s_finishing = true;
  s_paused = true;  // keine weiteren Ticks/Samples zählen

  if (s_sess.duration_s < MIN_SAVE_SECONDS) {
    app_timer_register(10, close_window, NULL);
    return;
  }

  s_sess.kcal_x10 = (uint16_t)(s_kcal * 10);
  s_sess.hr_avg = s_hr_n ? s_hr_sum / s_hr_n : 0;
  s_sess.spm_avg = s_spm_n ? s_spm_sum / s_spm_n : 0;
  for (int a = 0; a < 3; a++) {
    s_sess.grav[a] = s_grav_n ? s_grav_sum[a] / (int32_t)s_grav_n : 0;
  }
  uint32_t total = 0;
  for (int i = 0; i < ORIENT_COUNT; i++) total += s_orient_count[i];
  for (int i = 0; i < ORIENT_COUNT; i++) {
    s_sess.orient_pct[i] = total ? s_orient_count[i] * 100 / total : 0;
  }
  s_sess.sleep_state = SLEEP_PENDING;
  s_sess.day_state = morning_is_morning(s_sess.start) ? DAY_PENDING : DAY_NOT_MORNING;

  storage_add(&s_sess);
  if (s_sess.day_state == DAY_PENDING) morning_schedule_checkin(&s_sess);
  vibes_double_pulse();
  app_timer_register(10, close_window, (void *)1);
}

static void toggle_pause(void) {
  s_paused = !s_paused;
  vibes_short_pulse();
  update_ui();
}

static void mark_climax(void) {
  if (s_paused || s_sess.climax_s) return;
  s_sess.climax_s = s_sess.duration_s ? s_sess.duration_s : 1;
  vibes_short_pulse();
  update_ui();
}

static void select_click(ClickRecognizerRef rec, void *ctx) { toggle_pause(); }

static void select_long_click(ClickRecognizerRef rec, void *ctx) { finish(); }

static void back_click(ClickRecognizerRef rec, void *ctx) { finish(); }

static void down_click(ClickRecognizerRef rec, void *ctx) { mark_climax(); }

#if defined(PBL_TOUCH)
// Bewusst nur Wischgesten, keine Taps: Hautkontakt während der Session soll nichts auslösen
static void swipe_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event != RecognizerEvent_Completed) return;
  switch (swipe_recognizer_get_direction(recognizer)) {
    case SwipeDirection_Left:
    case SwipeDirection_Right: toggle_pause(); break;
    case SwipeDirection_Up: mark_climax(); break;
    default: break;
  }
}
#endif

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static TextLayer *make_text(Layer *root, GRect frame, const char *font, GColor color,
                            GTextAlignment align) {
  TextLayer *t = text_layer_create(frame);
  text_layer_set_background_color(t, GColorClear);
  text_layer_set_text_color(t, color);
  text_layer_set_font(t, fonts_get_system_font(font));
  text_layer_set_text_alignment(t, align);
  layer_add_child(root, text_layer_get_layer(t));
  return t;
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int w = b.size.w, h = b.size.h;
  bool big = h >= 200;  // Pebble Time 2 (emery): 200x228
  int pad = 6;

  window_set_background_color(window, GColorBlack);

  s_mode_layer = make_text(root, GRect(0, big ? 4 : 0, w, 24),
                           big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD, ACCENT_COLOR,
                           GTextAlignmentCenter);
  s_time_layer = make_text(root, GRect(0, big ? 24 : 16, w, 50),
                           big ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_32_BOLD_NUMBERS,
                           GColorWhite, GTextAlignmentCenter);
  s_count_label = make_text(root, GRect(0, big ? 76 : 54, w, 18), FONT_KEY_GOTHIC_14,
                            PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite), GTextAlignmentCenter);
  StrId label = s_mode == MODE_SOLO ? S_LBL_STROKES
                : s_mode == MODE_SOLO_TOY ? S_LBL_ACTIVE : S_LBL_MOVES;
  text_layer_set_text(s_count_label, tr(label));
  s_count_layer = make_text(root, GRect(0, big ? 92 : 68, w, 44),
                            big ? FONT_KEY_LECO_36_BOLD_NUMBERS : FONT_KEY_LECO_28_LIGHT_NUMBERS,
                            ACCENT_COLOR, GTextAlignmentCenter);

  const char *row_font = big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD;
  int row_y = big ? 138 : 102;
  s_hr_layer = make_text(root, GRect(pad, row_y, w / 2 - pad, 30), row_font, GColorWhite,
                         GTextAlignmentLeft);
  s_kcal_layer = make_text(root, GRect(w / 2, row_y, w / 2 - pad, 30), row_font, GColorWhite,
                           GTextAlignmentRight);
  s_spm_layer = make_text(root, GRect(0, row_y + (big ? 30 : 22), w, 24),
                          big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14, GColorWhite,
                          GTextAlignmentCenter);
  s_hint_layer = make_text(root, GRect(0, h - (big ? 36 : 18), w, big ? 36 : 18),
                           FONT_KEY_GOTHIC_14, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite),
                           GTextAlignmentCenter);
  const char *hint = tr(big ? S_HINT_BUTTONS : S_HINT_BUTTONS_SHORT);

#if defined(PBL_TOUCH)
  // Eigene Gesten statt System-Bridge; bei ausgeschaltetem Touch wird jede Berührung ignoriert
  window_set_touch_bridge_disabled(window, true);
  if (storage_profile()->touch_session) {
    window_attach_recognizer(
        window, swipe_recognizer_create(swipe_handler, NULL,
                                        SwipeDirection_Left | SwipeDirection_Right | SwipeDirection_Up));
    hint = tr(S_HINT_TOUCH);
  }
#endif
  text_layer_set_text(s_hint_layer, hint);

  update_ui();
}

static void window_unload(Window *window) {
  autostart_session_active(false);
  accel_data_service_unsubscribe();
  tick_timer_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_set_heart_rate_sample_period(0);
#endif
  text_layer_destroy(s_mode_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_count_label);
  text_layer_destroy(s_count_layer);
  text_layer_destroy(s_hr_layer);
  text_layer_destroy(s_kcal_layer);
  text_layer_destroy(s_spm_layer);
  text_layer_destroy(s_hint_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void tracker_window_push(SessionMode mode) { tracker_window_push_at(mode, time(NULL), 0); }

void tracker_window_push_at(SessionMode mode, time_t start, uint16_t cycles) {
  s_mode = mode;
  s_sess = (Session){0};
  s_sess.start = start;
  s_sess.mode = mode;
  s_paused = false;
  s_finishing = false;
  s_kcal = 0;
  s_hr_sum = s_hr_n = s_spm_sum = s_spm_n = 0;
  s_spm = 0;
  s_last_hr = 0;
  memset(s_orient_count, 0, sizeof(s_orient_count));
  memset(s_grav_sum, 0, sizeof(s_grav_sum));
  s_grav_n = 0;
  s_cur_second = 0;
  memset(s_buckets, 0, sizeof(s_buckets));
  s_bucket_idx = 0;
  s_bucket_filled = 0;

  DetectorParams params;
  detector_params_for(mode, &params);
  detector_init(&s_det, &params);

  // Rückdatiert (automatisch erkannt): bisherige Zeit und Zyklen übernehmen
  int elapsed = time(NULL) - start;
  if (elapsed > 0) {
    s_sess.duration_s = elapsed;
    s_sess.active_s = elapsed;
    s_sess.strokes = cycles;
    int spm = cycles * 60 / elapsed;
    s_kcal = kcal_per_min(0, spm, true) * elapsed / 60.0f;
    if (spm > 0) {
      s_spm_sum = (uint32_t)spm * elapsed;
      s_spm_n = elapsed;
      s_sess.spm_max = spm > 255 ? 255 : spm;
    }
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
  autostart_session_active(true);

#if defined(PBL_HEALTH)
  health_service_set_heart_rate_sample_period(1);
#endif
  accel_data_service_subscribe(BATCH_SIZE, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  vibes_short_pulse();
}
