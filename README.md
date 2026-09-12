# mopacc

Split rgpot engine for **AM1** via OpenMOPAC `libmopac.so`. Same shape as
[nwchemc](https://github.com/OmniPotentRPC/nwchemc) and
[cpmdc](https://github.com/OmniPotentRPC/cpmdc): rgpot dlopens
`libmopacc.so`; this tree owns the C ABI.

AM1 is not an NWChem or CPMD Hamiltonian. Do not grow those engines.
XTB in rgpot is GFN, not AM1. Do not wrap the `mopac` executable.

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

## Build

Stub (no libmopac; `mopacc_available() == 0`):

```
meson setup build
meson compile -C build
meson test -C build
```

Real embed, on the remote builder, against conda-forge `mopac` >= 23.2:

```
meson setup build-mopac -Dwith_mopac=true -Dmopac_root=$CONDA_PREFIX
meson compile -C build-mopac
./build-mopac/host_hcn_am1
```

Env: `MOPACC_LIBRARY` / `RGPOT_MOPACC_ENGINE` once an rgpot frontend
dlopens this `.so`. Required symbols: `mopacc_set_params`,
`mopacc_energy_gradient`, `mopacc_available`, `mopacc_c_abi_version`.

## License

MIT for this wrapper. OpenMOPAC 23 is Apache-2.0.
