#pragma once
// Gemeinsam von App (src/c) und Hintergrund-Worker (worker_src/c) genutzt.
// Nur Standard-Header: pebble.h und pebble_worker.h dürfen nicht gemischt werden.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Persistente Schlüssel (App und Worker teilen sich den Speicher)
#define KEY_VERSION 1
#define KEY_PROFILE 2
#define KEY_COUNT 3
#define KEY_CALIB 4
#define KEY_AUTO 5          // letzte automatische Erkennung (AutoDetect)
#define KEY_POS_TEMPLATES 6 // angelernte Stellungen
#define KEY_POS_NAMES 7     // eigene Namen der Stellungen
#define KEY_SESSION_BASE 100

// Werte bleiben stabil, da sie in gespeicherten Sessions stehen
typedef enum {
  MODE_SOLO = 0,     // Solo, Streichen (Penis)
  MODE_PARTNER = 1,
  MODE_SOLO_RUB = 2, // Solo, Reiben (Klitoris, kleine kreisende Bewegungen)
  MODE_SOLO_TOY = 3, // Solo mit Toy: nur aktive Zeit, keine Zyklen
} SessionMode;

typedef enum { STYLE_AUTO = 0, STYLE_STROKE = 1, STYLE_RUB = 2, STYLE_TOY = 3 } SoloStyle;
typedef enum { SEX_MALE = 0, SEX_FEMALE = 1, SEX_OTHER = 2 } Sex;
typedef enum { SENS_LOW = 0, SENS_NORMAL = 1, SENS_HIGH = 2 } Sensitivity;
typedef enum { LIGHT_NORMAL = 0, LIGHT_ON = 1, LIGHT_PULSE = 2 } LightMode;

typedef struct __attribute__((packed)) {
  uint8_t sex;
  uint8_t age;
  uint16_t weight_kg;
  uint8_t sensitivity;    // Sensitivity
  uint8_t touch_session;  // Wischgesten während der Session (nur Touch-Uhren)
  uint8_t solo_style;     // SoloStyle
  uint8_t lang;           // 0 = automatisch, sonst 1 + Index in LANG_CODES
  uint8_t checkin;        // Stimmung nach Morgen-Session abfragen
  uint8_t auto_start;     // 0 = aus, sonst Minuten bis zur Nachfrage
  uint8_t light;          // LightMode während der Session
} Profile;

// Angelernte Erkennung je Bewegungsart
enum { CALIB_STROKE, CALIB_RUB, CALIB_PARTNER, CALIB_COUNT };

typedef struct __attribute__((packed)) {
  uint8_t valid;
  uint8_t spm;           // gemessener Rhythmus
  uint16_t min_gap;      // Mindestabstand zweier Zyklen in Samples
  int32_t energy;        // mittlere Bewegungsenergie (mg^2)
} Calib;

// Vom Worker erkannte Aktivität, für die Nachfrage in der App
typedef struct __attribute__((packed)) {
  uint32_t start;        // Beginn der rhythmischen Bewegung
  uint16_t cycles;       // bis dahin gezählte Zyklen
  uint8_t mode;          // SessionMode, nach dem erkannt wurde
} AutoDetect;

// Nachrichten zwischen App und Worker (AppWorkerMessage.data0 frei)
enum {
  WMSG_SESSION_ACTIVE = 1,  // App -> Worker: Session läuft, nicht erkennen
  WMSG_SESSION_IDLE = 2,    // App -> Worker: Session beendet
  WMSG_RELOAD = 3,          // App -> Worker: Profil/Anlernen geändert
  WMSG_DETECTED = 10,       // Worker -> App: Aktivität erkannt (Details in KEY_AUTO)
};

// --- Bewegungserkennung --------------------------------------------------

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

// Parameter für Modus + Anlernen + Empfindlichkeit (c darf NULL sein)
void detector_params(SessionMode mode, const Calib *c, uint8_t sensitivity, DetectorParams *out);
void detector_params_from_calib(SessionMode mode, const Calib *c, DetectorParams *out);
void detector_init(Detector *d, const DetectorParams *p);
bool detector_process(Detector *d, int16_t x, int16_t y, int16_t z);  // true = neuer Zyklus
int32_t isqrt32(int32_t v);
int calib_index(SessionMode mode);  // -1 = kein Anlernen für diesen Modus
SessionMode solo_mode_for(const Profile *p);
