#include "app.h"

// Clay schickt Auswahlfelder als String, Slider/Toggles als Zahl
static int32_t tuple_int(const Tuple *t) {
  if (t->type == TUPLE_CSTRING) {
    int32_t v = 0;
    for (const char *c = t->value->cstring; *c >= '0' && *c <= '9'; c++) v = v * 10 + (*c - '0');
    return v;
  }
  bool is_signed = t->type == TUPLE_INT;
  switch (t->length) {
    case 1: return is_signed ? t->value->int8 : t->value->uint8;
    case 2: return is_signed ? t->value->int16 : t->value->uint16;
    default: return t->value->int32;
  }
}

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void settings_send_profile(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  Profile *p = storage_profile();
  dict_write_int32(out, MESSAGE_KEY_SEX, p->sex);
  dict_write_int32(out, MESSAGE_KEY_AGE, p->age);
  dict_write_int32(out, MESSAGE_KEY_WEIGHT, p->weight_kg);
  dict_write_int32(out, MESSAGE_KEY_SENSITIVITY, p->sensitivity);
  dict_write_int32(out, MESSAGE_KEY_TOUCH_SESSION, p->touch_session);
  dict_write_int32(out, MESSAGE_KEY_SOLO_STYLE, p->solo_style);
  dict_write_int32(out, MESSAGE_KEY_LANGUAGE, p->lang);
  dict_write_int32(out, MESSAGE_KEY_CHECKIN, p->checkin);
  app_message_outbox_send();
}

static void inbox_received(DictionaryIterator *it, void *context) {
  if (dict_find(it, MESSAGE_KEY_REQUEST_PROFILE)) {
    settings_send_profile();
    return;
  }

  Profile *p = storage_profile();
  bool changed = false;
  Tuple *t;
  if ((t = dict_find(it, MESSAGE_KEY_SEX))) {
    p->sex = clamp(tuple_int(t), SEX_MALE, SEX_OTHER);
    changed = true;
  }
  if ((t = dict_find(it, MESSAGE_KEY_AGE))) {
    p->age = clamp(tuple_int(t), 16, 99);
    changed = true;
  }
  if ((t = dict_find(it, MESSAGE_KEY_WEIGHT))) {
    p->weight_kg = clamp(tuple_int(t), 35, 250);
    changed = true;
  }
  if ((t = dict_find(it, MESSAGE_KEY_SENSITIVITY))) {
    p->sensitivity = clamp(tuple_int(t), SENS_LOW, SENS_HIGH);
    changed = true;
  }
  if ((t = dict_find(it, MESSAGE_KEY_TOUCH_SESSION))) {
    p->touch_session = tuple_int(t) ? 1 : 0;
    changed = true;
  }

  if ((t = dict_find(it, MESSAGE_KEY_SOLO_STYLE))) {
    p->solo_style = clamp(tuple_int(t), STYLE_AUTO, STYLE_TOY);
    changed = true;
  }
  if ((t = dict_find(it, MESSAGE_KEY_CHECKIN))) {
    p->checkin = tuple_int(t) ? 1 : 0;
    changed = true;
  }
  if ((t = dict_find(it, MESSAGE_KEY_LANGUAGE))) {
    p->lang = clamp(tuple_int(t), 0, LANG_COUNT);
    changed = true;
    i18n_load();
  }

  if (changed) {
    storage_save_profile();
    vibes_short_pulse();
  }
}

void settings_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 128);
}
