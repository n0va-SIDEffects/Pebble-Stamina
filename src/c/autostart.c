#include "app.h"

// Automatischer Session-Start: Der Hintergrund-Worker (worker_src/) erkennt
// anhaltende rhythmische Bewegung und meldet sich. Hier wird nachgefragt und
// die Session auf den Beginn der Bewegung rückdatiert gestartet.

#define PROMPT_TIMEOUT_MS 60000
#define DETECTION_MAX_AGE (30 * 60)  // ältere Erkennungen nicht mehr anbieten

enum { ROW_SOLO, ROW_PARTNER, ROW_NO, ROW_COUNT };

static Window *s_window;
static MenuLayer *s_menu;
static TextLayer *s_title, *s_text;
static char s_text_buf[96];
static AutoDetect s_det;
static bool s_launched_by_worker;
static bool s_session_active;
static AppTimer *s_timeout;
static int s_choice;

static void send_to_worker(uint8_t type) {
  if (!app_worker_is_running()) return;
  AppWorkerMessage msg = {0};
  app_worker_send_message(type, &msg);
}

void autostart_session_active(bool active) {
  s_session_active = active;
  send_to_worker(active ? WMSG_SESSION_ACTIVE : WMSG_SESSION_IDLE);
}

void autostart_settings_changed(void) {
  if (storage_profile()->auto_start) {
    if (app_worker_is_running()) {
      send_to_worker(WMSG_RELOAD);
    } else {
      app_worker_launch();
    }
  } else if (app_worker_is_running()) {
    app_worker_kill();
  }
}

// --- Nachfrage -----------------------------------------------------------

// Fensterwechsel erst nach dem Klick-Handler (siehe tracker.c)
static void apply_choice(void *ctx) {
  Window *w = s_window;
  switch (s_choice) {
    case ROW_SOLO: tracker_window_push_at(solo_mode(), s_det.start, s_det.cycles); break;
    case ROW_PARTNER: tracker_window_push_at(MODE_PARTNER, s_det.start, s_det.cycles); break;
    default:
      // Vom Worker gestartet und abgelehnt: App gleich wieder schließen
      if (s_launched_by_worker) {
        window_stack_pop_all(true);
        return;
      }
      break;
  }
  window_stack_remove(w, s_choice == ROW_NO);
}

static void choose(int choice) {
  if (s_timeout) {
    app_timer_cancel(s_timeout);
    s_timeout = NULL;
  }
  s_choice = choice;
  app_timer_register(10, apply_choice, NULL);
}

static void timeout_handler(void *ctx) {
  s_timeout = NULL;
  choose(ROW_NO);
}

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) { return ROW_COUNT; }

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  switch (index->row) {
    case ROW_SOLO:
      menu_cell_basic_draw(g, cell, tr(S_HDR_SOLO), mode_name(solo_mode()), NULL);
      break;
    case ROW_PARTNER:
      menu_cell_basic_draw(g, cell, tr(S_STYLE_PARTNER), NULL, NULL);
      break;
    case ROW_NO:
      menu_cell_basic_draw(g, cell, tr(S_AUTO_NO), NULL, NULL);
      break;
  }
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) { choose(index->row); }

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int head = b.size.h >= 200 ? 76 : 62;

  s_title = text_layer_create(GRect(4, 0, b.size.w - 8, 28));
  text_layer_set_font(s_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title, GTextAlignmentCenter);
  text_layer_set_text_color(s_title, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack));
  text_layer_set_text(s_title, tr(S_AUTO_TITLE));
  layer_add_child(root, text_layer_get_layer(s_title));

  char dur[12];
  fmt_duration(dur, sizeof(dur), time(NULL) - s_det.start);
  snprintf(s_text_buf, sizeof(s_text_buf), tr(S_AUTO_Q_FMT), dur);
  s_text = text_layer_create(GRect(4, 28, b.size.w - 8, head - 28));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  text_layer_set_text(s_text, s_text_buf);
  layer_add_child(root, text_layer_get_layer(s_text));

  s_menu = menu_layer_create(GRect(0, head, b.size.w, b.size.h - head));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
                                             .get_num_rows = menu_rows,
                                             .draw_row = menu_draw,
                                             .select_click = menu_select,
                                         });
#if defined(PBL_COLOR)
  menu_layer_set_highlight_colors(s_menu, GColorFolly, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  if (s_timeout) {
    app_timer_cancel(s_timeout);
    s_timeout = NULL;
  }
  menu_layer_destroy(s_menu);
  text_layer_destroy(s_title);
  text_layer_destroy(s_text);
  window_destroy(s_window);
  s_window = NULL;
}

static bool show_prompt(bool launched_by_worker) {
  if (s_window || s_session_active || !persist_exists(KEY_AUTO)) return false;
  persist_read_data(KEY_AUTO, &s_det, sizeof(s_det));
  persist_delete(KEY_AUTO);  // nur einmal nachfragen
  time_t age = time(NULL) - s_det.start;
  if (age < 0 || age > DETECTION_MAX_AGE) return false;

  s_launched_by_worker = launched_by_worker;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
  vibes_short_pulse();
  s_timeout = app_timer_register(PROMPT_TIMEOUT_MS, timeout_handler, NULL);
  return true;
}

static void worker_message(uint16_t type, AppWorkerMessage *data) {
  if (type == WMSG_DETECTED) show_prompt(false);
}

void autostart_init(void) {
  app_worker_message_subscribe(worker_message);
  autostart_settings_changed();
}

bool autostart_handle_launch(void) {
  return launch_reason() == APP_LAUNCH_WORKER && show_prompt(true);
}
