#include <pebble_worker.h>
#include "../../src/shared/shared.h"

// Hintergrund-Worker für den automatischen Session-Start.
// Läuft nur, wenn "Auto-Start" eingeschaltet ist. Er wertet einmal pro Sekunde
// die Beschleunigung mit derselben Erkennung wie die App aus. Hält rhythmische
// Bewegung lange genug an (und man geht/läuft nicht), fragt die App nach.

#define SAMPLE_HZ 25
#define MAX_GAP_S 10            // so lange darf die Bewegung am Stück aussetzen
#define MIN_RHYTHMIC_PCT 70     // Anteil rhythmischer Sekunden im Lauf
#define HR_CHECK_AFTER_S 30     // ab hier Puls sekündlich messen
#define HR_ELEVATED_BPM 10      // über dem Üblichen zu dieser Tageszeit
#define COOLDOWN_S (30 * 60)    // nach einer Nachfrage
#define AFTER_SESSION_S (10 * 60)

static Profile s_profile;
static Calib s_calib[CALIB_COUNT];
static Detector s_det;
static SessionMode s_mode;
static bool s_enabled;
static bool s_subscribed;
static bool s_session_active;
static time_t s_cooldown_until;

// aktueller Lauf rhythmischer Bewegung
static time_t s_run_start;
static uint16_t s_run_secs, s_run_rhythmic, s_gap;
static uint32_t s_run_cycles;
static bool s_hr_fast;

static void reset_run(void) {
  s_run_start = 0;
  s_run_secs = s_run_rhythmic = s_gap = 0;
  s_run_cycles = 0;
  if (s_hr_fast) {
    health_service_set_heart_rate_sample_period(0);
    s_hr_fast = false;
  }
}

static bool walking_or_running(void) {
  return health_service_peek_current_activities() & (HealthActivityWalk | HealthActivityRun);
}

static bool hr_elevated(time_t now) {
  HealthValue hr = health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (hr <= 0) return false;
  HealthValue usual = health_service_aggregate_averaged(
      HealthMetricHeartRateBPM, now - 15 * 60, now, HealthAggregationAvg,
      HealthServiceTimeScopeDailyWeekdayOrWeekend);
  return usual > 0 && hr >= usual + HR_ELEVATED_BPM;
}

static void trigger(time_t now) {
  AutoDetect a = {
      .start = s_run_start,
      .cycles = s_run_cycles > UINT16_MAX ? UINT16_MAX : s_run_cycles,
      .mode = s_mode,
  };
  persist_write_data(KEY_AUTO, &a, sizeof(a));
  s_cooldown_until = now + COOLDOWN_S;
  reset_run();

  // Ist die App offen, zeigt sie die Nachfrage sofort; sonst wird sie gestartet
  AppWorkerMessage msg = {0};
  app_worker_send_message(WMSG_DETECTED, &msg);
  worker_launch_app();
}

static void per_second(time_t now, int cycles, bool moving) {
  if (walking_or_running()) {
    reset_run();
    return;
  }

  int max_per_s = s_mode == MODE_SOLO_RUB ? 8 : 5;
  bool rhythmic = moving && cycles >= 1 && cycles <= max_per_s;
  if (!s_run_start) {
    if (!rhythmic) return;
    s_run_start = now;
  }

  s_run_secs++;
  s_run_cycles += cycles;
  if (rhythmic) {
    s_run_rhythmic++;
    s_gap = 0;
  } else if (++s_gap > MAX_GAP_S) {
    reset_run();
    return;
  }

  if (s_run_secs == HR_CHECK_AFTER_S && !s_hr_fast) {
    s_hr_fast = health_service_set_heart_rate_sample_period(1);
  }

  // Erhöhter Puls bestätigt die Vermutung: dann schon nach der halben Zeit fragen
  int needed = s_profile.auto_start * 60;
  if (s_run_secs > HR_CHECK_AFTER_S + 10 && hr_elevated(now)) needed /= 2;

  if (s_run_secs >= needed && s_run_rhythmic * 100 >= s_run_secs * MIN_RHYTHMIC_PCT) {
    trigger(now);
  }
}

static void accel_handler(AccelData *data, uint32_t num) {
  if (!s_enabled || s_session_active) return;
  time_t now = time(NULL);
  if (now < s_cooldown_until) return;

  int cycles = 0;
  s_det.moving = 0;
  for (uint32_t i = 0; i < num; i++) {
    if (data[i].did_vibrate) continue;
    if (detector_process(&s_det, data[i].x, data[i].y, data[i].z)) cycles++;
  }
  per_second(now, cycles, s_det.moving > SAMPLE_HZ / 3);
}

// Beschleunigung nur abonnieren, wenn tatsächlich erkannt werden soll (Akku)
static void update_subscription(void) {
  bool want = s_enabled && !s_session_active;
  if (want && !s_subscribed) {
    accel_data_service_subscribe(SAMPLE_HZ, accel_handler);  // ein Aufruf pro Sekunde
    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  } else if (!want && s_subscribed) {
    accel_data_service_unsubscribe();
  }
  s_subscribed = want;
}

static void load_settings(void) {
  s_profile = (Profile){.sex = SEX_MALE, .sensitivity = SENS_NORMAL, .solo_style = STYLE_AUTO};
  if (persist_exists(KEY_PROFILE)) persist_read_data(KEY_PROFILE, &s_profile, sizeof(s_profile));
  if (persist_exists(KEY_CALIB)) persist_read_data(KEY_CALIB, s_calib, sizeof(s_calib));

  s_mode = solo_mode_for(&s_profile);
  // Mit Toy bewegt sich die Hand kaum: nichts zu erkennen
  if (s_mode == MODE_SOLO_TOY) s_mode = MODE_SOLO_RUB;
  s_enabled = s_profile.auto_start > 0;

  DetectorParams params;
  int ci = calib_index(s_mode);
  detector_params(s_mode, ci >= 0 ? &s_calib[ci] : NULL, s_profile.sensitivity, &params);
  detector_init(&s_det, &params);
  reset_run();
  update_subscription();
}

static void app_message(uint16_t type, AppWorkerMessage *data) {
  switch (type) {
    case WMSG_SESSION_ACTIVE:
      s_session_active = true;
      reset_run();
      break;
    case WMSG_SESSION_IDLE:
      s_session_active = false;
      s_cooldown_until = time(NULL) + AFTER_SESSION_S;
      break;
    case WMSG_RELOAD:
      load_settings();
      break;
  }
  update_subscription();
}

int main(void) {
  app_worker_message_subscribe(app_message);
  load_settings();
  worker_event_loop();
  accel_data_service_unsubscribe();
  reset_run();
}
