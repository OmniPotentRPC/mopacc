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

static int check_result(const char *label, MopacCResult r, const double *grad) {
  int i;
  if (r.ok != 1) {
    fprintf(stderr, "%s ok=%d msg=%s last=%s\n", label, r.ok, r.message,
            mopacc_last_error());
    return 0;
  }
  if (!isfinite(r.energy_h)) {
    fprintf(stderr, "%s non-finite energy_h\n", label);
    return 0;
  }
  for (i = 0; i < 9; ++i) {
    if (!isfinite(grad[i])) {
      fprintf(stderr, "%s non-finite gradient[%d]\n", label, i);
      return 0;
    }
  }
  return 1;
}

int main(void) {
  MopacCParams p;
  MopacCSession *s;
  MopacCResult r1;
  MopacCResult r2;
  double g1[9];
  double g2[9];
  int i;

  if (!mopacc_available()) {
    return 0;
  }

  p.charge = 0;
  p.spin = 0;
  p.model = MOPACC_MODEL_AM1;
  p.tolerance = 1.0;
  p.max_time = 120;

  s = mopacc_session_create(&p, sizeof(p));
  if (s == NULL) {
    fprintf(stderr, "mopacc_session_create failed: %s\n", mopacc_last_error());
    return 1;
  }

  for (i = 0; i < 9; ++i) {
    g1[i] = 0.0;
    g2[i] = 0.0;
  }

  r1 = mopacc_session_energy_gradient(s, 3, POS, Z, g1);
  if (!check_result("call1", r1, g1)) {
    mopacc_session_destroy(s);
    return 1;
  }

  r2 = mopacc_session_energy_gradient(s, 3, POS, Z, g2);
  if (!check_result("call2", r2, g2)) {
    mopacc_session_destroy(s);
    return 1;
  }

  mopacc_session_destroy(s);
  mopacc_finalize();
  return 0;
}
