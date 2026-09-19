#include "app.h"

// Anlernen: Nutzer:in macht CAL_SECONDS lang die typische Bewegung. Daraus
// werden Bewegungsenergie und Rhythmus bestimmt und die Erkennung darauf
// eingestellt. Zur Kontrolle läuft die neue Erkennung über dieselbe Aufnahme.
#define SAMPLE_HZ 25
#define CAL_SECONDS 15
#define CAL_SAMPLES (CAL_SECONDS * SAMPLE_HZ)
#define WARMUP SAMPLE_HZ  // Filter einschwingen lassen
#define COUNTDOWN 3
#define MIN_RMS 15        // mg: darunter zu wenig Bewegung
#define MIN_CYCLES 5

typedef enum { CAL_INTRO, CAL_COUNTDOWN, CAL_RECORDING, CAL_RESULT, CAL_FAILED } CalState;

static const SessionMode CALIB_MODE[CALIB_COUNT] = {
    [CALIB_STROKE] = MODE_SOLO, [CALIB_RUB] = MODE_SOLO_RUB, [CALIB_PARTNER] = MODE_PARTNER};

// --- Aufnahme-Fenster ----------------------------------------------------

static Window *s_rec_window;
static TextLayer *s_title, *s_text;
static char s_text_buf[200];
static CalState s_state;
static int s_which;
static int s_seconds;
static int16_t *s_samples;  // x, y, z verschachtelt
static int s_n;
static Calib s_result;

static void show(void) {
  switch (s_state) {
    case CAL_INTRO:
      snprintf(s_text_buf, sizeof(s_text_buf), tr(S_CAL_INTRO_FMT), CAL_SECONDS);
      break;
    case CAL_COUNTDOWN:
      snprintf(s_text_buf, sizeof(s_text_buf), tr(S_CAL_COUNTDOWN_FMT), s_seconds);
      break;
    case CAL_RECORDING:
      snprintf(s_text_buf, sizeof(s_text_buf), tr(S_CAL_RECORDING_FMT), s_seconds);
      break;
    case CAL_FAILED:
      snprintf(s_text_buf, sizeof(s_text_buf), "%s", tr(S_CAL_TOO_LITTLE));
      break;
    case CAL_RESULT:
      break;  // Text setzt analyze()
  }
  text_layer_set_text(s_text, s_text_buf);
}

static void accel_handler(AccelData *data, uint32_t num) {
  if (s_state != CAL_RECORDING) return;
  for (uint32_t i = 0; i < num && s_n < CAL_SAMPLES; i++) {
    if (data[i].did_vibrate) continue;
    s_samples[s_n * 3] = data[i].x;
    s_samples[s_n * 3 + 1] = data[i].y;
    s_samples[s_n * 3 + 2] = data[i].z;
    s_n++;
  }
}

static void analyze(void) {
  // 1. Energie und dominante Achse nach Abzug der Schwerkraft
  int32_t lp16[3] = {s_samples[0] * 16, s_samples[1] * 16, s_samples[2] * 16};
  int64_t sum_sq[3] = {0, 0, 0};
  int16_t *hp_all = s_samples;  // wird in-place durch HP-Werte ersetzt
  for (int i = 0; i < s_n; i++) {
    for (int a = 0; a < 3; a++) {
      int32_t v = s_samples[i * 3 + a];
      lp16[a] += (v * 16 - lp16[a]) / 16;
      int32_t hp = v - lp16[a] / 16;
      hp_all[i * 3 + a] = hp;
      if (i >= WARMUP) sum_sq[a] += hp * hp;
    }
  }
  int used = s_n - WARMUP;
  int dom = 0;
  for (int a = 1; a < 3; a++) {
    if (sum_sq[a] > sum_sq[dom]) dom = a;
  }
  int32_t energy = used > 0 ? (int32_t)(sum_sq[dom] / used) : 0;
  int32_t rms = isqrt32(energy);

  // 2. Rhythmus über Nulldurchgänge mit Hysterese (30 % RMS)
  int cycles = 0, phase = 0;
  int32_t hyst = rms * 3 / 10, smooth = 0;
  for (int i = WARMUP; i < s_n; i++) {
    smooth += (hp_all[i * 3 + dom] - smooth) / 2;
    if (phase == 0 && smooth < -hyst) phase = 1;
    else if (phase == 1 && smooth > hyst) {
      phase = 0;
      cycles++;
    }
  }

  if (used <= 0 || rms < MIN_RMS || cycles < MIN_CYCLES) {
    s_state = CAL_FAILED;
    vibes_double_pulse();
    show();
    return;
  }

  int spm = cycles * 60 * SAMPLE_HZ / used;
  int samples_per_cycle = used / cycles;
  s_result = (Calib){
      .valid = 1,
      .spm = spm > 255 ? 255 : spm,
      .min_gap = samples_per_cycle * 45 / 100,
      .energy = energy,
  };

  // 3. Kontrolle: neue Erkennung über die Originalaufnahme laufen lassen.
  //    Die Rohdaten sind überschrieben, daher HP-Werte + Schwerkraft 0 einspeisen:
  //    der Tiefpass des Detektors bleibt dann ~0 und die Zyklen sind identisch.
  DetectorParams params;
  detector_params_from_calib(CALIB_MODE[s_which], &s_result, &params);
  Detector d;
  detector_init(&d, &params);
  int detected = 0;
  for (int i = 0; i < s_n; i++) {
    if (detector_process(&d, hp_all[i * 3], hp_all[i * 3 + 1], hp_all[i * 3 + 2]) && i >= WARMUP) {
      detected++;
    }
  }

  snprintf(s_text_buf, sizeof(s_text_buf), tr(S_CAL_RESULT_FMT), spm, (int)rms, detected, cycles);
  s_state = CAL_RESULT;
  vibes_double_pulse();
  show();
}

static void tick_handler(struct tm *t, TimeUnits changed) {
  if (s_state == CAL_COUNTDOWN) {
    if (--s_seconds > 0) {
      show();
      return;
    }
    s_state = CAL_RECORDING;
    s_seconds = CAL_SECONDS;
    s_n = 0;
    vibes_short_pulse();
    show();
  } else if (s_state == CAL_RECORDING) {
    if (--s_seconds > 0 && s_n < CAL_SAMPLES) {
      show();
      return;
    }
    tick_timer_service_unsubscribe();
    analyze();
  }
}

static void start(void) {
  s_state = CAL_COUNTDOWN;
  s_seconds = COUNTDOWN;
  show();
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
  switch (s_state) {
    case CAL_INTRO:
    case CAL_FAILED:
      start();
      break;
    case CAL_RESULT:
      *storage_calib(s_which) = s_result;
      storage_save_calib();
      autostart_settings_changed();  // Worker nutzt die neue Erkennung
      vibes_short_pulse();
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

  s_title = text_layer_create(GRect(4, 4, b.size.w - 8, 28));
  text_layer_set_font(s_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title, GTextAlignmentCenter);
  text_layer_set_text_color(s_title, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack));
  text_layer_set_text(s_title, mode_name(CALIB_MODE[s_which]));
  layer_add_child(root, text_layer_get_layer(s_title));

  s_text = text_layer_create(GRect(6, 36, b.size.w - 12, b.size.h - 36));
  text_layer_set_font(s_text, fonts_get_system_font(b.size.h >= 200 ? FONT_KEY_GOTHIC_18_BOLD
                                                                    : FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_text));

#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, tap_recognizer_create(tap_handler, NULL));
#endif

  s_state = CAL_INTRO;
  show();
}

static void rec_unload(Window *window) {
  tick_timer_service_unsubscribe();
  accel_data_service_unsubscribe();
  free(s_samples);
  s_samples = NULL;
  text_layer_destroy(s_title);
  text_layer_destroy(s_text);
  window_destroy(s_rec_window);
  s_rec_window = NULL;
}

static void record_window_push(int which) {
  s_samples = malloc(CAL_SAMPLES * 3 * sizeof(int16_t));
  if (!s_samples) return;
  s_which = which;
  s_rec_window = window_create();
  window_set_click_config_provider(s_rec_window, click_config);
  window_set_window_handlers(s_rec_window, (WindowHandlers){
                                               .load = rec_load,
                                               .unload = rec_unload,
                                           });
  window_stack_push(s_rec_window, true);
  accel_data_service_subscribe(5, accel_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
}

// --- Auswahl der Bewegungsart --------------------------------------------

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) { return CALIB_COUNT; }

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  Calib *c = storage_calib(index->row);
  char sub[48];
  if (c->valid) {
    snprintf(sub, sizeof(sub), tr(S_CAL_TRAINED_FMT), c->spm);
  } else {
    snprintf(sub, sizeof(sub), "%s", tr(S_CAL_DEFAULT));
  }
  menu_cell_basic_draw(g, cell, mode_name(CALIB_MODE[index->row]), sub, NULL);
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) { record_window_push(index->row); }

static void menu_select_long(MenuLayer *m, MenuIndex *index, void *ctx) {
  Calib *c = storage_calib(index->row);
  if (!c->valid) return;
  *c = (Calib){0};
  storage_save_calib();
  autostart_settings_changed();
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

void calibrate_window_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}
