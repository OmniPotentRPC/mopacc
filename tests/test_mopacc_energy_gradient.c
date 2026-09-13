#include "mopacc.h"

#include <math.h>
#include <stdio.h>

/* Baker-Chan HCN reactant (IRC endpoint), Angstrom. */
static const int Z[3] = {6, 7, 1};
static const double POS[9] = {
    0.36034298970010631, -0.27031640329923812, 0.00000001386933716,
    -0.61249335751037925, 0.33182649086758403, -0.00000005639996329,
    1.26739044288123237, -0.83193477410619154, 0.00000009474306231,
};

int main(void) {
  double grad[9];
  MopacCParams p;
  MopacCResult r;
  int i;

  if (!mopacc_available()) {
    return 0;
  }

  p.charge = 0;
  p.spin = 0;
  p.model = MOPACC_MODEL_AM1;
  p.tolerance = 1.0;
  p.max_time = 120;

  for (i = 0; i < 9; ++i) {
    grad[i] = 0.0;
  }

  r = mopacc_energy_gradient(3, POS, Z, &p, sizeof(p), grad);
  if (r.ok != 1) {
    fprintf(stderr, "mopacc_energy_gradient ok=%d msg=%s last=%s\n", r.ok,
            r.message, mopacc_last_error());
    return 1;
  }
  if (!isfinite(r.energy_h)) {
    fprintf(stderr, "non-finite energy_h\n");
    return 1;
  }
  for (i = 0; i < 9; ++i) {
    if (!isfinite(grad[i])) {
      fprintf(stderr, "non-finite gradient[%d]\n", i);
      return 1;
    }
  }
  mopacc_finalize();
  return 0;
}
