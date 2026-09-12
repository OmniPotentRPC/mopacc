#include "mopacc.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
  if (mopacc_c_abi_version() != RGPOT_MOPACC_C_ABI_VERSION) {
    fprintf(stderr, "abi version mismatch\n");
    return 1;
  }
  if (mopacc_abi_version() != MOPACC_ABI_VERSION) {
    fprintf(stderr, "abi version mismatch\n");
    return 1;
  }
  if (mopacc_version() == NULL || mopacc_version()[0] == '\0') {
    fprintf(stderr, "empty version\n");
    return 1;
  }

  if (mopacc_available()) {
    /* Real embed: HCN smoke lives in host_hcn_am1. ABI probes only here. */
    return 0;
  }

  double pos[9] = {0};
  int z[3] = {6, 7, 1};
  double g[9] = {0};
  MopacCResult r =
      mopacc_energy_gradient(3, pos, z, NULL, 0, g);
  if (r.ok) {
    fprintf(stderr, "stub must not report ok\n");
    return 1;
  }
  if (mopacc_set_params(NULL, 0) == 0) {
    fprintf(stderr, "stub set_params must fail\n");
    return 1;
  }
  if (mopacc_session_create(NULL, 0) != NULL) {
    fprintf(stderr, "stub session_create must fail\n");
    return 1;
  }
  return 0;
}
