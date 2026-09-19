#include "app.h"

// Die Erkennung selbst liegt in src/shared/detector.c (auch vom Worker genutzt)

void detector_params_for(SessionMode mode, DetectorParams *out) {
  int ci = calib_index(mode);
  detector_params(mode, ci >= 0 ? storage_calib(ci) : NULL, storage_profile()->sensitivity, out);
}
