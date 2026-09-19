#include "app.h"

// --- Zahleneingabe -------------------------------------------------------

typedef void (*NumberCallback)(int value);

static Window *s_num_window;
static TextLayer *s_num_title, *s_num_value, *s_num_hint;
static char s_num_buf[24];
static int s_num_val, s_num_min, s_num_max;
static const char *s_num_unit;
static NumberCallback s_num_cb;

static void num_update(void) {
  snprintf(s_num_buf, sizeof(s_num_buf), "%d %s", s_num_val, s_num_unit);
  text_layer_set_text(s_num_value, s_num_buf);
}

static void num_up(ClickRecognizerRef rec, void *ctx) {
  if (s_num_val < s_num_max) s_num_val++;
  num_update();
}

static void num_down(ClickRecognizerRef rec, void *ctx) {
  if (s_num_val > s_num_min) s_num_val--;
  num_update();
}

static void num_select(ClickRecognizerRef rec, void *ctx) {
  s_num_cb(s_num_val);
  window_stack_pop(true);
}

#if defined(PBL_TOUCH)
#define PIXELS_PER_STEP 10
static int s_num_pan_start;

// Vertikal ziehen: nach oben = mehr, nach unten = weniger
static void num_pan_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event == RecognizerEvent_Started) {
    s_num_pan_start = s_num_val;
  } else if (event == RecognizerEvent_Updated) {
    int v = s_num_pan_start - pan_recognizer_get_delta_since_start(recognizer).y / PIXELS_PER_STEP;
    s_num_val = v < s_num_min ? s_num_min : v > s_num_max ? s_num_max : v;
    num_update();
  }
}

static void num_tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event == RecognizerEvent_Completed) num_select(NULL, NULL);
}

static void num_swipe_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event == RecognizerEvent_Completed) window_stack_pop(true);
}
#endif

static void num_click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 80, num_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 80, num_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, num_select);
}

static void num_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int mid = b.size.h / 2;

  s_num_value = text_layer_create(GRect(0, mid - 22, b.size.w, 40));
  text_layer_set_font(s_num_value, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_num_value, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_num_value));

  s_num_hint = text_layer_create(GRect(4, mid + 22, b.size.w - 8, 40));
  text_layer_set_font(s_num_hint, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_num_hint, GTextAlignmentCenter);
  text_layer_set_text(s_num_hint, tr(S_NUM_HINT));
  layer_add_child(root, text_layer_get_layer(s_num_hint));

#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, pan_recognizer_create(num_pan_handler, NULL, PanAxis_Vertical));
  window_attach_recognizer(window, tap_recognizer_create(num_tap_handler, NULL));
  window_attach_recognizer(window,
                           swipe_recognizer_create(num_swipe_handler, NULL, SwipeDirection_Right));
  text_layer_set_text(s_num_hint, tr(S_NUM_HINT_TOUCH));
#endif

  num_update();
}

static void num_unload(Window *window) {
  text_layer_destroy(s_num_title);
  text_layer_destroy(s_num_value);
  text_layer_destroy(s_num_hint);
  window_destroy(s_num_window);
  s_num_window = NULL;
}

static void number_window_push(const char *title, int value, int min, int max, const char *unit,
                               NumberCallback cb) {
  s_num_val = value;
  s_num_min = min;
  s_num_max = max;
  s_num_unit = unit;
  s_num_cb = cb;

  s_num_window = window_create();
  window_set_click_config_provider(s_num_window, num_click_config);
  window_set_window_handlers(s_num_window, (WindowHandlers){
                                               .load = num_load,
                                               .unload = num_unload,
                                           });
  Layer *root = window_get_root_layer(s_num_window);
  s_num_title = text_layer_create(GRect(0, 10, layer_get_bounds(root).size.w, 30));
  text_layer_set_font(s_num_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_num_title, GTextAlignmentCenter);
  text_layer_set_text(s_num_title, title);
  layer_add_child(root, text_layer_get_layer(s_num_title));
  window_stack_push(s_num_window, true);
}

// --- Einstellungsmenü ----------------------------------------------------

enum {
  ROW_SEX,
  ROW_AGE,
  ROW_WEIGHT,
  ROW_STYLE,
  ROW_SENS,
  ROW_TOUCH,
  ROW_CHECKIN,
  ROW_AUTO,
  ROW_LIGHT,
  ROW_ASK_PARTNER,
  ROW_LANG,
  ROW_WIPE,
  ROW_COUNT
};

static Window *s_window;
static MenuLayer *s_menu;
static bool s_wipe_armed;

static const StrId STYLE_STR[] = {S_STYLE_AUTO, S_STYLE_STROKE, S_STYLE_RUB, S_STYLE_TOY};
static const StrId SEX_STR[] = {S_SEX_M, S_SEX_F, S_SEX_D};
static const StrId SENS_STR[] = {S_SENS_LOW, S_SENS_NORMAL, S_SENS_HIGH};

static void profile_changed(void) {
  storage_save_profile();
  autostart_settings_changed();
  settings_send_profile();  // hält die Einstellungsseite am Handy aktuell
}

static void set_age(int v) {
  storage_profile()->age = v;
  profile_changed();
}

static void set_weight(int v) {
  storage_profile()->weight_kg = v;
  profile_changed();
}

// Die Touch-Zeile gibt es nur auf Uhren mit Touchscreen
static int row_for(int index) {
#if defined(PBL_TOUCH)
  return index;
#else
  return index >= ROW_TOUCH ? index + 1 : index;
#endif
}

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) {
#if defined(PBL_TOUCH)
  return ROW_COUNT;
#else
  return ROW_COUNT - 1;
#endif
}

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  Profile *p = storage_profile();
  char sub[32];
  const char *title = "", *value = sub;
  switch (row_for(index->row)) {
    case ROW_SEX:
      title = tr(S_P_SEX);
      value = tr(SEX_STR[p->sex <= SEX_OTHER ? p->sex : 0]);
      break;
    case ROW_AGE:
      title = tr(S_P_AGE);
      snprintf(sub, sizeof(sub), tr(S_AGE_FMT), p->age);
      break;
    case ROW_WEIGHT:
      title = tr(S_P_WEIGHT);
      snprintf(sub, sizeof(sub), "%d kg", p->weight_kg);
      break;
    case ROW_STYLE:
      title = tr(S_P_STYLE);
      if (p->solo_style == STYLE_AUTO) {
        snprintf(sub, sizeof(sub), "%s (%s)", tr(S_STYLE_AUTO), mode_name(solo_mode()));
      } else {
        value = tr(STYLE_STR[p->solo_style <= STYLE_TOY ? p->solo_style : 0]);
      }
      break;
    case ROW_SENS:
      title = tr(S_P_SENS);
      value = tr(SENS_STR[p->sensitivity <= SENS_HIGH ? p->sensitivity : SENS_NORMAL]);
      break;
    case ROW_TOUCH:
      title = tr(S_P_TOUCH);
      value = tr(p->touch_session ? S_ON : S_OFF);
      break;
    case ROW_CHECKIN:
      title = tr(S_P_CHECKIN);
      value = tr(p->checkin ? S_ON : S_OFF);
      break;
    case ROW_AUTO:
      title = tr(S_P_AUTO);
      if (p->auto_start) {
        snprintf(sub, sizeof(sub), tr(S_AUTO_AFTER_FMT), p->auto_start);
      } else {
        value = tr(S_OFF);
      }
      break;
    case ROW_LIGHT:
      title = tr(S_P_LIGHT);
      value = tr(p->light == LIGHT_ON ? S_LIGHT_ON : p->light == LIGHT_PULSE ? S_LIGHT_PULSE
                                                                            : S_LIGHT_NORMAL);
      break;
    case ROW_ASK_PARTNER:
      title = tr(S_P_ASK_PARTNER);
      value = tr(p->ask_partner ? S_ON : S_OFF);
      break;
    case ROW_LANG:
      title = tr(S_P_LANG);
      value = p->lang ? lang_name(p->lang - 1) : tr(S_STYLE_AUTO);
      break;
    case ROW_WIPE:
      title = tr(S_P_WIPE);
      value = tr(s_wipe_armed ? S_WIPE_ARMED : S_WIPE_HINT);
      break;
  }
  menu_cell_basic_draw(g, cell, title, value, NULL);
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  Profile *p = storage_profile();
  switch (row_for(index->row)) {
    case ROW_SEX: p->sex = (p->sex + 1) % 3; break;
    case ROW_STYLE: p->solo_style = (p->solo_style + 1) % 4; break;
    case ROW_SENS: p->sensitivity = (p->sensitivity + 1) % 3; break;
    case ROW_TOUCH: p->touch_session = !p->touch_session; break;
    case ROW_CHECKIN: p->checkin = !p->checkin; break;
    case ROW_AUTO: p->auto_start = (p->auto_start + 1) % 4; break;
    case ROW_LIGHT: p->light = (p->light + 1) % 3; break;
    case ROW_ASK_PARTNER: p->ask_partner = !p->ask_partner; break;
    case ROW_LANG:
      p->lang = (p->lang + 1) % (LANG_COUNT + 1);
      i18n_load();
      break;
    case ROW_AGE:
      number_window_push(tr(S_P_AGE), p->age, 16, 99, tr(S_AGE_UNIT), set_age);
      return;
    case ROW_WEIGHT:
      number_window_push(tr(S_P_WEIGHT), p->weight_kg, 35, 250, "kg", set_weight);
      return;
    default:
      return;
  }
  profile_changed();
  menu_layer_reload_data(m);
}

static void menu_select_long(MenuLayer *m, MenuIndex *index, void *ctx) {
  if (row_for(index->row) != ROW_WIPE) return;
  if (!s_wipe_armed) {
    s_wipe_armed = true;
    vibes_short_pulse();
  } else {
    storage_delete_all();
    s_wipe_armed = false;
    vibes_double_pulse();
  }
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

void profile_window_push(void) {
  s_wipe_armed = false;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}
