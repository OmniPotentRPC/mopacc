# mopacc

Split rgpot engine for OpenMOPAC `libmopac.so`. Same shape as
[nwchemc](https://github.com/OmniPotentRPC/nwchemc) and
[cpmdc](https://github.com/OmniPotentRPC/cpmdc): rgpot dlopens
`libmopacc.so`; this tree owns the C ABI.

This is the OpenMOPAC potential surface: every `mopac_system.model`
(PM7, PM6-D3H4, PM6-ORG, PM6, AM1, RM1), COSMO, lattice, SCF, geometry
relax, and vibrational evaluation, conventional or MOZYME. Omitted
params default the Hamiltonian to AM1. Do not grow NWChem or CPMD for
this. Do not wrap the `mopac` executable.

See [docs/rgpot-integration.md](docs/rgpot-integration.md) for the packed
`MopacCParams` contract, units, and `MOPACC_LIBRARY` /
`RGPOT_MOPACC_ENGINE` discovery. Params are **not Cap'n Proto**.

## Backend

| mopacc | OpenMOPAC (`mopac.h`, 23.x) |
| --- | --- |
| models | `mopac_system.model` 0–5 (default AM1) |
| SCF | `mopac_scf` / `mozyme_scf` |
| relax | `mopac_relax` / `mozyme_relax` |
| frequencies | `mopac_vibe` / `mozyme_vibe` |
| cell | `nlattice`, lattice vectors, pressure |
| solvent | COSMO `epsilon` (vacuum when 1) |
| session density | `mopac_state` / `mozyme_state` |
| properties | heat, gradient, dipole, charges, stress, freq |

ABI units: Angstrom in, Hartree and Hartree/Bohr out. Converted from
`mopac_properties.heat` (kcal/mol) and `coord_deriv` (kcal/mol/A).
Dipole is Debye. Stress is GPa Voigt.

Params are a packed `MopacCParams`. NULL / size 0 is charge 0, closed
shell, vacuum, conventional solver, AM1.

The public ABI does not expose C++ or Rust types:

```c
int mopacc_set_params(const void *params, size_t params_size_bytes);
MopacCResult mopacc_energy_gradient(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes, double *grad_h_bohr);
MopacCResult mopacc_energy(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes);
MopacCResult mopacc_energy_forces(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes, double *forces_h_bohr);
MopacCSession *mopacc_session_create(const void *params,
                                     size_t params_size_bytes);
void mopacc_session_destroy(MopacCSession *session);
int mopacc_session_set_params(MopacCSession *session, const void *params,
                              size_t params_size_bytes);
MopacCResult mopacc_session_energy_gradient(
    MopacCSession *session, int n_atoms, const double *positions_ang,
    const int *atomic_numbers, double *grad_h_bohr);
int mopacc_set_cell(const double *lattice_ang, int nlattice,
                    int nlattice_move, double pressure_gpa);
int mopacc_session_set_cell(MopacCSession *session,
                            const double *lattice_ang, int nlattice,
                            int nlattice_move, double pressure_gpa);
MopacCResult mopacc_relax(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes, double *coord_update_ang,
    double *grad_h_bohr);
MopacCResult mopacc_vibe(
    int n_atoms, const double *positions_ang, const int *atomic_numbers,
    const void *params, size_t params_size_bytes, double *freq_cm,
    double *disp);
MopacCResult mopacc_session_relax(
    MopacCSession *session, int n_atoms, const double *positions_ang,
    const int *atomic_numbers, double *coord_update_ang, double *grad_h_bohr);
MopacCResult mopacc_session_vibe(
    MopacCSession *session, int n_atoms, const double *positions_ang,
    const int *atomic_numbers, double *freq_cm, double *disp);
int mopacc_last_charges(int n_atoms, double *charges);
int mopacc_session_charges(const MopacCSession *session, int n_atoms,
                           double *charges);
const char *mopacc_version(void);
int mopacc_c_abi_version(void);
int mopacc_abi_version(void);
int mopacc_available(void);
const char *mopacc_last_error(void);
void mopacc_finalize(void);
```

## Build

Stub (no libmopac; `mopacc_available() == 0`):

```
meson setup build
meson compile -C build
meson test -C build
```

With Pixi, the same stub packaging check is `pixi run test-stub`.

Install header + `mopacc.pc` (`soversion` 1):

```
meson setup build --prefix "$PWD/prefix"
meson install -C build
export PKG_CONFIG_PATH="$PWD/prefix/lib/pkgconfig:$PWD/prefix/lib64/pkgconfig"
pkg-config --cflags --libs mopacc
```

Real embed, on the remote builder, against conda-forge `mopac` >= 23.2:

```
meson setup build-mopac -Dwith_mopac=true -Dmopac_root=$CONDA_PREFIX
meson compile -C build-mopac
./build-mopac/host_hcn_am1
```

Or `pixi run -e mopac test-mopac`.

Env: `MOPACC_LIBRARY` / `RGPOT_MOPACC_ENGINE` once an rgpot frontend
dlopens this `.so`. Required symbols: `mopacc_set_params`,
`mopacc_energy_gradient`, `mopacc_session_create`, `mopacc_session_destroy`,
`mopacc_session_set_params`, `mopacc_session_energy_gradient`,
`mopacc_available`, `mopacc_c_abi_version`.

## License

MIT for this wrapper. OpenMOPAC 23 is Apache-2.0.
