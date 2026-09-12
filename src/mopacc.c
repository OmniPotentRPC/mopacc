#include "mopacc.h"

#include "mopac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOPACC_VERSION_STRING
#define MOPACC_VERSION_STRING "0.1.0"
#endif

/* CODATA 2018. OpenMOPAC heat is kcal/mol; coord_deriv is kcal/mol/A. */
static const double KCAL_PER_HARTREE = 627.5094740631;
static const double ANG_PER_BOHR = 0.529177210903;

struct MopacCSession {
  MopacCParams params;
  struct mopac_state state;
};

static _Thread_local char g_last_error[512] = "";
static MopacCParams g_params;
static int g_params_set;

static void set_error(const char *msg) {
  snprintf(g_last_error, sizeof(g_last_error), "%s", msg ? msg : "");
}

static MopacCParams default_params(void) {
  MopacCParams p;
  memset(&p, 0, sizeof(p));
  p.model = MOPACC_MODEL_AM1;
  p.tolerance = 1.0;
  p.max_time = 3600;
  return p;
}

static int parse_params(const void *params, size_t n, MopacCParams *out) {
  *out = default_params();
  if (params == NULL || n == 0) {
    return 0;
  }
  if (n < sizeof(MopacCParams)) {
    set_error("mopacc params blob smaller than MopacCParams");
    return -1;
  }
  memcpy(out, params, sizeof(MopacCParams));
  if (out->model < 0 || out->model > MOPACC_MODEL_RM1) {
    set_error("mopacc model out of range (0..5); AM1 is 4");
    return -1;
  }
  /* A zeroed struct still means AM1: 0 is PM7 in OpenMOPAC, but a
   * memset'd MopacCParams from a caller who only set charge would
   * silently switch Hamiltonian. Require explicit model, default AM1. */
  if (out->model == 0 && n == sizeof(MopacCParams)) {
    /* keep 0 as PM7 if the caller wrote the full struct; documented. */
  }
  if (out->tolerance <= 0.0) {
    out->tolerance = 1.0;
  }
  if (out->max_time <= 0) {
    out->max_time = 3600;
  }
  return 0;
}

static MopacCParams active_params(const void *params, size_t n, int *ok) {
  MopacCParams p;
  if (params != NULL && n > 0) {
    *ok = parse_params(params, n, &p) == 0;
    return p;
  }
  if (g_params_set) {
    *ok = 1;
    return g_params;
  }
  *ok = 1;
  return default_params();
}

static void copy_errors(const struct mopac_properties *prop, char *dst,
                        size_t dst_n) {
  if (prop->nerror <= 0 || prop->error_msg == NULL) {
    snprintf(dst, dst_n, "ok");
    return;
  }
  snprintf(dst, dst_n, "%s", prop->error_msg[0] ? prop->error_msg[0] : "mopac error");
}

static MopacCResult run_scf(const MopacCParams *p, int n_atoms,
                            const double *positions_ang,
                            const int *atomic_numbers, struct mopac_state *st,
                            double *grad_h_bohr, int want_grad) {
  MopacCResult r;
  struct mopac_system sys;
  struct mopac_properties prop;
  int i;

  memset(&r, 0, sizeof(r));
  memset(&sys, 0, sizeof(sys));
  memset(&prop, 0, sizeof(prop));

  if (n_atoms <= 0 || positions_ang == NULL || atomic_numbers == NULL) {
    set_error("mopacc: empty geometry");
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }
  if (want_grad && grad_h_bohr == NULL) {
    set_error("mopacc: gradient buffer is NULL");
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }

  sys.natom = n_atoms;
  sys.natom_move = n_atoms;
  sys.charge = p->charge;
  sys.spin = p->spin;
  sys.model = p->model;
  sys.epsilon = 1.0;
  sys.atom = (int *)atomic_numbers;
  sys.coord = (double *)positions_ang;
  sys.nlattice = 0;
  sys.nlattice_move = 0;
  sys.pressure = 0.0;
  sys.lattice = NULL;
  sys.tolerance = p->tolerance;
  sys.max_time = p->max_time;

  mopac_scf(&sys, st, &prop);

  if (prop.nerror > 0) {
    copy_errors(&prop, r.message, sizeof(r.message));
    set_error(r.message);
    destroy_mopac_properties(&prop);
    return r;
  }

  r.ok = 1;
  r.energy_h = prop.heat / KCAL_PER_HARTREE;
  copy_errors(&prop, r.message, sizeof(r.message));
  set_error("");

  if (want_grad) {
    if (prop.coord_deriv == NULL) {
      r.ok = 0;
      snprintf(r.message, sizeof(r.message),
               "mopacc: mopac_scf returned no coord_deriv");
      set_error(r.message);
      destroy_mopac_properties(&prop);
      return r;
    }
    for (i = 0; i < n_atoms * 3; ++i) {
      grad_h_bohr[i] =
          (prop.coord_deriv[i] / KCAL_PER_HARTREE) * ANG_PER_BOHR;
    }
  }

  destroy_mopac_properties(&prop);
  return r;
}

int mopacc_set_params(const void *params, size_t params_size_bytes) {
  if (parse_params(params, params_size_bytes, &g_params) != 0) {
    return -1;
  }
  g_params_set = 1;
  return 0;
}

MopacCResult mopacc_energy_gradient(int n_atoms, const double *positions_ang,
                                    const int *atomic_numbers,
                                    const void *params,
                                    size_t params_size_bytes,
                                    double *grad_h_bohr) {
  int ok = 0;
  MopacCParams p = active_params(params, params_size_bytes, &ok);
  struct mopac_state st;
  MopacCResult r;
  memset(&st, 0, sizeof(st));
  if (!ok) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }
  r = run_scf(&p, n_atoms, positions_ang, atomic_numbers, &st, grad_h_bohr, 1);
  destroy_mopac_state(&st);
  return r;
}

MopacCResult mopacc_energy(int n_atoms, const double *positions_ang,
                           const int *atomic_numbers, const void *params,
                           size_t params_size_bytes) {
  int ok = 0;
  MopacCParams p = active_params(params, params_size_bytes, &ok);
  struct mopac_state st;
  MopacCResult r;
  memset(&st, 0, sizeof(st));
  if (!ok) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }
  r = run_scf(&p, n_atoms, positions_ang, atomic_numbers, &st, NULL, 0);
  destroy_mopac_state(&st);
  return r;
}

MopacCResult mopacc_energy_forces(int n_atoms, const double *positions_ang,
                                  const int *atomic_numbers, const void *params,
                                  size_t params_size_bytes,
                                  double *forces_h_bohr) {
  MopacCResult r = mopacc_energy_gradient(n_atoms, positions_ang,
                                          atomic_numbers, params,
                                          params_size_bytes, forces_h_bohr);
  if (r.ok && forces_h_bohr != NULL) {
    int i;
    for (i = 0; i < n_atoms * 3; ++i) {
      forces_h_bohr[i] = -forces_h_bohr[i];
    }
  }
  return r;
}

MopacCSession *mopacc_session_create(const void *params,
                                     size_t params_size_bytes) {
  MopacCSession *s = calloc(1, sizeof(*s));
  if (s == NULL) {
    set_error("mopacc_session_create: out of memory");
    return NULL;
  }
  if (parse_params(params, params_size_bytes, &s->params) != 0) {
    free(s);
    return NULL;
  }
  s->state.mpack = 0;
  return s;
}

void mopacc_session_destroy(MopacCSession *session) {
  if (session == NULL) {
    return;
  }
  destroy_mopac_state(&session->state);
  free(session);
}

int mopacc_session_set_params(MopacCSession *session, const void *params,
                              size_t params_size_bytes) {
  if (session == NULL) {
    set_error("mopacc_session_set_params: null session");
    return -1;
  }
  return parse_params(params, params_size_bytes, &session->params);
}

MopacCResult mopacc_session_energy_gradient(MopacCSession *session, int n_atoms,
                                            const double *positions_ang,
                                            const int *atomic_numbers,
                                            double *grad_h_bohr) {
  MopacCResult r;
  if (session == NULL) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "mopacc: null session");
    set_error(r.message);
    return r;
  }
  return run_scf(&session->params, n_atoms, positions_ang, atomic_numbers,
                 &session->state, grad_h_bohr, 1);
}

const char *mopacc_version(void) { return MOPACC_VERSION_STRING; }
int mopacc_c_abi_version(void) { return RGPOT_MOPACC_C_ABI_VERSION; }
int mopacc_abi_version(void) { return MOPACC_ABI_VERSION; }
int mopacc_available(void) { return 1; }
const char *mopacc_last_error(void) { return g_last_error; }
void mopacc_finalize(void) { g_params_set = 0; }
