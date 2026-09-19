#include "app.h"

// Erfolge ("Achievements"): werden beim Speichern einer Session und beim Öffnen
// der App geprüft. Summen (Sessions, kcal, ...) werden separat gezählt, weil der
// Verlauf nur die letzten MAX_SESSIONS Einträge behält.

// Reihenfolge = Reihenfolge der Texte B_xxx / B_xxx_D in i18n/en.json
enum {
  B_FIRST, B_TEN, B_FIFTY, B_HUNDRED, B_MARATHON, B_SPRINT, B_BURNER, B_KCAL, B_MOVES,
  B_HEART, B_EARLY, B_NIGHT, B_STREAK3, B_STREAK7, B_DREAMS, B_DEEP, B_MORNING, B_TEAM,
  B_EXPLORER, B_COACH, B_MARK,
  B_250, B_500, B_MOVES50K, B_MOVES100K, B_KCAL5K, B_HOURS10, B_HOURS50, B_MARK50,
  B_PARTNER10, B_PARTNER50, B_SOLO50, B_ENDURANCE, B_QUICK, B_RHYTHM, B_TURBO, B_T1000,
  B_T2000, B_FURNACE, B_REDZONE, B_ZEN, B_LUNCH, B_WEEKEND, B_TWICE, B_TRIPLE, B_STREAK14,
  B_STREAK30, B_COMEBACK, B_NEWYEAR, B_VALENTINE, B_VARIETY, B_POS5, B_POS8, B_AUTO, B_LIGHT,
  B_DREAMWEEK, B_SUNSHINE, B_CHECKIN, B_ANALYST,
  BADGE_COUNT
};

_Static_assert(BADGE_COUNT <= 64, "Bitmaske reicht für höchstens 64 Erfolge");

typedef struct __attribute__((packed)) {
  uint64_t unlocked;            // Bitmaske
  uint32_t when[BADGE_COUNT];   // Zeitpunkt der Freischaltung
} BadgeState;

_Static_assert(sizeof(BadgeState) <= PERSIST_DATA_MAX_LENGTH, "Erfolge passen nicht in einen Schlüssel");

typedef struct __attribute__((packed)) {
  uint16_t sessions;
  uint32_t moves;
  uint32_t kcal_x10;
  uint16_t marks;               // markierte Höhepunkte
  int32_t last_day;             // lokaler Tag der letzten Session (für Serien)
  uint16_t streak;
  uint16_t best_streak;
  uint32_t secs;                // Gesamtdauer
  uint16_t solo, partner;
  uint8_t day_sessions;         // Sessions am Tag last_day
  uint8_t modes;                // Bitmaske der genutzten Solo-Stile
  int32_t saturday;             // letzter Samstag mit Session
  uint8_t stats_seen;
} Totals;

static BadgeState s_badges;
static Totals s_totals;

static const char *name(int b) { return tr(S_B_FIRST + 2 * b); }
static const char *description(int b) { return tr(S_B_FIRST + 2 * b + 1); }
static bool has(int b) { return s_badges.unlocked & (1ull << b); }

int badges_unlocked_count(void) {
  int n = 0;
  for (int b = 0; b < BADGE_COUNT; b++) n += has(b);
  return n;
}

int badges_total(void) { return BADGE_COUNT; }

static void save(void) {
  persist_write_data(KEY_BADGES, &s_badges, sizeof(s_badges));
  persist_write_data(KEY_TOTALS, &s_totals, sizeof(s_totals));
}

// Setzt die Bitmaske der neu freigeschalteten Erfolge
static void unlock(int b, bool condition, time_t when, uint64_t *fresh) {
  if (!condition || has(b)) return;
  s_badges.unlocked |= 1ull << b;
  s_badges.when[b] = when;
  *fresh |= 1ull << b;
}

static uint64_t apply_session(const Session *s) {
  uint64_t fresh = 0;
  time_t now = s->start + s->duration_s;

  bool had_sessions = s_totals.sessions > 0;
  s_totals.sessions++;
  s_totals.moves += s->strokes;
  s_totals.kcal_x10 += s->kcal_x10;
  s_totals.secs += s->duration_s;
  if (s->climax_s) s_totals.marks++;
  if (s->mode == MODE_PARTNER) {
    s_totals.partner++;
  } else {
    s_totals.solo++;
    s_totals.modes |= 1 << s->mode;
  }

  int32_t day = local_day_index(s->start);
  bool comeback = had_sessions && day - s_totals.last_day >= 30;
  if (day == s_totals.last_day) {
    if (s_totals.day_sessions < 255) s_totals.day_sessions++;
  } else {
    s_totals.streak = day == s_totals.last_day + 1 ? s_totals.streak + 1 : 1;
    s_totals.day_sessions = 1;
  }
  s_totals.last_day = day;
  if (s_totals.streak > s_totals.best_streak) s_totals.best_streak = s_totals.streak;

  time_t start = s->start;
  struct tm tm = *localtime(&start);
  int hour = tm.tm_hour;
  bool weekend = false;
  if (tm.tm_wday == 6) s_totals.saturday = day;
  if (tm.tm_wday == 0) weekend = s_totals.saturday == day - 1;

  int positions = 0;
  for (int i = 0; i < POS_COUNT; i++) positions += s->pos_pct[i] > 0;
  const uint8_t all_styles = (1 << MODE_SOLO) | (1 << MODE_SOLO_RUB) | (1 << MODE_SOLO_TOY);

  unlock(B_FIRST, s_totals.sessions >= 1, now, &fresh);
  unlock(B_TEN, s_totals.sessions >= 10, now, &fresh);
  unlock(B_FIFTY, s_totals.sessions >= 50, now, &fresh);
  unlock(B_HUNDRED, s_totals.sessions >= 100, now, &fresh);
  unlock(B_MARATHON, s->duration_s >= 30 * 60, now, &fresh);
  unlock(B_SPRINT, s->spm_max >= 180, now, &fresh);
  unlock(B_BURNER, s->kcal_x10 >= 1000, now, &fresh);
  unlock(B_KCAL, s_totals.kcal_x10 >= 10000, now, &fresh);
  unlock(B_MOVES, s_totals.moves >= 10000, now, &fresh);
  unlock(B_HEART, s->hr_max >= 150, now, &fresh);
  unlock(B_EARLY, hour >= 4 && hour < 9, now, &fresh);
  unlock(B_NIGHT, hour < 4, now, &fresh);
  unlock(B_STREAK3, s_totals.streak >= 3, now, &fresh);
  unlock(B_STREAK7, s_totals.streak >= 7, now, &fresh);
  unlock(B_TEAM, s->mode == MODE_PARTNER, now, &fresh);
  unlock(B_EXPLORER, positions >= 3, now, &fresh);
  unlock(B_MARK, s_totals.marks >= 10, now, &fresh);

  unlock(B_250, s_totals.sessions >= 250, now, &fresh);
  unlock(B_500, s_totals.sessions >= 500, now, &fresh);
  unlock(B_MOVES50K, s_totals.moves >= 50000, now, &fresh);
  unlock(B_MOVES100K, s_totals.moves >= 100000, now, &fresh);
  unlock(B_KCAL5K, s_totals.kcal_x10 >= 50000, now, &fresh);
  unlock(B_HOURS10, s_totals.secs >= 10 * 3600, now, &fresh);
  unlock(B_HOURS50, s_totals.secs >= 50 * 3600, now, &fresh);
  unlock(B_MARK50, s_totals.marks >= 50, now, &fresh);
  unlock(B_PARTNER10, s_totals.partner >= 10, now, &fresh);
  unlock(B_PARTNER50, s_totals.partner >= 50, now, &fresh);
  unlock(B_SOLO50, s_totals.solo >= 50, now, &fresh);
  unlock(B_ENDURANCE, s->duration_s >= 60 * 60, now, &fresh);
  unlock(B_QUICK, s->climax_s > 0 && s->climax_s <= 5 * 60, now, &fresh);
  unlock(B_RHYTHM, s->active_s >= 10 * 60 && s->spm_avg >= 120, now, &fresh);
  unlock(B_TURBO, s->spm_max >= 240, now, &fresh);
  unlock(B_T1000, s->strokes >= 1000, now, &fresh);
  unlock(B_T2000, s->strokes >= 2000, now, &fresh);
  unlock(B_FURNACE, s->kcal_x10 >= 2000, now, &fresh);
  unlock(B_REDZONE, s->hr_max >= 170, now, &fresh);
  unlock(B_ZEN, s->duration_s >= 10 * 60 && s->hr_avg > 0 && s->hr_avg < 90, now, &fresh);
  unlock(B_LUNCH, hour >= 11 && hour < 14, now, &fresh);
  unlock(B_WEEKEND, weekend, now, &fresh);
  unlock(B_TWICE, s_totals.day_sessions >= 2, now, &fresh);
  unlock(B_TRIPLE, s_totals.day_sessions >= 3, now, &fresh);
  unlock(B_STREAK14, s_totals.streak >= 14, now, &fresh);
  unlock(B_STREAK30, s_totals.streak >= 30, now, &fresh);
  unlock(B_COMEBACK, comeback, now, &fresh);
  unlock(B_NEWYEAR, tm.tm_mon == 0 && tm.tm_mday == 1, now, &fresh);
  unlock(B_VALENTINE, tm.tm_mon == 1 && tm.tm_mday == 14, now, &fresh);
  unlock(B_VARIETY, (s_totals.modes & all_styles) == all_styles, now, &fresh);
  unlock(B_AUTO, s->flags & SESSION_AUTO, now, &fresh);
  unlock(B_LIGHT, s->flags & SESSION_PULSE_LIGHT, now, &fresh);
  return fresh;
}

// Erfolge, die erst später feststehen (Schlaf, Stimmung) oder nicht an Sessions hängen
static uint64_t check_state(void) {
  uint64_t fresh = 0;
  time_t now = time(NULL);
  bool dreams = false, deep = false, morning = false, checkin = false;
  int better_nights = 0, sunny = 0;
  for (int i = 0; i < storage_count(); i++) {
    Session *s = storage_get(i);
    if (s->sleep_state == SLEEP_DONE) {
      dreams |= s->sleep_latency_min <= 15;
      deep |= s->sleep_delta_min >= 30;
      better_nights += s->sleep_delta_min > 0;
    }
    if (s->day_state != DAY_NOT_MORNING && s->mood) {
      checkin = true;
      morning |= s->mood == 5;
      sunny += s->mood == 5;
    }
  }
  bool coach = pos_trained_count() > 0;
  for (int c = 0; c < CALIB_COUNT; c++) coach |= storage_calib(c)->valid;

  unlock(B_DREAMS, dreams, now, &fresh);
  unlock(B_DEEP, deep, now, &fresh);
  unlock(B_MORNING, morning, now, &fresh);
  unlock(B_COACH, coach, now, &fresh);
  unlock(B_POS5, pos_trained_count() >= 5, now, &fresh);
  unlock(B_POS8, pos_trained_count() >= POS_COUNT, now, &fresh);
  unlock(B_DREAMWEEK, better_nights >= 7, now, &fresh);
  unlock(B_SUNSHINE, sunny >= 5, now, &fresh);
  unlock(B_CHECKIN, checkin, now, &fresh);
  unlock(B_ANALYST, s_totals.stats_seen, now, &fresh);
  return fresh;
}

uint64_t badges_stats_opened(void) {
  s_totals.stats_seen = 1;
  uint64_t fresh = check_state();
  persist_write_data(KEY_TOTALS, &s_totals, sizeof(s_totals));
  if (fresh) persist_write_data(KEY_BADGES, &s_badges, sizeof(s_badges));
  return fresh;
}

void badges_init(void) {
  if (persist_exists(KEY_BADGES)) persist_read_data(KEY_BADGES, &s_badges, sizeof(s_badges));
  if (persist_exists(KEY_TOTALS)) {
    persist_read_data(KEY_TOTALS, &s_totals, sizeof(s_totals));
    return;
  }
  // Erster Start mit Erfolgen: vorhandenen Verlauf still nachrechnen
  s_totals.last_day = -10;
  for (int i = storage_count() - 1; i >= 0; i--) apply_session(storage_get(i));
  check_state();
  save();
}

uint64_t badges_on_session(const Session *s) {
  uint64_t fresh = apply_session(s) | check_state();
  save();
  return fresh;
}

uint64_t badges_check(void) {
  uint64_t fresh = check_state();
  if (fresh) save();
  return fresh;
}

// --- Medaille zeichnen ---------------------------------------------------

static void draw_medal(GContext *g, GPoint c, int r, bool unlocked) {
  GColor ribbon = unlocked ? PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack)
                           : PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack);
  GColor fill = unlocked ? PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorBlack)
                         : PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite);
  GColor ring = unlocked ? PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorWhite)
                         : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
  // Band
  graphics_context_set_fill_color(g, ribbon);
  int w = r * 2 / 3;
  graphics_fill_rect(g, GRect(c.x - w, c.y - r - r / 2, w - 1, r), 0, GCornerNone);
  graphics_fill_rect(g, GRect(c.x + 1, c.y - r - r / 2, w - 1, r), 0, GCornerNone);
  // Medaille
  graphics_context_set_fill_color(g, fill);
  graphics_fill_circle(g, c, r);
  graphics_context_set_stroke_color(g, ring);
  graphics_context_set_stroke_width(g, r > 20 ? 3 : 1);
  graphics_draw_circle(g, c, r);
  graphics_draw_circle(g, c, r * 2 / 3);
}

// --- Anzeige einzelner Erfolge (neu freigeschaltet oder aus der Liste) ----

static Window *s_pop_window;
static Layer *s_pop_layer;
static uint64_t s_pop_queue;  // Bitmaske der noch anzuzeigenden Erfolge
static int s_pop_badge;
static bool s_pop_fresh;      // true = gerade freigeschaltet

static int next_in_queue(void) {
  for (int b = 0; b < BADGE_COUNT; b++) {
    if (s_pop_queue & (1ull << b)) return b;
  }
  return -1;
}

static void pop_update(Layer *layer, GContext *g) {
  GRect b = layer_get_bounds(layer);
  bool big = b.size.h >= 200;
  int b_id = s_pop_badge;
  bool unlocked = has(b_id);

  GRect head = GRect(4, big ? 6 : 2, b.size.w - 8, 26);
  graphics_context_set_text_color(g, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack));
  graphics_draw_text(g, s_pop_fresh ? tr(S_BADGE_UNLOCKED) : tr(S_MENU_BADGES),
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), head,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int r = big ? 30 : 22;
  int cy = big ? 84 : 64;
  draw_medal(g, GPoint(b.size.w / 2, cy), r, unlocked);

  graphics_context_set_text_color(g, GColorBlack);
  int y = cy + r + 6;
  graphics_draw_text(g, name(b_id), fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(4, y, b.size.w - 8, 30), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  graphics_draw_text(g, description(b_id), fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(8, y + 28, b.size.w - 16, 44), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  char foot[48];
  if (unlocked) {
    char date[32];
    fmt_date(date, sizeof(date), s_badges.when[b_id]);
    snprintf(foot, sizeof(foot), tr(S_BADGE_DATE_FMT), date);
  } else {
    snprintf(foot, sizeof(foot), "%s", tr(S_BADGE_LOCKED));
  }
  if (s_pop_fresh && next_in_queue() >= 0) snprintf(foot, sizeof(foot), "%s", tr(S_BADGE_NEXT));
  graphics_context_set_text_color(g, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  graphics_draw_text(g, foot, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(4, b.size.h - 22, b.size.w - 8, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void pop_select(ClickRecognizerRef rec, void *ctx) {
  int next = next_in_queue();
  if (s_pop_fresh && next >= 0) {
    s_pop_badge = next;
    s_pop_queue &= ~(1ull << next);
    vibes_short_pulse();
    layer_mark_dirty(s_pop_layer);
  } else {
    window_stack_pop(true);
  }
}

static void pop_click_config(void *ctx) { window_single_click_subscribe(BUTTON_ID_SELECT, pop_select); }

#if defined(PBL_TOUCH)
static void pop_tap(const Recognizer *r, RecognizerEvent e) {
  if (e == RecognizerEvent_Completed) pop_select(NULL, NULL);
}
#endif

static void pop_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_pop_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_pop_layer, pop_update);
  layer_add_child(root, s_pop_layer);
#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, tap_recognizer_create(pop_tap, NULL));
#endif
}

static void pop_unload(Window *window) {
  layer_destroy(s_pop_layer);
  window_destroy(s_pop_window);
  s_pop_window = NULL;
}

static void pop_push(int badge, bool fresh) {
  if (s_pop_window) return;
  s_pop_badge = badge;
  s_pop_fresh = fresh;
  s_pop_window = window_create();
  window_set_click_config_provider(s_pop_window, pop_click_config);
  window_set_window_handlers(s_pop_window, (WindowHandlers){
                                               .load = pop_load,
                                               .unload = pop_unload,
                                           });
  window_stack_push(s_pop_window, true);
}

static void pin_badges(uint64_t fresh) {
  if (storage_profile()->timeline == TIMELINE_OFF) return;
  for (int b = 0; b < BADGE_COUNT; b++) {
    if (!(fresh & (1ull << b))) continue;
    char id[24];
    snprintf(id, sizeof(id), "stamina-b-%d", b);
    settings_queue_pin(id, s_badges.when[b], name(b), description(b));
  }
}

void badges_announce(uint64_t fresh) {
  pin_badges(fresh);
  if (!fresh || s_pop_window) return;
  s_pop_queue = fresh;
  int first = next_in_queue();
  s_pop_queue &= ~(1ull << first);
  vibes_double_pulse();
  pop_push(first, true);
}

// --- Liste aller Erfolge -------------------------------------------------
// Freigeschaltete zuerst, danach die offenen

static Window *s_window;
static MenuLayer *s_menu;
static int s_order[BADGE_COUNT];

static void build_order(void) {
  int n = 0;
  for (int pass = 0; pass < 2; pass++) {
    for (int b = 0; b < BADGE_COUNT; b++) {
      if (has(b) == (pass == 0)) s_order[n++] = b;
    }
  }
}

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *ctx) { return BADGE_COUNT; }

static int16_t row_height(MenuLayer *m, MenuIndex *index, void *ctx) { return 48; }

static int16_t header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void draw_header(GContext *g, const Layer *cell, uint16_t section, void *ctx) {
  char buf[48];
  snprintf(buf, sizeof(buf), tr(S_SUB_BADGES_FMT), badges_unlocked_count(), BADGE_COUNT);
  menu_cell_basic_header_draw(g, cell, buf);
}

static void menu_draw(GContext *g, const Layer *cell, MenuIndex *index, void *ctx) {
  int b = s_order[index->row];
  GRect bounds = layer_get_bounds(cell);
  bool unlocked = has(b);
  bool hl = menu_cell_layer_is_highlighted(cell);

  draw_medal(g, GPoint(22, bounds.size.h / 2 + 4), 12, unlocked);

  GColor text = hl ? GColorWhite : (unlocked ? GColorBlack : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  graphics_context_set_text_color(g, text);
  int x = 44;
  graphics_draw_text(g, name(b), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(x, 0, bounds.size.w - x - 4, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(g, description(b), fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(x, 22, bounds.size.w - x - 4, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void menu_select(MenuLayer *m, MenuIndex *index, void *ctx) {
  pop_push(s_order[index->row], false);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
                                             .get_num_rows = menu_rows,
                                             .get_cell_height = row_height,
                                             .get_header_height = header_height,
                                             .draw_header = draw_header,
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
  build_order();
  menu_layer_reload_data(s_menu);
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void badges_window_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}
