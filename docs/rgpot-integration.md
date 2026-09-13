# rgpot integration guide

This is the C ABI contract rgpot should wire against for the OpenMOPAC AM1
backend. The stable boundary is a packed `MopacCParams` blob, not Cap'n Proto
and not a MOPAC input deck.

AM1 is the default Hamiltonian (`mopac_system.model = 4` / `mopac_scf`).
Do not grow an NWChem or CPMD engine for this path.

## Wiring stage

rgpot dlopens the installed `libmopacc.so` and calls the packed-params ABI
directly:

1. Discover the library through `MOPACC_LIBRARY` or `RGPOT_MOPACC_ENGINE`.
2. Confirm `mopacc_available()` and `mopacc_c_abi_version()`.
3. Pass method knobs as a packed `MopacCParams` (charge, spin, `model=4` AM1).
4. Call `mopacc_energy_gradient()` for a one-shot step, or keep a
   `MopacCSession` for repeated geometries.

Use this as the merge/pr decision point:

1. rgpot links the installed package through pkg-config (`PKG_CONFIG_PATH` and
   `pkg-config --cflags --libs mopacc`).
2. rgpot sends a packed `MopacCParams`, not a Cap'n Proto message.
3. rgpot reads Hartree / Hartree/Bohr results from `MopacCResult` and the
   gradient buffer.

## Params

`MopacCParams` is a POD copied by size. NULL / size 0 means AM1, charge 0,
closed shell, vacuum.

| Field | Meaning |
| --- | --- |
| `charge` | net charge |
| `spin` | OpenMOPAC spin excitations |
| `model` | `4` = AM1 (`MOPACC_MODEL_AM1`) |
| `tolerance` | GNORM/RELSCF scale; `1.0` is the OpenMOPAC default |
| `max_time` | seconds; `0` uses 3600 |

A zeroed struct still means AM1 only when the caller writes `model=4`. Do not
treat `model=0` as AM1 once a full blob is supplied (`0` is PM7 in OpenMOPAC).

```c
MopacCParams params = {
    .charge = 0,
    .spin = 0,
    .model = MOPACC_MODEL_AM1, /* 4 */
    .tolerance = 1.0,
    .max_time = 3600,
};

MopacCSession *session = mopacc_session_create(&params, sizeof(params));
MopacCResult status = mopacc_session_energy_gradient(
    session, n_atoms, positions_ang, atomic_numbers, grad_h_bohr);
mopacc_session_destroy(session);
```

`mopacc_energy_gradient()` is the matching one-shot path. Session reuse keeps
OpenMOPAC `mopac_state` across calls. The process-global OpenMOPAC API is not
thread-safe.

## Units

Geometry stays on the argument list. Results are Hartree / Hartree/Bohr, same
units as `nwchemc.h`. OpenMOPAC `heat` (kcal/mol) and `coord_deriv`
(kcal/mol/Angstrom) are converted inside `libmopacc`.

| Field | Unit |
| --- | --- |
| input positions | Angstrom |
| `MopacCResult.energy_h` | Hartree |
| `grad_h_bohr` | Hartree/Bohr |
| `forces_h_bohr` | Hartree/Bohr, negative of the nuclear derivative |

## Environment

| Variable | Role |
| --- | --- |
| `MOPACC_LIBRARY` | path to `libmopacc.so` for an rgpot frontend dlopen |
| `RGPOT_MOPACC_ENGINE` | alternate engine path used by the rgpot MOPACPot loader |

Required symbols after dlopen:

- `mopacc_energy_gradient`
- `mopacc_session_create`
- `mopacc_session_destroy`
- `mopacc_session_set_params`
- `mopacc_session_energy_gradient`
- `mopacc_available`
- `mopacc_set_params`
- `mopacc_c_abi_version`

## Stub vs real embed

| Configure | `mopacc_available()` | Backend |
| --- | --- | --- |
| default / `-Dwith_mopac=false` | `0` | `src/mopacc_stub.c` |
| `-Dwith_mopac=true -Dmopac_root=...` | `1` | `src/mopacc.c` + OpenMOPAC `mopac_scf` |

The installed package smoke `mopacc-installed-pkgconfig-consumer` compiles and
links the packed-params ABI against the stub install, then runs invalid-input
checks on `mopacc_energy_gradient` and `mopacc_session_*`.

## Installed package

```sh
meson setup build --prefix "$PWD/prefix"
meson install -C build
export PKG_CONFIG_PATH="$PWD/prefix/lib/pkgconfig:$PWD/prefix/lib64/pkgconfig"
pkg-config --cflags --libs mopacc
```

With Pixi, the stub packaging check is `pixi run test-stub`. The real AM1 embed
is the optional `mopac` feature (`pixi run -e mopac test-mopac`) against
conda-forge OpenMOPAC >= 23.2.
