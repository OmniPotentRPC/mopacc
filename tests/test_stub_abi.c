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
  if (mopacc_energy(3, pos, z, NULL, 0).ok) {
    fprintf(stderr, "stub energy must not report ok\n");
    return 1;
  }
  if (mopacc_energy_forces(3, pos, z, NULL, 0, g).ok) {
    fprintf(stderr, "stub energy_forces must not report ok\n");
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
  if (mopacc_session_set_params(NULL, NULL, 0) == 0) {
    fprintf(stderr, "stub session_set_params must fail\n");
    return 1;
  }
  if (mopacc_session_energy_gradient(NULL, 3, pos, z, g).ok) {
    fprintf(stderr, "stub session_energy_gradient must not report ok\n");
    return 1;
  }
  mopacc_session_destroy(NULL);
  if (mopacc_last_error() == NULL || mopacc_last_error()[0] == '\0') {
    fprintf(stderr, "stub last_error should explain the missing embed\n");
    return 1;
  }
  mopacc_finalize();
  return 0;
}
