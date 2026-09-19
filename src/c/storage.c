#include "app.h"

#define STORAGE_VERSION 1

_Static_assert(sizeof(Session) <= PERSIST_DATA_MAX_LENGTH, "Session too large for persist");

// Chronologisch: [0] = ältester Eintrag
static Session s_sessions[MAX_SESSIONS];
static int s_count;
static Profile s_profile;
static Calib s_calib[CALIB_COUNT];

static void write_from(int from) {
  for (int i = from; i < s_count; i++) {
    persist_write_data(KEY_SESSION_BASE + i, &s_sessions[i], sizeof(Session));
  }
  persist_write_int(KEY_COUNT, s_count);
}

void storage_init(void) {
  if (persist_exists(KEY_VERSION) && persist_read_int(KEY_VERSION) == STORAGE_VERSION) {
    s_count = persist_exists(KEY_COUNT) ? persist_read_int(KEY_COUNT) : 0;
    if (s_count < 0 || s_count > MAX_SESSIONS) s_count = 0;
    for (int i = 0; i < s_count; i++) {
      persist_read_data(KEY_SESSION_BASE + i, &s_sessions[i], sizeof(Session));
    }
  } else {
    s_count = 0;
    persist_write_int(KEY_VERSION, STORAGE_VERSION);
    persist_write_int(KEY_COUNT, 0);
  }

  // Defaults zuerst, damit Felder fehlen dürfen, die ältere Versionen noch nicht gespeichert haben
  s_profile = (Profile){
      .sex = SEX_MALE, .age = 30, .weight_kg = 80, .sensitivity = SENS_NORMAL, .touch_session = 1,
      .solo_style = STYLE_AUTO, .lang = 0, .checkin = 1, .auto_start = 0,
      .ask_partner = 1};
  if (persist_exists(KEY_PROFILE)) {
    persist_read_data(KEY_PROFILE, &s_profile, sizeof(Profile));
  }
  if (persist_exists(KEY_CALIB)) {
    persist_read_data(KEY_CALIB, s_calib, sizeof(s_calib));
  }
}

int storage_count(void) { return s_count; }

Session *storage_get(int idx) {
  if (idx < 0 || idx >= s_count) return NULL;
  return &s_sessions[s_count - 1 - idx];
}

int storage_find(uint32_t start) {
  for (int i = 0; i < s_count; i++) {
    if (storage_get(i)->start == start) return i;
  }
  return -1;
}

void storage_add(const Session *s) {
  if (s_count == MAX_SESSIONS) {
    memmove(&s_sessions[0], &s_sessions[1], sizeof(Session) * (MAX_SESSIONS - 1));
    s_sessions[MAX_SESSIONS - 1] = *s;
    write_from(0);
  } else {
    s_sessions[s_count++] = *s;
    write_from(s_count - 1);
  }
}

void storage_save(int idx) {
  if (idx < 0 || idx >= s_count) return;
  int i = s_count - 1 - idx;
  persist_write_data(KEY_SESSION_BASE + i, &s_sessions[i], sizeof(Session));
}

void storage_delete(int idx) {
  if (idx < 0 || idx >= s_count) return;
  int i = s_count - 1 - idx;
  memmove(&s_sessions[i], &s_sessions[i + 1], sizeof(Session) * (s_count - 1 - i));
  s_count--;
  persist_delete(KEY_SESSION_BASE + s_count);
  write_from(i);
}

void storage_delete_all(void) {
  for (int i = 0; i < s_count; i++) persist_delete(KEY_SESSION_BASE + i);
  s_count = 0;
  persist_write_int(KEY_COUNT, 0);
}

Profile *storage_profile(void) { return &s_profile; }

void storage_save_profile(void) { persist_write_data(KEY_PROFILE, &s_profile, sizeof(Profile)); }

Calib *storage_calib(int which) { return &s_calib[which]; }

void storage_save_calib(void) { persist_write_data(KEY_CALIB, s_calib, sizeof(s_calib)); }
