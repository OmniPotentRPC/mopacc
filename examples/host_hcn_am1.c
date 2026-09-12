#include "mopacc.h"

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
    fprintf(stderr, "mopacc stub: libmopac not linked\n");
    return 2;
  }

  p.charge = 0;
  p.spin = 0;
  p.model = MOPACC_MODEL_AM1;
  p.tolerance = 1.0;
  p.max_time = 120;

  r = mopacc_energy_gradient(3, POS, Z, &p, sizeof(p), grad);
  printf("ok=%d energy_h=%.10f msg=%s\n", r.ok, r.energy_h, r.message);
  if (!r.ok) {
    fprintf(stderr, "mopacc last_error: %s\n", mopacc_last_error());
    return 1;
  }
  for (i = 0; i < 3; ++i) {
    printf("grad[%d] % .8e % .8e % .8e\n", i, grad[3 * i], grad[3 * i + 1],
           grad[3 * i + 2]);
  }
  return 0;
}
