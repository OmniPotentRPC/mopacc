#include "mopacc.h"

#include <stdio.h>
#include <string.h>

#ifndef MOPACC_VERSION_STRING
#define MOPACC_VERSION_STRING "mopacc-stub/0.1.0"
#endif

static _Thread_local char g_last_error[512] =
    "OpenMOPAC embed not available in mopacc stub";

static MopacCResult stub_fail(void) {
  MopacCResult r;
  r.ok = 0;
  r.energy_h = 0.0;
  snprintf(r.message, sizeof(r.message), "%s", g_last_error);
  return r;
}

int mopacc_set_params(const void *params, size_t params_size_bytes) {
  (void)params;
  (void)params_size_bytes;
  return -1;
}

MopacCResult mopacc_energy_gradient(int n_atoms, const double *positions_ang,
                                    const int *atomic_numbers,
                                    const void *params,
                                    size_t params_size_bytes,
                                    double *grad_h_bohr) {
  (void)n_atoms;
  (void)positions_ang;
  (void)atomic_numbers;
  (void)params;
  (void)params_size_bytes;
  (void)grad_h_bohr;
  return stub_fail();
}

MopacCResult mopacc_energy(int n_atoms, const double *positions_ang,
                           const int *atomic_numbers, const void *params,
                           size_t params_size_bytes) {
  (void)n_atoms;
  (void)positions_ang;
  (void)atomic_numbers;
  (void)params;
  (void)params_size_bytes;
  return stub_fail();
}

MopacCResult mopacc_energy_forces(int n_atoms, const double *positions_ang,
                                  const int *atomic_numbers, const void *params,
                                  size_t params_size_bytes,
                                  double *forces_h_bohr) {
  (void)n_atoms;
  (void)positions_ang;
  (void)atomic_numbers;
  (void)params;
  (void)params_size_bytes;
  (void)forces_h_bohr;
  return stub_fail();
}

MopacCSession *mopacc_session_create(const void *params,
                                     size_t params_size_bytes) {
  (void)params;
  (void)params_size_bytes;
  return NULL;
}

void mopacc_session_destroy(MopacCSession *session) { (void)session; }

int mopacc_session_set_params(MopacCSession *session, const void *params,
                              size_t params_size_bytes) {
  (void)session;
  (void)params;
  (void)params_size_bytes;
  return -1;
}

MopacCResult mopacc_session_energy_gradient(MopacCSession *session, int n_atoms,
                                            const double *positions_ang,
                                            const int *atomic_numbers,
                                            double *grad_h_bohr) {
  (void)session;
  (void)n_atoms;
  (void)positions_ang;
  (void)atomic_numbers;
  (void)grad_h_bohr;
  return stub_fail();
}

const char *mopacc_version(void) { return MOPACC_VERSION_STRING; }
int mopacc_c_abi_version(void) { return RGPOT_MOPACC_C_ABI_VERSION; }
int mopacc_abi_version(void) { return MOPACC_ABI_VERSION; }
int mopacc_available(void) { return 0; }
const char *mopacc_last_error(void) { return g_last_error; }
void mopacc_finalize(void) {}
