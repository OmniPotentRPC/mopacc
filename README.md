# mopacc

Split rgpot engine for **AM1** via OpenMOPAC `libmopac.so`. Same shape as
[nwchemc](https://github.com/OmniPotentRPC/nwchemc) and
[cpmdc](https://github.com/OmniPotentRPC/cpmdc): rgpot dlopens
`libmopacc.so`; this tree owns the C ABI.

AM1 is not an NWChem or CPMD Hamiltonian. Do not grow those engines.
XTB in rgpot is GFN, not AM1. Do not wrap the `mopac` executable.

See [docs/rgpot-integration.md](docs/rgpot-integration.md) for the packed
`MopacCParams` contract, Hartree / Hartree/Bohr units, and
`MOPACC_LIBRARY` / `RGPOT_MOPACC_ENGINE` discovery. Params are **not Cap'n
Proto**.

## Backend

| mopacc | OpenMOPAC (`mopac.h`, 23.x) |
| --- | --- |
| default Hamiltonian | `mopac_system.model = 4` (AM1) |
| energy + gradient | `mopac_scf` |
| session density | `mopac_state` reused across calls |
| free properties | `destroy_mopac_properties` |

ABI units: Angstrom in, Hartree and Hartree/Bohr out. Converted from
`mopac_properties.heat` (kcal/mol) and `coord_deriv` (kcal/mol/A).

Params are a packed `MopacCParams` (charge, spin, model, tolerance,
max_time). NULL / size 0 is AM1, charge 0, closed shell, vacuum.

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
