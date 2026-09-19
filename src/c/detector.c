#include "app.h"

// Bewegungserkennung bei 25 Hz: Pro Achse wird die Schwerkraft per Tiefpass
// abgezogen, die Achse mit der meisten Bewegungsenergie gewählt und deren
// Signal mit einem Schmitt-Trigger (adaptive Schwelle) in Zyklen zerlegt.

// Standardwerte je Modus (Empfindlichkeit "normal", nicht angelernt)
//   Streichen: größere, lineare Bewegungen des Unterarms, 1-4 Hz
//   Reiben:    kleine, schnelle, oft kreisende Bewegungen der Finger/Hand, bis ~8 Hz
//   Toy:       Hand hält eher still, nur die aktive Zeit wird erfasst
static const DetectorParams DEFAULTS[] = {
    [MODE_SOLO] = {1600, 50, 6, 4, true},
    [MODE_PARTNER] = {1600, 50, 6, 5, true},
    [MODE_SOLO_RUB] = {400, 25, 5, 3, true},
    [MODE_SOLO_TOY] = {250, 0, 0, 0, false},
};

// Skalierung je Empfindlichkeit (niedrig / normal / hoch) in Zehnteln
static const int32_t ENERGY_SCALE[] = {16, 10, 6};
static const int32_t THRESHOLD_SCALE[] = {12, 10, 8};

static int calib_index(SessionMode mode) {
  switch (mode) {
    case MODE_SOLO: return CALIB_STROKE;
    case MODE_SOLO_RUB: return CALIB_RUB;
    case MODE_PARTNER: return CALIB_PARTNER;
    default: return -1;
  }
}

int32_t isqrt32(int32_t v) {
  if (v <= 0) return 0;
  int32_t r = v, x = (v + 1) / 2;
  while (x < r) {
    r = x;
    x = (x + v / x) / 2;
  }
  return r;
}

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void detector_params_from_calib(SessionMode mode, const Calib *c, DetectorParams *out) {
  *out = DEFAULTS[mode];
  if (!c || !c->valid || !out->count_cycles) return;
  // Ruhe-Grenze bei einem Fünftel der typischen Energie, Schwelle bei 30 % der RMS-Amplitude
  out->min_energy = clamp(c->energy / 5, 150, 8000);
  out->min_threshold = clamp(isqrt32(c->energy) * 3 / 10, 15, 120);
  out->min_gap = clamp(c->min_gap, 2, 12);
}

void detector_params_for(SessionMode mode, DetectorParams *out) {
  int ci = calib_index(mode);
  detector_params_from_calib(mode, ci >= 0 ? storage_calib(ci) : NULL, out);

  uint8_t sens = storage_profile()->sensitivity;
  if (sens > SENS_HIGH) sens = SENS_NORMAL;
  out->min_energy = out->min_energy * ENERGY_SCALE[sens] / 10;
  out->min_threshold = out->min_threshold * THRESHOLD_SCALE[sens] / 10;
}

void detector_init(Detector *d, const DetectorParams *p) {
  memset(d, 0, sizeof(*d));
  d->p = *p;
  d->first = true;
}

bool detector_process(Detector *d, int16_t x, int16_t y, int16_t z) {
  int32_t v[3] = {x, y, z};
  if (d->first) {
    for (int a = 0; a < 3; a++) d->lp16[a] = v[a] * 16;
    d->first = false;
  }

  int32_t hp[3];
  int dom = 0;
  for (int a = 0; a < 3; a++) {
    d->lp16[a] += (v[a] * 16 - d->lp16[a]) / 16;
    hp[a] = v[a] - d->lp16[a] / 16;
    d->energy[a] += (hp[a] * hp[a] - d->energy[a]) / 32;
    if (d->energy[a] > d->energy[dom]) dom = a;
  }
  d->dom = dom;

  d->smooth += (hp[dom] - d->smooth) / 2;
  if (d->since_last < 1000) d->since_last++;

  if (d->energy[dom] < d->p.min_energy) {
    d->phase = 0;
    return false;
  }
  d->moving++;
  if (!d->p.count_cycles) return false;

  int32_t th = isqrt32(d->energy[dom]) * d->p.threshold_tenths / 10;
  if (th < d->p.min_threshold) th = d->p.min_threshold;

  if (d->phase == 0) {
    if (d->smooth < -th) d->phase = 1;
  } else if (d->smooth > th && d->since_last >= d->p.min_gap) {
    d->phase = 0;
    d->since_last = 0;
    return true;
  }
  return false;
}
