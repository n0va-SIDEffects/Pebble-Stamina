#include "app.h"

// Diagramme: Sessions je Tag / Woche / Monat, Solo und Partner gestapelt.
// Der Verlauf hält nur MAX_SESSIONS Einträge, deshalb werden die Zähler hier
// separat als Ringpuffer geführt (letztes Element = aktueller Tag/Woche/Monat).

#define DAYS 14
#define WEEKS 12
#define MONTHS 12

typedef struct {
  int32_t day, week, month;   // Kennung des jeweils letzten Eintrags
  uint8_t d[DAYS][2];         // [solo, partner]
  uint8_t w[WEEKS][2];
  uint8_t m[MONTHS][2];
} Activity;

static Activity s_act;

typedef enum { VIEW_DAYS, VIEW_WEEKS, VIEW_MONTHS, VIEW_COUNT } ChartView;

// Wochen beginnen montags: Tag 0 der Unix-Zeit war ein Donnerstag
static int32_t week_of(int32_t day) { return (day + 3) / 7; }

static int32_t month_of(time_t t) {
  struct tm *tm = localtime(&t);
  return (tm->tm_year + 1900) * 12 + tm->tm_mon;
}

// Ringpuffer bis zur Kennung target weiterschieben, Lücken mit 0 füllen
static void shift(uint8_t (*buf)[2], int n, int32_t *anchor, int32_t target) {
  int32_t diff = target - *anchor;
  if (diff <= 0) return;
  if (diff >= n) {
    memset(buf, 0, n * 2);
  } else {
    memmove(buf, buf + diff, (n - diff) * 2);
    memset(buf + n - diff, 0, diff * 2);
  }
  *anchor = target;
}

static void add(uint8_t (*buf)[2], int n, int32_t *anchor, int32_t id, int kind) {
  shift(buf, n, anchor, id);
  int32_t back = *anchor - id;
  if (back < 0 || back >= n) return;  // älter als der Puffer
  uint8_t *v = &buf[n - 1 - back][kind];
  if (*v < 255) (*v)++;
}

static void add_session(const Session *s) {
  int kind = s->mode == MODE_PARTNER ? 1 : 0;
  int32_t day = local_day_index(s->start);
  add(s_act.d, DAYS, &s_act.day, day, kind);
  add(s_act.w, WEEKS, &s_act.week, week_of(day), kind);
  add(s_act.m, MONTHS, &s_act.month, month_of(s->start), kind);
}

static void save(void) { persist_write_data(KEY_ACTIVITY, &s_act, sizeof(s_act)); }

void charts_init(void) {
  if (persist_exists(KEY_ACTIVITY)) {
    persist_read_data(KEY_ACTIVITY, &s_act, sizeof(s_act));
    return;
  }
  // Erster Start: vorhandenen Verlauf übernehmen (älteste zuerst)
  for (int i = storage_count() - 1; i >= 0; i--) add_session(storage_get(i));
  save();
}

void charts_add_session(const Session *s) {
  add_session(s);
  save();
}

// Vor dem Anzeigen bis heute weiterschieben, damit leere Tage als 0 erscheinen
static void advance_to_now(void) {
  time_t now = time(NULL);
  int32_t day = local_day_index(now);
  shift(s_act.d, DAYS, &s_act.day, day);
  shift(s_act.w, WEEKS, &s_act.week, week_of(day));
  shift(s_act.m, MONTHS, &s_act.month, month_of(now));
}

// --- Anzeige -------------------------------------------------------------

static Window *s_window;
static Layer *s_layer;
static ChartView s_view;

static GColor solo_color(void) { return PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack); }
static GColor partner_color(void) { return PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack); }

static void view_data(uint8_t (**buf)[2], int *n) {
  switch (s_view) {
    case VIEW_WEEKS: *buf = s_act.w; *n = WEEKS; break;
    case VIEW_MONTHS: *buf = s_act.m; *n = MONTHS; break;
    default: *buf = s_act.d; *n = DAYS; break;
  }
}

// Beschriftung unter Balken i (Zahlen statt Wörter, damit es in jeder Sprache passt)
static void bar_label(int i, int n, char *out, size_t size) {
  time_t now = time(NULL);
  int back = n - 1 - i;
  if (s_view == VIEW_MONTHS) {
    int32_t m = s_act.month - back;
    snprintf(out, size, "%u", (unsigned)(uint8_t)(m % 12 + 1));
    return;
  }
  // Tage: Datum des Tages; Wochen: Datum des Montags
  time_t t = now - back * (s_view == VIEW_WEEKS ? 7 : 1) * SECONDS_PER_DAY;
  if (s_view == VIEW_WEEKS) {
    struct tm *tm = localtime(&t);
    t -= ((tm->tm_wday + 6) % 7) * SECONDS_PER_DAY;
  }
  struct tm *tm = localtime(&t);
  uint8_t mday = tm->tm_mday, mon = tm->tm_mon + 1;
  if (s_view == VIEW_DAYS) {
    snprintf(out, size, "%u", (unsigned)mday);
  } else {
    snprintf(out, size, "%u.%u", (unsigned)mday, (unsigned)mon);
  }
}

static void draw_legend_box(GContext *g, GRect r, GColor c, bool hatched) {
  graphics_context_set_fill_color(g, c);
  graphics_context_set_stroke_color(g, c);
  if (hatched) {
    graphics_draw_rect(g, r);
  } else {
    graphics_fill_rect(g, r, 0, GCornerNone);
  }
}

static void update_proc(Layer *layer, GContext *g) {
  GRect b = layer_get_bounds(layer);
  bool big = b.size.h >= 200;
  uint8_t (*buf)[2];
  int n;
  view_data(&buf, &n);

  // Titel
  const StrId titles[] = {S_CHART_DAYS, S_CHART_WEEKS, S_CHART_MONTHS};
  graphics_context_set_text_color(g, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack));
  graphics_draw_text(g, tr(titles[s_view]), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, 0, b.size.w - 8, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  int max = 1, solo = 0, partner = 0;
  for (int i = 0; i < n; i++) {
    if (buf[i][0] + buf[i][1] > max) max = buf[i][0] + buf[i][1];
    solo += buf[i][0];
    partner += buf[i][1];
  }

  int left = 18, right = b.size.w - 6;
  int top = 30, bottom = b.size.h - (big ? 64 : 52);
  int height = bottom - top;
  int slot = (right - left) / n;
  int bar = slot - 2 > 3 ? slot - 2 : slot;

  // Hilfslinien: Maximum und Hälfte
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_stroke_color(g, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  graphics_context_set_text_color(g, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  char num[8];
  snprintf(num, sizeof(num), "%d", max);
  graphics_draw_text(g, num, small, GRect(0, top - 9, left - 2, 16), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);
  for (int x = left; x < right; x += 4) graphics_draw_pixel(g, GPoint(x, top));

  // Balken: Solo unten, Partner darauf
  for (int i = 0; i < n; i++) {
    int x = left + i * slot + (slot - bar) / 2;
    int hs = buf[i][0] * height / max;
    int hp = buf[i][1] * height / max;
    if (hs) {
      graphics_context_set_fill_color(g, solo_color());
      graphics_fill_rect(g, GRect(x, bottom - hs, bar, hs), 0, GCornerNone);
    }
    if (hp) {
      GRect r = GRect(x, bottom - hs - hp, bar, hp);
#if defined(PBL_COLOR)
      graphics_context_set_fill_color(g, partner_color());
      graphics_fill_rect(g, r, 0, GCornerNone);
#else
      graphics_context_set_stroke_color(g, GColorBlack);
      graphics_draw_rect(g, r);  // Schwarz-Weiß: Partner als Umriss
#endif
    }
  }
  graphics_context_set_stroke_color(g, GColorBlack);
  graphics_draw_line(g, GPoint(left, bottom), GPoint(right, bottom));

  // Beschriftung: erster, mittlerer, letzter Balken
  char label[12];
  int marks[] = {0, n / 2, n - 1};
  for (int k = 0; k < 3; k++) {
    int i = marks[k];
    bar_label(i, n, label, sizeof(label));
    int cx = left + i * slot + slot / 2;
    graphics_draw_text(g, label, small, GRect(cx - 20, bottom + 1, 40, 16), GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
  }

  // Legende mit Summen
  int ly = bottom + 20;
  char sums[40];
  draw_legend_box(g, GRect(8, ly + 5, 10, 10), solo_color(), false);
  snprintf(sums, sizeof(sums), "%s %d", tr(S_CHART_SOLO), solo);
  graphics_context_set_text_color(g, GColorBlack);
  graphics_draw_text(g, sums, small, GRect(22, ly, b.size.w / 2 - 22, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  draw_legend_box(g, GRect(b.size.w / 2, ly + 5, 10, 10), partner_color(), !PBL_IF_COLOR_ELSE(1, 0));
  snprintf(sums, sizeof(sums), "%s %d", tr(S_STYLE_PARTNER), partner);
  graphics_draw_text(g, sums, small, GRect(b.size.w / 2 + 14, ly, b.size.w / 2 - 16, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (big) {
    graphics_context_set_text_color(g, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_draw_text(g, tr(S_CHART_HINT), small, GRect(4, b.size.h - 20, b.size.w - 8, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void switch_view(int delta) {
  s_view = (s_view + delta + VIEW_COUNT) % VIEW_COUNT;
  layer_mark_dirty(s_layer);
}

static void up_click(ClickRecognizerRef rec, void *ctx) { switch_view(-1); }
static void down_click(ClickRecognizerRef rec, void *ctx) { switch_view(1); }

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, down_click);
}

#if defined(PBL_TOUCH)
static void swipe_handler(const Recognizer *r, RecognizerEvent e) {
  if (e != RecognizerEvent_Completed) return;
  SwipeDirection d = swipe_recognizer_get_direction(r);
  if (d == SwipeDirection_Left) switch_view(1);
  if (d == SwipeDirection_Right) switch_view(-1);
}
#endif

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(root, s_layer);
#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
  window_attach_recognizer(window, swipe_recognizer_create(swipe_handler, NULL,
                                                           SwipeDirection_Left | SwipeDirection_Right));
#endif
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void charts_window_push(void) {
  advance_to_now();
  s_view = VIEW_DAYS;
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}
