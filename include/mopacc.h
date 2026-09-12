/**
 * @file mopacc.h
 * @brief C ABI between an rgpot MOPACPot frontend and libmopacc.so.
 *
 * Hamiltonian is AM1 (Dewar NDDO). Backend is OpenMOPAC libmopac
 * (include/mopac.h): mopac_scf with mopac_system.model = 4.
 *
 * OpenMOPAC reports heat of formation in kcal/mol and Cartesian
 * derivatives in kcal/mol/Angstrom. This ABI reports Hartree and
 * Hartree/Bohr, same units as nwchemc.h.
 *
 * Geometry stays on the argument list (Angstrom). Method knobs are a
 * packed MopacCParams blob, or NULL for AM1 vacuum defaults. There is
 * no Cap'n Proto arm yet.
 *
 * Not thread-safe: OpenMOPAC's API is process-global.
 */
#pragma once

#include <stddef.h>

#define RGPOT_MOPACC_C_ABI_VERSION 1
#define MOPACC_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

#if defined(RGPOT_MOPACC_BUILD) && (defined(_WIN32) || defined(_WIN64))
#define RGPOT_MOPACC_API __declspec(dllexport)
#else
#define RGPOT_MOPACC_API
#endif

/** OpenMOPAC model index. AM1 is the default Hamiltonian. */
#define MOPACC_MODEL_PM7 0
#define MOPACC_MODEL_PM6_D3H4 1
#define MOPACC_MODEL_PM6_ORG 2
#define MOPACC_MODEL_PM6 3
#define MOPACC_MODEL_AM1 4
#define MOPACC_MODEL_RM1 5

/**
 * Sticky method options. Pass as the params buffer, or omit the buffer
 * to get AM1 / charge 0 / closed shell / vacuum.
 */
typedef struct MopacCParams {
  int charge;      /**< net charge */
  int spin;        /**< OpenMOPAC spin excitations */
  int model;       /**< 4 = AM1; 0 here still means AM1 when size is 0 */
  double tolerance; /**< GNORM/RELSCF scale; 1.0 is the OpenMOPAC default */
  int max_time;    /**< seconds; 0 uses 3600 */
} MopacCParams;

typedef struct MopacCResult {
  int ok;            /**< 1 success, 0 failure */
  double energy_h;   /**< heat of formation converted to Hartree */
  char message[512];
} MopacCResult;

typedef struct MopacCSession MopacCSession;

RGPOT_MOPACC_API int mopacc_set_params(const void *params,
                                       size_t params_size_bytes);

RGPOT_MOPACC_API MopacCResult mopacc_energy_gradient(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes, double *grad_h_bohr);

RGPOT_MOPACC_API MopacCResult mopacc_energy(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes);

RGPOT_MOPACC_API MopacCResult mopacc_energy_forces(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes, double *forces_h_bohr);

RGPOT_MOPACC_API MopacCSession *mopacc_session_create(const void *params,
                                                      size_t params_size_bytes);

RGPOT_MOPACC_API void mopacc_session_destroy(MopacCSession *session);

RGPOT_MOPACC_API int mopacc_session_set_params(MopacCSession *session,
                                               const void *params,
                                               size_t params_size_bytes);

RGPOT_MOPACC_API MopacCResult mopacc_session_energy_gradient(
    MopacCSession *session, int n_atoms, const double *positions_ang,
    const int *atomic_numbers, double *grad_h_bohr);

RGPOT_MOPACC_API const char *mopacc_version(void);
RGPOT_MOPACC_API int mopacc_c_abi_version(void);
RGPOT_MOPACC_API int mopacc_abi_version(void);
RGPOT_MOPACC_API int mopacc_available(void);
RGPOT_MOPACC_API const char *mopacc_last_error(void);
RGPOT_MOPACC_API void mopacc_finalize(void);

#ifdef __cplusplus
}
#endif
