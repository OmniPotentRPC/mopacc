#include "mopacc.h"

#include "mopac.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOPACC_VERSION_STRING
#define MOPACC_VERSION_STRING "0.1.0"
#endif

/* CODATA 2018. OpenMOPAC heat is kcal/mol; coord_deriv is kcal/mol/A. */
static const double KCAL_PER_HARTREE = 627.5094740631;
static const double ANG_PER_BOHR = 0.529177210903;

struct MopacCCell {
  int nlattice;
  int nlattice_move;
  double pressure;
  double lattice[9];
};

struct MopacCSession {
  MopacCParams params;
  struct MopacCCell cell;
  struct mopac_state state;
  struct mozyme_state zstate;
  int n_last;
  double *charges;
};

enum MopacOp { MOPACC_OP_SCF = 0, MOPACC_OP_RELAX = 1, MOPACC_OP_VIBE = 2 };

static _Thread_local char g_last_error[512] = "";
static MopacCParams g_params;
static int g_params_set;
static struct MopacCCell g_cell;
static int g_n_last;
static double *g_charges;

static void set_error(const char *msg) {
  snprintf(g_last_error, sizeof(g_last_error), "%s", msg ? msg : "");
}

static MopacCParams default_params(void) {
  MopacCParams p;
  memset(&p, 0, sizeof(p));
  p.model = MOPACC_MODEL_AM1;
  p.tolerance = 1.0;
  p.max_time = 3600;
  p.epsilon = 1.0;
  p.solver = MOPACC_SOLVER_MOPAC;
  return p;
}

static int parse_params(const void *params, size_t n, MopacCParams *out) {
  *out = default_params();
  if (params == NULL || n == 0) {
    return 0;
  }
  /* Older blobs stop after max_time (24–32 bytes). Copy what arrived. */
  if (n < 24) {
    set_error("mopacc params blob smaller than the original MopacCParams");
    return -1;
  }
  memcpy(out, params, n < sizeof(*out) ? n : sizeof(*out));
  if (out->model < 0 || out->model > MOPACC_MODEL_RM1) {
    set_error("mopacc model out of range (0 PM7 .. 5 RM1)");
    return -1;
  }
  if (n < offsetof(MopacCParams, epsilon) || out->epsilon <= 0.0) {
    out->epsilon = 1.0;
  }
  if (n < offsetof(MopacCParams, solver)) {
    out->solver = MOPACC_SOLVER_MOPAC;
  }
  if (out->solver != MOPACC_SOLVER_MOPAC &&
      out->solver != MOPACC_SOLVER_MOZYME) {
    set_error("mopacc solver must be 0 (mopac) or 1 (mozyme)");
    return -1;
  }
  if (out->tolerance <= 0.0) {
    out->tolerance = 1.0;
  }
  if (out->max_time <= 0) {
    out->max_time = 3600;
  }
  return 0;
}

static int parse_cell(const double *lattice_ang, int nlattice,
                      int nlattice_move, double pressure,
                      struct MopacCCell *out) {
  memset(out, 0, sizeof(*out));
  if (nlattice < 0 || nlattice > 3) {
    set_error("mopacc nlattice must be 0..3");
    return -1;
  }
  if (nlattice > 0 && lattice_ang == NULL) {
    set_error("mopacc lattice pointer is NULL");
    return -1;
  }
  if (nlattice_move < 0 || nlattice_move > nlattice) {
    set_error("mopacc nlattice_move out of range");
    return -1;
  }
  out->nlattice = nlattice;
  out->nlattice_move = nlattice_move;
  out->pressure = pressure;
  if (nlattice > 0) {
    memcpy(out->lattice, lattice_ang, (size_t)nlattice * 3u * sizeof(double));
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

static void store_charges(int n_atoms, const double *src, double **dst,
                          int *n_last) {
  free(*dst);
  *dst = NULL;
  *n_last = 0;
  if (src == NULL || n_atoms <= 0) {
    return;
  }
  *dst = (double *)malloc((size_t)n_atoms * sizeof(double));
  if (*dst == NULL) {
    return;
  }
  memcpy(*dst, src, (size_t)n_atoms * sizeof(double));
  *n_last = n_atoms;
}

static void fill_result_props(MopacCResult *r, const struct mopac_properties *prop,
                              int n_atoms, double *grad_h_bohr, int want_grad,
                              double *coord_update_ang, double *freq_cm,
                              double *disp) {
  int i;
  int n3 = n_atoms * 3;
  r->energy_h = prop->heat / KCAL_PER_HARTREE;
  memcpy(r->dipole_debye, prop->dipole, sizeof(r->dipole_debye));
  memcpy(r->stress_gpa, prop->stress, sizeof(r->stress_gpa));
  if (want_grad && grad_h_bohr != NULL && prop->coord_deriv != NULL) {
    for (i = 0; i < n3; ++i) {
      grad_h_bohr[i] =
          (prop->coord_deriv[i] / KCAL_PER_HARTREE) * ANG_PER_BOHR;
    }
  }
  if (coord_update_ang != NULL && prop->coord_update != NULL) {
    memcpy(coord_update_ang, prop->coord_update,
           (size_t)n3 * sizeof(double));
  }
  if (freq_cm != NULL && prop->freq != NULL) {
    memcpy(freq_cm, prop->freq, (size_t)n3 * sizeof(double));
  }
  if (disp != NULL && prop->disp != NULL) {
    memcpy(disp, prop->disp, (size_t)n3 * (size_t)n3 * sizeof(double));
  }
}

static MopacCResult run_job(const MopacCParams *p, const struct MopacCCell *cell,
                            int n_atoms, const double *positions_ang,
                            const int *atomic_numbers, struct mopac_state *st,
                            struct mozyme_state *zst, enum MopacOp op,
                            double *grad_h_bohr, int want_grad,
                            double *coord_update_ang, double *freq_cm,
                            double *disp, double **charge_store, int *n_last) {
  MopacCResult r;
  struct mopac_system sys;
  struct mopac_properties prop;

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
  if (cell && cell->nlattice > 0 && p->epsilon != 1.0) {
    set_error("mopacc: COSMO epsilon must be 1 when a cell is set");
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }

  sys.natom = n_atoms;
  sys.natom_move = n_atoms;
  sys.charge = p->charge;
  sys.spin = p->spin;
  sys.model = p->model;
  sys.epsilon = p->epsilon;
  sys.atom = (int *)atomic_numbers;
  sys.coord = (double *)positions_ang;
  sys.nlattice = cell ? cell->nlattice : 0;
  sys.nlattice_move = cell ? cell->nlattice_move : 0;
  sys.pressure = cell ? cell->pressure : 0.0;
  sys.lattice = (sys.nlattice > 0) ? (double *)cell->lattice : NULL;
  sys.tolerance = p->tolerance;
  sys.max_time = p->max_time;

  if (p->solver == MOPACC_SOLVER_MOZYME) {
    if (op == MOPACC_OP_RELAX) {
      mozyme_relax(&sys, zst, &prop);
    } else if (op == MOPACC_OP_VIBE) {
      mozyme_vibe(&sys, zst, &prop);
    } else {
      mozyme_scf(&sys, zst, &prop);
    }
  } else if (op == MOPACC_OP_RELAX) {
    mopac_relax(&sys, st, &prop);
  } else if (op == MOPACC_OP_VIBE) {
    mopac_vibe(&sys, st, &prop);
  } else {
    mopac_scf(&sys, st, &prop);
  }

  if (prop.nerror > 0) {
    copy_errors(&prop, r.message, sizeof(r.message));
    set_error(r.message);
    destroy_mopac_properties(&prop);
    return r;
  }
  if (want_grad && prop.coord_deriv == NULL) {
    snprintf(r.message, sizeof(r.message),
             "mopacc: OpenMOPAC returned no coord_deriv");
    set_error(r.message);
    destroy_mopac_properties(&prop);
    return r;
  }

  r.ok = 1;
  copy_errors(&prop, r.message, sizeof(r.message));
  set_error("");
  fill_result_props(&r, &prop, n_atoms, grad_h_bohr, want_grad,
                    coord_update_ang, freq_cm, disp);
  store_charges(n_atoms, prop.charge, charge_store, n_last);
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
  struct mozyme_state zst;
  MopacCResult r;
  memset(&st, 0, sizeof(st));
  memset(&zst, 0, sizeof(zst));
  if (!ok) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }
  r = run_job(&p, &g_cell, n_atoms, positions_ang, atomic_numbers, &st, &zst,
              MOPACC_OP_SCF, grad_h_bohr, 1, NULL, NULL, NULL, &g_charges,
              &g_n_last);
  destroy_mopac_state(&st);
  destroy_mozyme_state(&zst);
  return r;
}

MopacCResult mopacc_energy(int n_atoms, const double *positions_ang,
                           const int *atomic_numbers, const void *params,
                           size_t params_size_bytes) {
  int ok = 0;
  MopacCParams p = active_params(params, params_size_bytes, &ok);
  struct mopac_state st;
  struct mozyme_state zst;
  MopacCResult r;
  memset(&st, 0, sizeof(st));
  memset(&zst, 0, sizeof(zst));
  if (!ok) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }
  r = run_job(&p, &g_cell, n_atoms, positions_ang, atomic_numbers, &st, &zst,
              MOPACC_OP_SCF, NULL, 0, NULL, NULL, NULL, &g_charges, &g_n_last);
  destroy_mopac_state(&st);
  destroy_mozyme_state(&zst);
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
  s->zstate.numat = 0;
  return s;
}

void mopacc_session_destroy(MopacCSession *session) {
  if (session == NULL) {
    return;
  }
  destroy_mopac_state(&session->state);
  destroy_mozyme_state(&session->zstate);
  free(session->charges);
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
  return run_job(&session->params, &session->cell, n_atoms, positions_ang,
                 atomic_numbers, &session->state, &session->zstate,
                 MOPACC_OP_SCF, grad_h_bohr, 1, NULL, NULL, NULL,
                 &session->charges, &session->n_last);
}

int mopacc_set_cell(const double *lattice_ang, int nlattice, int nlattice_move,
                    double pressure_gpa) {
  return parse_cell(lattice_ang, nlattice, nlattice_move, pressure_gpa,
                    &g_cell);
}

int mopacc_session_set_cell(MopacCSession *session, const double *lattice_ang,
                            int nlattice, int nlattice_move,
                            double pressure_gpa) {
  if (session == NULL) {
    set_error("mopacc_session_set_cell: null session");
    return -1;
  }
  return parse_cell(lattice_ang, nlattice, nlattice_move, pressure_gpa,
                    &session->cell);
}

static MopacCResult oneshot(enum MopacOp op, int n_atoms,
                            const double *positions_ang,
                            const int *atomic_numbers, const void *params,
                            size_t params_size_bytes, double *grad_h_bohr,
                            int want_grad, double *coord_update_ang,
                            double *freq_cm, double *disp) {
  int ok = 0;
  MopacCParams p = active_params(params, params_size_bytes, &ok);
  struct mopac_state st;
  struct mozyme_state zst;
  MopacCResult r;
  memset(&st, 0, sizeof(st));
  memset(&zst, 0, sizeof(zst));
  if (!ok) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "%s", g_last_error);
    return r;
  }
  r = run_job(&p, &g_cell, n_atoms, positions_ang, atomic_numbers, &st, &zst,
              op, grad_h_bohr, want_grad, coord_update_ang, freq_cm, disp,
              &g_charges, &g_n_last);
  destroy_mopac_state(&st);
  destroy_mozyme_state(&zst);
  return r;
}

MopacCResult mopacc_relax(int n_atoms, const double *positions_ang,
                          const int *atomic_numbers, const void *params,
                          size_t params_size_bytes, double *coord_update_ang,
                          double *grad_h_bohr) {
  return oneshot(MOPACC_OP_RELAX, n_atoms, positions_ang, atomic_numbers,
                 params, params_size_bytes, grad_h_bohr, grad_h_bohr != NULL,
                 coord_update_ang, NULL, NULL);
}

MopacCResult mopacc_vibe(int n_atoms, const double *positions_ang,
                         const int *atomic_numbers, const void *params,
                         size_t params_size_bytes, double *freq_cm,
                         double *disp) {
  return oneshot(MOPACC_OP_VIBE, n_atoms, positions_ang, atomic_numbers,
                 params, params_size_bytes, NULL, 0, NULL, freq_cm, disp);
}

MopacCResult mopacc_session_relax(MopacCSession *session, int n_atoms,
                                  const double *positions_ang,
                                  const int *atomic_numbers,
                                  double *coord_update_ang,
                                  double *grad_h_bohr) {
  MopacCResult r;
  if (session == NULL) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "mopacc: null session");
    set_error(r.message);
    return r;
  }
  return run_job(&session->params, &session->cell, n_atoms, positions_ang,
                 atomic_numbers, &session->state, &session->zstate,
                 MOPACC_OP_RELAX, grad_h_bohr, grad_h_bohr != NULL,
                 coord_update_ang, NULL, NULL, &session->charges,
                 &session->n_last);
}

MopacCResult mopacc_session_vibe(MopacCSession *session, int n_atoms,
                                 const double *positions_ang,
                                 const int *atomic_numbers, double *freq_cm,
                                 double *disp) {
  MopacCResult r;
  if (session == NULL) {
    memset(&r, 0, sizeof(r));
    snprintf(r.message, sizeof(r.message), "mopacc: null session");
    set_error(r.message);
    return r;
  }
  return run_job(&session->params, &session->cell, n_atoms, positions_ang,
                 atomic_numbers, &session->state, &session->zstate,
                 MOPACC_OP_VIBE, NULL, 0, NULL, freq_cm, disp,
                 &session->charges, &session->n_last);
}

static int copy_charges(int n_atoms, int n_last, const double *src,
                        double *out) {
  if (out == NULL || n_atoms <= 0 || src == NULL || n_last != n_atoms) {
    set_error("mopacc: no cached charges for this atom count");
    return -1;
  }
  memcpy(out, src, (size_t)n_atoms * sizeof(double));
  return 0;
}

int mopacc_last_charges(int n_atoms, double *charges) {
  return copy_charges(n_atoms, g_n_last, g_charges, charges);
}

int mopacc_session_charges(const MopacCSession *session, int n_atoms,
                           double *charges) {
  if (session == NULL) {
    set_error("mopacc_session_charges: null session");
    return -1;
  }
  return copy_charges(n_atoms, session->n_last, session->charges, charges);
}

const char *mopacc_version(void) { return MOPACC_VERSION_STRING; }
int mopacc_c_abi_version(void) { return RGPOT_MOPACC_C_ABI_VERSION; }
int mopacc_abi_version(void) { return MOPACC_ABI_VERSION; }
int mopacc_available(void) { return 1; }
const char *mopacc_last_error(void) { return g_last_error; }
void mopacc_finalize(void) {
  g_params_set = 0;
  memset(&g_cell, 0, sizeof(g_cell));
  free(g_charges);
  g_charges = NULL;
  g_n_last = 0;
}
