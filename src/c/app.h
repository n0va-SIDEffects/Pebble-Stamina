#pragma once
#include <pebble.h>
#include "i18n.auto.h"

#define MAX_SESSIONS 30

#define ACCENT_COLOR PBL_IF_COLOR_ELSE(GColorFolly, GColorWhite)

// Werte bleiben stabil, da sie in gespeicherten Sessions stehen
typedef enum {
  MODE_SOLO = 0,     // Solo, Streichen (Penis)
  MODE_PARTNER = 1,
  MODE_SOLO_RUB = 2, // Solo, Reiben (Klitoris, kleine kreisende Bewegungen)
  MODE_SOLO_TOY = 3, // Solo mit Toy: nur aktive Zeit, keine Zyklen
} SessionMode;

typedef enum { STYLE_AUTO = 0, STYLE_STROKE = 1, STYLE_RUB = 2, STYLE_TOY = 3 } SoloStyle;

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

typedef enum { SEX_MALE = 0, SEX_FEMALE = 1, SEX_OTHER = 2 } Sex;

// Orientierung des Handgelenks (Schwerkraftachse), Basis für spätere Stellungserkennung
enum { ORIENT_XP, ORIENT_XN, ORIENT_YP, ORIENT_YN, ORIENT_ZP, ORIENT_ZN, ORIENT_COUNT };

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
} Session;

typedef struct __attribute__((packed)) {
  uint8_t sex;
  uint8_t age;
  uint16_t weight_kg;
  uint8_t sensitivity;    // Sensitivity
  uint8_t touch_session;  // Wischgesten während der Session (nur Touch-Uhren)
  uint8_t solo_style;     // SoloStyle
  uint8_t lang;           // 0 = automatisch, sonst 1 + Index in LANG_CODES
  uint8_t checkin;        // Stimmung nach Morgen-Session abfragen
} Profile;

typedef enum { SENS_LOW = 0, SENS_NORMAL = 1, SENS_HIGH = 2 } Sensitivity;

// Angelernte Erkennung je Bewegungsart
enum { CALIB_STROKE, CALIB_RUB, CALIB_PARTNER, CALIB_COUNT };

typedef struct __attribute__((packed)) {
  uint8_t valid;
  uint8_t spm;           // gemessener Rhythmus
  uint16_t min_gap;      // Mindestabstand zweier Zyklen in Samples
  int32_t energy;        // mittlere Bewegungsenergie (mg^2)
} Calib;

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
bool morning_handle_launch(void);   // Check-in fällig? Dann Fenster öffnen
void mood_window_push(int idx);

// detector.c
typedef struct {
  int32_t min_energy;       // darunter gilt als Ruhe (mg^2)
  int32_t min_threshold;    // mg
  int32_t threshold_tenths; // Schwelle als Anteil der RMS-Amplitude
  int32_t min_gap;          // Samples zwischen zwei Zyklen
  bool count_cycles;
} DetectorParams;

typedef struct {
  DetectorParams p;
  bool first;
  int32_t lp16[3];     // Schwerkraftschätzung * 16
  int32_t energy[3];   // gleitende Varianz je Achse
  int32_t smooth;
  int phase;
  int since_last;
  int dom;
  uint32_t moving;     // Samples mit Bewegung (vom Aufrufer zurücksetzbar)
} Detector;

void detector_params_for(SessionMode mode, DetectorParams *out);
void detector_params_from_calib(SessionMode mode, const Calib *c, DetectorParams *out);
void detector_init(Detector *d, const DetectorParams *p);
bool detector_process(Detector *d, int16_t x, int16_t y, int16_t z);  // true = neuer Zyklus
int32_t isqrt32(int32_t v);

// tracker.c
SessionMode solo_mode(void);
void tracker_window_push(SessionMode mode);

// calibrate.c
void calibrate_window_push(void);

// history.c
const char *mode_name(uint8_t mode);
void history_window_push(void);
void detail_window_push(int idx);
void stats_window_push(void);

// profile.c
void profile_window_push(void);

// util.c
void fmt_duration(char *buf, size_t n, int secs);
void fmt_minutes(char *buf, size_t n, int mins);
void fmt_date(char *buf, size_t n, time_t t);
void fmt_kcal(char *buf, size_t n, int kcal_x10);
