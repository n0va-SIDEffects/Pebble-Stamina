#include "app.h"

enum { ROW_SOLO, ROW_PARTNER, ROW_HISTORY, ROW_STATS, ROW_CALIB, ROW_SETTINGS, ROW_COUNT };

static Window *s_window;
static MenuLayer *s_menu;
static char s_sub[48];

static void start_partner(int partner) {
  tracker_window_push_at(MODE_PARTNER, time(NULL), 0, partner);
}

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) { return ROW_COUNT; }

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  switch (index->row) {
    case ROW_SOLO:
      menu_cell_basic_draw(g, cell, tr(S_MENU_SOLO), mode_name(solo_mode()), NULL);
      break;
    case ROW_PARTNER:
      menu_cell_basic_draw(g, cell, tr(S_MENU_PARTNER), tr(S_SUB_PARTNER), NULL);
      break;
    case ROW_HISTORY:
      snprintf(s_sub, sizeof(s_sub), tr(S_SUB_HISTORY_FMT), storage_count());
      menu_cell_basic_draw(g, cell, tr(S_MENU_HISTORY), s_sub, NULL);
      break;
    case ROW_STATS:
      menu_cell_basic_draw(g, cell, tr(S_MENU_STATS), tr(S_SUB_STATS), NULL);
      break;
    case ROW_CALIB:
      menu_cell_basic_draw(g, cell, tr(S_MENU_CALIB), tr(S_SUB_CALIB), NULL);
      break;
    case ROW_SETTINGS:
      menu_cell_basic_draw(g, cell, tr(S_MENU_SETTINGS), tr(S_SUB_SETTINGS), NULL);
      break;
  }
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  switch (index->row) {
    case ROW_SOLO: tracker_window_push(solo_mode()); break;
    case ROW_PARTNER: partner_choose(start_partner); break;
    case ROW_HISTORY: history_window_push(); break;
    case ROW_STATS: stats_window_push(); break;
    case ROW_CALIB: calibrate_window_push(); break;
    case ROW_SETTINGS: profile_window_push(); break;
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
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

static void window_appear(Window *window) {
  i18n_load();  // Sprache kann sich in den Einstellungen geändert haben
  for (int i = 0; i < storage_count(); i++) {
    Session *s = storage_get(i);
    bool changed = sleep_evaluate(s);
    changed |= morning_evaluate(s);
    if (changed) storage_save(i);
  }
  menu_layer_reload_data(s_menu);
}

static void window_unload(Window *window) { menu_layer_destroy(s_menu); }

static void init(void) {
  storage_init();
  positions_init();
  partners_init();
  i18n_load();
  settings_init();
  morning_init();
  autostart_init();
#if defined(PBL_TOUCH)
  // Wischen scrollt Menüs/Textansichten, Tippen wählt aus
  app_touch_navigation_enable(true);
#endif
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
  // Vom Worker gestartet: Session-Nachfrage. Sonst ggf. Stimmung nach einer Morgen-Session
  if (!autostart_handle_launch()) morning_handle_launch();
}

static void deinit(void) { window_destroy(s_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
