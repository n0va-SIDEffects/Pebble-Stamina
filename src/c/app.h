#pragma once
#include <pebble.h>
#include "../shared/shared.h"
#include "i18n.auto.h"

#define MAX_SESSIONS 30

#define ACCENT_COLOR PBL_IF_COLOR_ELSE(GColorFolly, GColorWhite)

typedef enum {
  SLEEP_PENDING = 0,     // Nacht noch nicht vorbei
  SLEEP_DONE = 1,        // ausgewertet
  SLEEP_NONE = 2,        // innerhalb von 6 h danach nicht eingeschlafen
  SLEEP_UNAVAILABLE = 3  // keine Health-Daten (Berechtigung / zu alt)
} SleepState;

typedef enum {
  DAY_NOT_MORNING = 0,   // Session nicht am Morgen: keine Tagesauswertung
  DAY_PENDING = 1,
  DAY_DONE = 2,
  DAY_UNAVAILABLE = 3
} DayState;

// Orientierung des Handgelenks (Schwerkraftachse)
enum { ORIENT_XP, ORIENT_XN, ORIENT_YP, ORIENT_YN, ORIENT_ZP, ORIENT_ZN, ORIENT_COUNT };

#define POS_COUNT 8  // Plätze für angelernte Stellungen
#define PARTNER_COUNT 8
#define PARTNER_LEN 5  // Kürzel bis 4 Zeichen + NUL

typedef struct __attribute__((packed)) {
  uint32_t start;            // Unix-Zeit
  uint16_t duration_s;       // Gesamtdauer ohne Pausen
  uint16_t active_s;         // Sekunden mit rhythmischer Bewegung
  uint16_t strokes;          // erkannte Bewegungszyklen
  uint16_t kcal_x10;         // geschätzte kcal * 10
  uint16_t climax_s;         // Sekunden bis Höhepunkt, 0 = nicht markiert
  uint8_t hr_avg, hr_min, hr_max;
  uint8_t spm_avg, spm_max;  // Rhythmus (Zyklen pro Minute)
  uint8_t mode;              // SessionMode
  uint8_t sleep_state;       // SleepState
  uint16_t sleep_latency_min;
  uint16_t sleep_total_min;
  uint16_t sleep_deep_min;
  int16_t sleep_delta_min;   // Schlafdauer ggü. persönlichem Durchschnitt
  int16_t grav[3];           // mittlerer Schwerkraftvektor (mg)
  uint8_t orient_pct[ORIENT_COUNT];
  // Start in den Tag (nur Morgen-Sessions), belegt die frühere Reserve
  uint8_t day_state;         // DayState
  uint8_t mood;              // 1..5, 0 = nicht bewertet
  uint16_t day_steps;        // Schritte in den 4 h danach
  int8_t day_steps_pct;      // ggü. üblich an diesem Wochentagstyp, in %
  int8_t day_hr_delta;       // Puls ggü. üblich, bpm
  uint8_t day_active_min;
  uint8_t pos_pct[POS_COUNT]; // Zeitanteil je erkannter Stellung (Partner-Modus)
  uint8_t partner;            // 1..PARTNER_COUNT, 0 = keine Angabe
} Session;

// i18n.c
void i18n_load(void);
const char *tr(StrId id);
const char *lang_name(int index);  // Eigenname der Sprache, z. B. "Deutsch"

// Vorzeichen für "%s%d"-Paare in den Texten
#define SIGN(v) ((v) >= 0 ? "+" : "")

// storage.c
void storage_init(void);
int storage_count(void);
Session *storage_get(int idx);  // 0 = neuester Eintrag
int storage_find(uint32_t start);
void storage_add(const Session *s);
void storage_save(int idx);
void storage_delete(int idx);
void storage_delete_all(void);
Profile *storage_profile(void);
void storage_save_profile(void);
Calib *storage_calib(int which);
void storage_save_calib(void);

// settings.c (Einstellungen aus der Pebble-App auf dem Handy)
void settings_init(void);
void settings_send_profile(void);

// sleep.c
bool sleep_evaluate(Session *s);  // true = Eintrag wurde geändert
void sleep_evaluate_all(void);

// morning.c
bool morning_is_morning(time_t t);
bool morning_evaluate(Session *s);  // true = Eintrag wurde geändert
void morning_schedule_checkin(const Session *s);
void morning_init(void);
bool morning_handle_launch(void);   // Check-in fällig? Dann Fenster öffnen
void mood_window_push(int idx);

// detector.c (App-Seite: Parameter aus Profil + Anlernen)
void detector_params_for(SessionMode mode, DetectorParams *out);

// autostart.c (automatischer Session-Start über den Hintergrund-Worker)
void autostart_init(void);            // Worker passend zur Einstellung starten/stoppen
void autostart_settings_changed(void);
void autostart_session_active(bool active);
bool autostart_handle_launch(void);   // Start durch den Worker? Dann Nachfrage zeigen

// positions.c (Stellungserkennung im Partner-Modus)
enum { F_GX, F_GY, F_GZ, F_SX, F_SY, F_SZ, F_RMS, F_SPM, POS_FEATURES };

typedef struct {
  uint8_t count;               // Anzahl Aufnahmen, 0 = nicht angelernt
  int16_t f[POS_FEATURES];     // gemittelter Fingerabdruck
} PosTemplate;

typedef struct {
  int32_t grav[3];
  int32_t sq[3];
  int n;
  int cycles;
} PosAcc;

void positions_init(void);
void positions_window_push(void);
const char *pos_name(int i);         // eigener Name oder Standardname, -1 = "unbekannt"
const char *pos_custom_name(int i);  // "" = Standardname
void pos_set_name(int i, const char *name);
int pos_trained_count(void);
bool pos_in_use(int i);
void pos_delete(int i);
void pos_acc_reset(PosAcc *a);
void pos_acc_add(PosAcc *a, const Detector *d, int16_t x, int16_t y, int16_t z, bool cycle);
bool pos_acc_ready(const PosAcc *a);
bool pos_acc_features(PosAcc *a, uint32_t moving, int16_t *f);
int pos_classify(const int16_t *f);  // -1 = unbekannt

// partners.c
typedef void (*PartnerCallback)(int partner);
void partners_init(void);
const char *partner_name(int partner);  // "" = keine Angabe
void partner_set_name(int partner, const char *name);
void partner_choose(PartnerCallback cb);  // fragt "Mit wem?" (falls eingeschaltet)

// tracker.c
SessionMode solo_mode(void);
void tracker_window_push(SessionMode mode);
// Session rückdatiert starten (automatische Erkennung), partner: 0 = keine Angabe
void tracker_window_push_at(SessionMode mode, time_t start, uint16_t cycles, int partner);

// calibrate.c
void calibrate_window_push(void);

// history.c
const char *mode_name(uint8_t mode);
void history_window_push(void);
void detail_window_push(int idx);
void stats_window_push(void);

// profile.c
void profile_window_push(void);

// demo.c (nur Demo-Build, sonst leer)
void demo_seed(void);
void pos_set_template(int i, const int16_t *f, uint8_t count);

// util.c
void fmt_duration(char *buf, size_t n, int secs);
void fmt_minutes(char *buf, size_t n, int mins);
void fmt_date(char *buf, size_t n, time_t t);
void fmt_kcal(char *buf, size_t n, int kcal_x10);
