#include "app.h"

// Stellungserkennung für den Partner-Modus.
//
// Die Uhr sieht nur das Handgelenk der tragenden Person. Pro 5-s-Fenster wird
// ein "Fingerabdruck" gebildet:
//   - mittlerer Schwerkraftvektor: wie das Handgelenk im Raum liegt (wichtigstes Merkmal)
//   - Verteilung der Bewegungsenergie auf die drei Achsen: Richtung der Bewegung
//   - Stärke (RMS) und Rhythmus
// Angelernte Stellungen speichern den Mittelwert ihrer Aufnahmen. Erkannt wird
// die nächstgelegene Stellung, sofern sie nah genug ist.

#define SAMPLE_HZ 25
#define POS_WINDOW (5 * SAMPLE_HZ)
#define POS_MATCH_MAX 150   // größerer Abstand: "unbekannt"
#define POS_SIMILAR 60      // Stellungen näher als das sind kaum zu unterscheiden
#define POS_AVG_CAP 5       // neue Aufnahmen zählen mindestens 1/(5+1)
#define NAME_LEN 20

static PosTemplate s_templates[POS_COUNT];
static char s_names[POS_COUNT][NAME_LEN];

void positions_init(void) {
  if (persist_exists(KEY_POS_TEMPLATES)) {
    persist_read_data(KEY_POS_TEMPLATES, s_templates, sizeof(s_templates));
  }
  if (persist_exists(KEY_POS_NAMES)) persist_read_data(KEY_POS_NAMES, s_names, sizeof(s_names));
}

static void save_templates(void) {
  persist_write_data(KEY_POS_TEMPLATES, s_templates, sizeof(s_templates));
}

const char *pos_name(int i) {
  if (i < 0 || i >= POS_COUNT) return tr(S_POS_UNKNOWN);
  return s_names[i][0] ? s_names[i] : tr(S_POS_1 + i);
}

const char *pos_custom_name(int i) { return s_names[i]; }

void pos_set_name(int i, const char *name) {
  // Auf ganze UTF-8-Zeichen kürzen
  size_t n = strlen(name);
  if (n > NAME_LEN - 1) {
    n = NAME_LEN - 1;
    while (n > 0 && ((uint8_t)name[n] & 0xC0) == 0x80) n--;
  }
  memcpy(s_names[i], name, n);
  s_names[i][n] = '\0';
  persist_write_data(KEY_POS_NAMES, s_names, sizeof(s_names));
}

int pos_trained_count(void) {
  int n = 0;
  for (int i = 0; i < POS_COUNT; i++) n += s_templates[i].count > 0;
  return n;
}

// --- Merkmale ------------------------------------------------------------

void pos_acc_reset(PosAcc *a) { memset(a, 0, sizeof(*a)); }

void pos_acc_add(PosAcc *a, const Detector *d, int16_t x, int16_t y, int16_t z, bool cycle) {
  int32_t v[3] = {x, y, z};
  for (int i = 0; i < 3; i++) {
    int32_t g = d->lp16[i] / 16;
    int32_t hp = v[i] - g;
    if (hp > 2000) hp = 2000;
    if (hp < -2000) hp = -2000;
    a->grav[i] += g;
    a->sq[i] += hp * hp;
  }
  a->n++;
  a->cycles += cycle;
}

bool pos_acc_ready(const PosAcc *a) { return a->n >= POS_WINDOW; }

// Liefert false, wenn sich im Fenster zu wenig bewegt hat
bool pos_acc_features(PosAcc *a, uint32_t moving, int16_t *f) {
  bool ok = a->n > 0 && moving * 2 >= (uint32_t)a->n;
  if (ok) {
    int32_t total = a->sq[0] + a->sq[1] + a->sq[2];
    int32_t max_sq = 0;
    for (int i = 0; i < 3; i++) {
      f[F_GX + i] = a->grav[i] / a->n;
      f[F_SX + i] = total > 0 ? (int16_t)((int64_t)a->sq[i] * 100 / total) : 0;
      if (a->sq[i] > max_sq) max_sq = a->sq[i];
    }
    f[F_RMS] = isqrt32(max_sq / a->n);
    f[F_SPM] = a->cycles * 60 * SAMPLE_HZ / a->n;
  }
  pos_acc_reset(a);
  return ok;
}

static int32_t absdiff(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

static int32_t distance(const int16_t *a, const int16_t *b) {
  int32_t dg = 0, ds = 0;
  for (int i = 0; i < 3; i++) {
    dg += absdiff(a[F_GX + i], b[F_GX + i]);
    ds += absdiff(a[F_SX + i], b[F_SX + i]);
  }
  int32_t dr = absdiff(a[F_RMS], b[F_RMS]) / 4;
  int32_t dspm = absdiff(a[F_SPM], b[F_SPM]) / 4;
  // Orientierung zählt am meisten: 90° Drehung des Handgelenks ~ 2000 mg -> 250
  return dg / 8 + ds / 2 + (dr > 50 ? 50 : dr) + (dspm > 50 ? 50 : dspm);
}

static int nearest(const int16_t *f, int skip, int32_t *dist_out) {
  int best = -1;
  int32_t best_d = INT32_MAX;
  for (int i = 0; i < POS_COUNT; i++) {
    if (i == skip || !s_templates[i].count) continue;
    int32_t d = distance(f, s_templates[i].f);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  if (dist_out) *dist_out = best_d;
  return best;
}

int pos_classify(const int16_t *f) {
  int32_t d;
  int best = nearest(f, -1, &d);
  return (best >= 0 && d <= POS_MATCH_MAX) ? best : -1;
}

// Speichert eine Aufnahme; liefert die Stellung, die kaum zu unterscheiden ist, oder -1
static int train(int i, const int16_t *f) {
  PosTemplate *t = &s_templates[i];
  int w = t->count < POS_AVG_CAP ? t->count : POS_AVG_CAP;
  for (int k = 0; k < POS_FEATURES; k++) t->f[k] = (t->f[k] * w + f[k]) / (w + 1);
  if (t->count < 255) t->count++;
  save_templates();

  int32_t d;
  int other = nearest(t->f, i, &d);
  return (other >= 0 && d < POS_SIMILAR) ? other : -1;
}

// --- Aufnahme / Live-Test -------------------------------------------------

typedef enum { REC_INTRO, REC_READY, REC_RECORDING, REC_DONE, REC_FAILED, REC_LIVE } RecState;

#define READY_SECONDS 5
#define RECORD_SECONDS 20

static Window *s_rec_window;
static TextLayer *s_title, *s_text;
static char s_text_buf[200];
static RecState s_state;
static int s_pos;            // angelernte Stellung, -1 = Live-Test
static int s_seconds;
static Detector s_det;
static PosAcc s_acc;
static int32_t s_sum[POS_FEATURES];
static int s_windows;

static void show(void) {
  switch (s_state) {
    case REC_INTRO: snprintf(s_text_buf, sizeof(s_text_buf), "%s", tr(S_POS_INTRO)); break;
    case REC_READY:
      snprintf(s_text_buf, sizeof(s_text_buf), tr(S_POS_GET_READY_FMT), s_seconds);
      break;
    case REC_RECORDING:
      snprintf(s_text_buf, sizeof(s_text_buf), tr(S_POS_RECORDING_FMT), s_seconds);
      break;
    case REC_FAILED: snprintf(s_text_buf, sizeof(s_text_buf), "%s", tr(S_POS_TOO_LITTLE)); break;
    default: break;  // Text setzen finish_recording() bzw. der Live-Test
  }
  text_layer_set_text(s_text, s_text_buf);
}

static void finish_recording(void) {
  accel_data_service_unsubscribe();
  tick_timer_service_unsubscribe();
  // mindestens die Hälfte der Fenster mit Bewegung
  if (s_windows * 2 < RECORD_SECONDS * SAMPLE_HZ / POS_WINDOW) {
    s_state = REC_FAILED;
    vibes_double_pulse();
    show();
    return;
  }
  int16_t f[POS_FEATURES];
  for (int k = 0; k < POS_FEATURES; k++) f[k] = s_sum[k] / s_windows;
  int similar = train(s_pos, f);
  if (similar >= 0) {
    snprintf(s_text_buf, sizeof(s_text_buf), tr(S_POS_SIMILAR_FMT), s_templates[s_pos].count,
             pos_name(similar));
  } else {
    snprintf(s_text_buf, sizeof(s_text_buf), tr(S_POS_SAVED_FMT), s_templates[s_pos].count);
  }
  s_state = REC_DONE;
  vibes_double_pulse();
  show();
}

static void accel_handler(AccelData *data, uint32_t num) {
  if (s_state != REC_RECORDING && s_state != REC_LIVE) return;
  for (uint32_t i = 0; i < num; i++) {
    if (data[i].did_vibrate) continue;
    bool cycle = detector_process(&s_det, data[i].x, data[i].y, data[i].z);
    pos_acc_add(&s_acc, &s_det, data[i].x, data[i].y, data[i].z, cycle);
    if (!pos_acc_ready(&s_acc)) continue;

    int16_t f[POS_FEATURES];
    bool moving = pos_acc_features(&s_acc, s_det.moving, f);
    s_det.moving = 0;
    if (s_state == REC_LIVE) {
      snprintf(s_text_buf, sizeof(s_text_buf), "%s", moving ? pos_name(pos_classify(f)) : "...");
      text_layer_set_text(s_text, s_text_buf);
    } else if (moving) {
      for (int k = 0; k < POS_FEATURES; k++) s_sum[k] += f[k];
      s_windows++;
    }
  }
}

static void start_accel(void) {
  DetectorParams params;
  detector_params_for(MODE_PARTNER, &params);
  detector_init(&s_det, &params);
  pos_acc_reset(&s_acc);
  accel_data_service_subscribe(5, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
}

static void tick_handler(struct tm *t, TimeUnits changed) {
  if (s_state == REC_READY) {
    if (--s_seconds > 0) {
      show();
      return;
    }
    s_state = REC_RECORDING;
    s_seconds = RECORD_SECONDS;
    memset(s_sum, 0, sizeof(s_sum));
    s_windows = 0;
    vibes_short_pulse();
    start_accel();
    show();
  } else if (s_state == REC_RECORDING) {
    if (--s_seconds > 0) {
      show();
      return;
    }
    finish_recording();
  }
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
  switch (s_state) {
    case REC_INTRO:
    case REC_FAILED:
      s_state = REC_READY;
      s_seconds = READY_SECONDS;
      show();
      tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
      break;
    case REC_DONE:
      window_stack_pop(true);
      break;
    default:
      break;
  }
}

static void click_config(void *ctx) { window_single_click_subscribe(BUTTON_ID_SELECT, select_click); }

#if defined(PBL_TOUCH)
static void tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event == RecognizerEvent_Completed) select_click(NULL, NULL);
}
#endif

static void rec_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  bool big = b.size.h >= 200;

  s_title = text_layer_create(GRect(4, 4, b.size.w - 8, 28));
  text_layer_set_font(s_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title, GTextAlignmentCenter);
  text_layer_set_text_color(s_title, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack));
  text_layer_set_text(s_title, s_pos >= 0 ? pos_name(s_pos) : tr(S_POS_LIVE));
  layer_add_child(root, text_layer_get_layer(s_title));

  s_text = text_layer_create(GRect(6, 36, b.size.w - 12, b.size.h - 36));
  const char *font = big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  // Live-Test: erkannte Stellung groß anzeigen
  if (s_state == REC_LIVE && pos_trained_count() > 0) font = FONT_KEY_GOTHIC_28_BOLD;
  text_layer_set_font(s_text, fonts_get_system_font(font));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_text));

#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, tap_recognizer_create(tap_handler, NULL));
#endif

  if (s_state == REC_LIVE) {
    if (pos_trained_count() == 0) {
      text_layer_set_text(s_text, tr(S_POS_NONE_TRAINED));
    } else {
      text_layer_set_text(s_text, "...");
      start_accel();
    }
  } else {
    show();
  }
}

static void rec_unload(Window *window) {
  tick_timer_service_unsubscribe();
  accel_data_service_unsubscribe();
  text_layer_destroy(s_title);
  text_layer_destroy(s_text);
  window_destroy(s_rec_window);
  s_rec_window = NULL;
}

static void rec_window_push(int pos) {
  s_pos = pos;
  s_state = pos >= 0 ? REC_INTRO : REC_LIVE;
  s_rec_window = window_create();
  window_set_click_config_provider(s_rec_window, click_config);
  window_set_window_handlers(s_rec_window, (WindowHandlers){
                                               .load = rec_load,
                                               .unload = rec_unload,
                                           });
  window_stack_push(s_rec_window, true);
}

// --- Liste ---------------------------------------------------------------

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) { return POS_COUNT + 1; }

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  if (index->row == 0) {
    menu_cell_basic_draw(g, cell, tr(S_POS_LIVE), tr(S_POS_LIVE_SUB), NULL);
    return;
  }
  int i = index->row - 1;
  char sub[48];
  if (s_templates[i].count) {
    snprintf(sub, sizeof(sub), tr(S_POS_TRAINED_FMT), s_templates[i].count);
  } else {
    snprintf(sub, sizeof(sub), "%s", tr(S_POS_UNTRAINED));
  }
  menu_cell_basic_draw(g, cell, pos_name(i), sub, NULL);
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  rec_window_push((int)index->row - 1);
}

static void menu_select_long(MenuLayer *m, MenuIndex *index, void *ctx) {
  if (index->row == 0) return;
  PosTemplate *t = &s_templates[index->row - 1];
  if (!t->count) return;
  *t = (PosTemplate){0};
  save_templates();
  vibes_short_pulse();
  menu_layer_reload_data(m);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
                                             .get_num_rows = menu_rows,
                                             .draw_row = menu_draw,
                                             .select_click = menu_select,
                                             .select_long_click = menu_select_long,
                                         });
#if defined(PBL_COLOR)
  menu_layer_set_highlight_colors(s_menu, GColorFolly, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_appear(Window *window) { menu_layer_reload_data(s_menu); }

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void positions_window_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}
