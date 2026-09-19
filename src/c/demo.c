#include "app.h"

// Beispieldaten für Store-Screenshots. Nur im Demo-Build aktiv:
//   STAMINA_DEMO=1 pebble build
// Befüllt eine leere App mit realistischen Sessions, Stellungen und Partnern.

#if defined(DEMO)

static time_t at(int days_ago, int hour, int minute) {
  return time_start_of_today() - days_ago * SECONDS_PER_DAY + hour * SECONDS_PER_HOUR +
         minute * 60;
}

static void add(Session s) {
  s.grav[2] = -980;
  storage_add(&s);
}

void demo_seed(void) {
  if (storage_count() > 0) return;

  partner_set_name(1, "JK");
  partner_set_name(2, "AB");

  // Fingerabdrücke passend zu den Emulator-Testdaten (posA/posB)
  const int16_t riding[POS_FEATURES] = {0, -150, -980, 100, 0, 0, 283, 120};
  const int16_t missionary[POS_FEATURES] = {80, 950, -150, 0, 0, 100, 212, 90};
  const int16_t spooning[POS_FEATURES] = {900, 0, -400, 5, 80, 15, 150, 70};
  pos_set_name(0, "Riding (top)");
  pos_set_template(0, riding, 3);
  pos_set_name(1, "Missionary (top)");
  pos_set_template(1, missionary, 2);
  pos_set_name(2, "Spooning");
  pos_set_template(2, spooning, 1);

  add((Session){.start = at(6, 22, 10), .duration_s = 11 * 60 + 5, .active_s = 600, .strokes = 842,
                .kcal_x10 = 452, .climax_s = 10 * 60 + 31, .hr_avg = 92, .hr_min = 71, .hr_max = 124,
                .spm_avg = 82, .spm_max = 126, .mode = MODE_SOLO, .sleep_state = SLEEP_DONE,
                .sleep_latency_min = 14, .sleep_total_min = 441, .sleep_deep_min = 101,
                .sleep_delta_min = 21});
  add((Session){.start = at(5, 23, 20), .duration_s = 24 * 60 + 40, .active_s = 1300, .strokes = 1502,
                .kcal_x10 = 1521, .climax_s = 23 * 60 + 5, .hr_avg = 104, .hr_min = 78, .hr_max = 146,
                .spm_avg = 71, .spm_max = 118, .mode = MODE_PARTNER, .partner = 1,
                .sleep_state = SLEEP_DONE, .sleep_latency_min = 9, .sleep_total_min = 468,
                .sleep_deep_min = 118, .sleep_delta_min = 48, .pos_pct = {41, 36, 12}});
  add((Session){.start = at(3, 7, 5), .duration_s = 13 * 60 + 12, .active_s = 700, .strokes = 961,
                .kcal_x10 = 533, .climax_s = 12 * 60 + 40, .hr_avg = 95, .hr_min = 74, .hr_max = 129,
                .spm_avg = 88, .spm_max = 134, .mode = MODE_SOLO, .sleep_state = SLEEP_NONE,
                .day_state = DAY_DONE, .mood = 4, .day_steps = 3480, .day_steps_pct = 14,
                .day_hr_delta = 3, .day_active_min = 46});
  add((Session){.start = at(2, 22, 45), .duration_s = 9 * 60 + 30, .active_s = 510, .strokes = 1402,
                .kcal_x10 = 315, .climax_s = 9 * 60 + 2, .hr_avg = 89, .hr_min = 70, .hr_max = 121,
                .spm_avg = 148, .spm_max = 210, .mode = MODE_SOLO_RUB, .sleep_state = SLEEP_DONE,
                .sleep_latency_min = 11, .sleep_total_min = 485, .sleep_deep_min = 126,
                .sleep_delta_min = 57});
  add((Session){.start = at(1, 7, 15), .duration_s = 21 * 60 + 40, .active_s = 1180,
                .strokes = 1310, .kcal_x10 = 1420, .climax_s = 20 * 60 + 10, .hr_avg = 108,
                .hr_min = 82, .hr_max = 151, .spm_avg = 76, .spm_max = 118, .mode = MODE_PARTNER,
                .partner = 1, .sleep_state = SLEEP_NONE, .day_state = DAY_DONE, .mood = 5,
                .day_steps = 4120, .day_steps_pct = 22, .day_hr_delta = 4, .day_active_min = 58,
                .pos_pct = {38, 34, 16}});
  add((Session){.start = at(1, 23, 5), .duration_s = 14 * 60 + 12, .active_s = 750, .strokes = 1034,
                .kcal_x10 = 584, .climax_s = 13 * 60 + 40, .hr_avg = 96, .hr_min = 74, .hr_max = 131,
                .spm_avg = 84, .spm_max = 132, .mode = MODE_SOLO, .sleep_state = SLEEP_DONE,
                .sleep_latency_min = 12, .sleep_total_min = 472, .sleep_deep_min = 118,
                .sleep_delta_min = 34});
  add((Session){.start = at(0, 0, 40), .duration_s = 18 * 60 + 20, .active_s = 990, .strokes = 1188,
                .kcal_x10 = 1236, .climax_s = 17 * 60 + 2, .hr_avg = 101, .hr_min = 79, .hr_max = 139,
                .spm_avg = 72, .spm_max = 112, .mode = MODE_PARTNER, .partner = 2,
                .sleep_state = SLEEP_DONE, .sleep_latency_min = 7, .sleep_total_min = 455,
                .sleep_deep_min = 131, .sleep_delta_min = 29, .pos_pct = {22, 51, 9}});
}

#else

void demo_seed(void) {}

#endif
