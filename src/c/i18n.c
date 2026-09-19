#include "app.h"

// Texte liegen je Sprache als Ressource vor (erzeugt aus i18n/*.json),
// geladen wird nur die aktive Sprache.

static const char *const LANG_CODE[LANG_COUNT] = LANG_CODES;
static const uint32_t LANG_RES[LANG_COUNT] = LANG_RESOURCE_IDS;

// Eigennamen, damit man die Sprache auch in einer fremden Oberfläche wiederfindet.
// Nicht-lateinische Schriften stehen in lateinischer Umschrift, weil die Systemschrift
// sie ohne Sprachpaket nicht darstellen kann.
static const char *const LANG_NAME[LANG_COUNT] = {
    "English", "Deutsch", "Français", "Español", "Italiano", "Português", "Nederlands",
    "Polski", "Čeština", "Svenska", "Dansk", "Norsk", "Suomi", "Russkij (RU)",
    "Ukrajinska (UK)", "Türkçe", "Nihongo (JA)", "Zhongwen (ZH)", "Hangugeo (KO)"};

// Brauchen Glyphen aus einem Sprachpaket der Uhr
static bool needs_language_pack(int lang) {
  const char *c = LANG_CODE[lang];
  return !strcmp(c, "ru") || !strcmp(c, "uk") || !strcmp(c, "ja") || !strcmp(c, "zh") ||
         !strcmp(c, "ko");
}

static const char *s_str[STR_COUNT];
static char *s_buf;
static char *s_prev;  // vorheriger Puffer bleibt eine Runde bestehen, falls noch ein Layer darauf zeigt
static int s_lang = -1;

const char *lang_name(int index) {
  return (index >= 0 && index < LANG_COUNT) ? LANG_NAME[index] : "";
}

static int detect_system_language(void) {
  const char *loc = i18n_get_system_locale();
  if (!loc) return 0;
  char code[3] = {loc[0], loc[0] ? loc[1] : 0, 0};
  if (strcmp(code, "no") == 0 || strcmp(code, "nn") == 0) strcpy(code, "nb");
  for (int i = 0; i < LANG_COUNT; i++) {
    if (strcmp(code, LANG_CODE[i]) == 0) return i;
  }
  return 0;
}

static bool load_language(int lang) {
  ResHandle h = resource_get_handle(LANG_RES[lang]);
  size_t size = resource_size(h);
  char *buf = malloc(size + 1);
  if (!buf) return false;
  resource_load(h, (uint8_t *)buf, size);
  buf[size] = '\0';

  const char *tmp[STR_COUNT];
  int n = 0;
  for (char *c = buf; n < STR_COUNT && c < buf + size; c += strlen(c) + 1) tmp[n++] = c;
  if (n != STR_COUNT) {
    free(buf);
    return false;
  }

  memcpy(s_str, tmp, sizeof(s_str));
  free(s_prev);
  s_prev = s_buf;
  s_buf = buf;
  s_lang = lang;
  return true;
}

void i18n_load(void) {
  uint8_t setting = storage_profile()->lang;
  int system = detect_system_language();
  int lang = (setting >= 1 && setting <= LANG_COUNT) ? setting - 1 : system;
  // Ohne passendes Sprachpaket (= Uhr läuft nicht in dieser Sprache) gäbe es nur Kästchen
  if (needs_language_pack(lang) && lang != system) lang = 0;
  if (lang == s_lang) return;
  if (!load_language(lang) && s_lang < 0) load_language(0);
}

const char *tr(StrId id) { return (id < STR_COUNT && s_str[id]) ? s_str[id] : ""; }
