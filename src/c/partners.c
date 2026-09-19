#include "app.h"

// Partner-Kürzel: Beim Start einer Partner-Session wird gefragt, mit wem.
// Kürzel werden auf der Uhr eingegeben (Buchstaben-Auswahl) oder in der Pebble-App.

#define INITIALS_MAX 3  // auf der Uhr; in der Pebble-App bis PARTNER_LEN - 1

static char s_partners[PARTNER_COUNT][PARTNER_LEN];

void partners_init(void) {
  if (persist_exists(KEY_PARTNERS)) persist_read_data(KEY_PARTNERS, s_partners, sizeof(s_partners));
}

static void save(void) { persist_write_data(KEY_PARTNERS, s_partners, sizeof(s_partners)); }

// 1..PARTNER_COUNT, 0 = keine Angabe
const char *partner_name(int partner) {
  if (partner < 1 || partner > PARTNER_COUNT) return "";
  return s_partners[partner - 1];
}

void partner_set_name(int partner, const char *name) {
  char *dst = s_partners[partner - 1];
  size_t n = strlen(name);
  if (n > PARTNER_LEN - 1) {
    n = PARTNER_LEN - 1;
    while (n > 0 && ((uint8_t)name[n] & 0xC0) == 0x80) n--;  // ganze UTF-8-Zeichen
  }
  memcpy(dst, name, n);
  dst[n] = '\0';
  save();
}

static int free_partner(void) {
  for (int i = 0; i < PARTNER_COUNT; i++) {
    if (!s_partners[i][0]) return i + 1;
  }
  return 0;
}

// --- Kürzel eingeben -----------------------------------------------------

static const char CHARSET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#define CHARSET_LEN ((int)sizeof(CHARSET) - 1)

typedef void (*InitialsCallback)(const char *initials);

static Window *s_ini_window;
static TextLayer *s_ini_title, *s_ini_hint;
static TextLayer *s_ini_char[INITIALS_MAX];
static char s_ini_buf[INITIALS_MAX][2];
static int s_ini_idx[INITIALS_MAX];
static int s_ini_pos;
static InitialsCallback s_ini_cb;
static char s_ini_result[INITIALS_MAX + 1];

static void ini_update(void) {
  for (int i = 0; i < INITIALS_MAX; i++) {
    s_ini_buf[i][0] = CHARSET[s_ini_idx[i]] == ' ' ? '_' : CHARSET[s_ini_idx[i]];
    text_layer_set_text(s_ini_char[i], s_ini_buf[i]);
    bool active = i == s_ini_pos;
    text_layer_set_background_color(s_ini_char[i], active ? ACCENT_COLOR : GColorClear);
    text_layer_set_text_color(s_ini_char[i],
                              active ? PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack) : GColorBlack);
  }
}

static void ini_change(int delta) {
  s_ini_idx[s_ini_pos] = (s_ini_idx[s_ini_pos] + delta + CHARSET_LEN) % CHARSET_LEN;
  ini_update();
}

static void ini_finish(void *ctx) {
  int n = 0;
  for (int i = 0; i < INITIALS_MAX; i++) {
    if (CHARSET[s_ini_idx[i]] != ' ') s_ini_result[n++] = CHARSET[s_ini_idx[i]];
  }
  s_ini_result[n] = '\0';
  Window *w = s_ini_window;
  if (n) s_ini_cb(s_ini_result);
  window_stack_remove(w, !n);
}

static void ini_done(void) { app_timer_register(10, ini_finish, NULL); }

static void ini_up(ClickRecognizerRef rec, void *ctx) { ini_change(1); }
static void ini_down(ClickRecognizerRef rec, void *ctx) { ini_change(-1); }

static void ini_select(ClickRecognizerRef rec, void *ctx) {
  if (s_ini_pos < INITIALS_MAX - 1) {
    s_ini_pos++;
    if (!s_ini_idx[s_ini_pos]) s_ini_idx[s_ini_pos] = s_ini_idx[s_ini_pos - 1];
    ini_update();
  } else {
    ini_done();
  }
}

static void ini_select_long(ClickRecognizerRef rec, void *ctx) { ini_done(); }

static void ini_back(ClickRecognizerRef rec, void *ctx) {
  if (s_ini_pos > 0) {
    s_ini_idx[s_ini_pos] = 0;
    s_ini_pos--;
    ini_update();
  } else {
    window_stack_pop(true);
  }
}

static void ini_click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 120, ini_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 120, ini_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, ini_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, ini_select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, ini_back);
}

#if defined(PBL_TOUCH)
// Hoch/runter wischen: Buchstabe wechseln, tippen: nächste Stelle
static void ini_swipe(const Recognizer *r, RecognizerEvent e) {
  if (e != RecognizerEvent_Completed) return;
  SwipeDirection d = swipe_recognizer_get_direction(r);
  if (d == SwipeDirection_Up) ini_change(1);
  if (d == SwipeDirection_Down) ini_change(-1);
}

static void ini_tap(const Recognizer *r, RecognizerEvent e) {
  if (e == RecognizerEvent_Completed) ini_select(NULL, NULL);
}
#endif

static void ini_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int cw = 44, gap = 8, total = INITIALS_MAX * cw + (INITIALS_MAX - 1) * gap;
  int x0 = (b.size.w - total) / 2, y = b.size.h / 2 - 34;

  s_ini_title = text_layer_create(GRect(4, 6, b.size.w - 8, 30));
  text_layer_set_font(s_ini_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_ini_title, GTextAlignmentCenter);
  text_layer_set_text(s_ini_title, tr(S_INITIALS_TITLE));
  layer_add_child(root, text_layer_get_layer(s_ini_title));

  for (int i = 0; i < INITIALS_MAX; i++) {
    s_ini_char[i] = text_layer_create(GRect(x0 + i * (cw + gap), y, cw, 52));
    text_layer_set_font(s_ini_char[i], fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(s_ini_char[i], GTextAlignmentCenter);
    layer_add_child(root, text_layer_get_layer(s_ini_char[i]));
    s_ini_buf[i][1] = '\0';
  }

  s_ini_hint = text_layer_create(GRect(4, y + 60, b.size.w - 8, b.size.h - y - 60));
  text_layer_set_font(s_ini_hint, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_ini_hint, GTextAlignmentCenter);
  text_layer_set_text(s_ini_hint, tr(S_INITIALS_HINT));
  layer_add_child(root, text_layer_get_layer(s_ini_hint));

#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, swipe_recognizer_create(ini_swipe, NULL,
                                                           SwipeDirection_Up | SwipeDirection_Down));
  window_attach_recognizer(window, tap_recognizer_create(ini_tap, NULL));
#endif
  ini_update();
}

static void ini_unload(Window *window) {
  for (int i = 0; i < INITIALS_MAX; i++) text_layer_destroy(s_ini_char[i]);
  text_layer_destroy(s_ini_title);
  text_layer_destroy(s_ini_hint);
  window_destroy(s_ini_window);
  s_ini_window = NULL;
}

static void initials_window_push(InitialsCallback cb) {
  s_ini_cb = cb;
  s_ini_pos = 0;
  memset(s_ini_idx, 0, sizeof(s_ini_idx));
  s_ini_idx[0] = 1;  // "A"
  s_ini_window = window_create();
  window_set_click_config_provider(s_ini_window, ini_click_config);
  window_set_window_handlers(s_ini_window, (WindowHandlers){
                                               .load = ini_load,
                                               .unload = ini_unload,
                                           });
  window_stack_push(s_ini_window, true);
}

// --- Auswahl: mit wem? ---------------------------------------------------

static Window *s_window;
static MenuLayer *s_menu;
static PartnerCallback s_cb;
static int s_rows[PARTNER_COUNT];  // Zeile -> Partner (1-basiert)
static int s_row_count;
static int s_delete_armed;         // Partner, der beim nächsten Halten gelöscht wird
static int s_chosen;

static void rebuild(void) {
  s_row_count = 0;
  for (int i = 0; i < PARTNER_COUNT; i++) {
    if (s_partners[i][0]) s_rows[s_row_count++] = i + 1;
  }
}

// Reihenfolge: gespeicherte Partner, neuer Partner, keine Angabe
static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) { return s_row_count + 2; }

static int16_t header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void draw_header(GContext *g, const Layer *cell, uint16_t section, void *ctx) {
  menu_cell_basic_header_draw(g, cell, tr(S_PARTNER_CHOOSE));
}

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  int row = index->row;
  if (row < s_row_count) {
    int p = s_rows[row];
    menu_cell_basic_draw(g, cell, partner_name(p),
                         p == s_delete_armed ? tr(S_PARTNER_DELETE_ARMED) : NULL, NULL);
  } else if (row == s_row_count) {
    menu_cell_basic_draw(g, cell, tr(S_PARTNER_NEW),
                         free_partner() ? tr(S_PARTNER_NEW_SUB) : tr(S_POS_FULL), NULL);
  } else {
    menu_cell_basic_draw(g, cell, tr(S_PARTNER_NONE), NULL, NULL);
  }
}

// Weiter zur Session, danach dieses Fenster entfernen (nicht im Klick-Handler)
static void finish(void *ctx) {
  Window *w = s_window;
  s_cb(s_chosen);
  window_stack_remove(w, false);
}

static void choose(int partner) {
  s_chosen = partner;
  app_timer_register(10, finish, NULL);
}

static void new_partner_done(const char *initials) {
  int p = free_partner();
  if (!p) return;
  partner_set_name(p, initials);
  settings_send_profile();
  choose(p);
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  int row = index->row;
  if (row < s_row_count) {
    choose(s_rows[row]);
  } else if (row == s_row_count) {
    if (free_partner()) initials_window_push(new_partner_done);
  } else {
    choose(0);
  }
}

static void menu_select_long(MenuLayer *m, MenuIndex *index, void *ctx) {
  if (index->row >= s_row_count) return;
  int p = s_rows[index->row];
  if (s_delete_armed != p) {
    s_delete_armed = p;
    vibes_short_pulse();
  } else {
    s_partners[p - 1][0] = '\0';
    save();
    settings_send_profile();
    s_delete_armed = 0;
    vibes_double_pulse();
    rebuild();
  }
  menu_layer_reload_data(m);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
                                             .get_num_rows = menu_rows,
                                             .get_header_height = header_height,
                                             .draw_header = draw_header,
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

static void window_appear(Window *window) {
  rebuild();
  menu_layer_reload_data(s_menu);
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void partner_choose(PartnerCallback cb) {
  if (!storage_profile()->ask_partner) {
    cb(0);
    return;
  }
  s_cb = cb;
  s_delete_armed = 0;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}
