# rgpot integration

mopacc is the AM1 engine. It dlopens as `libmopacc.so`. It is not
an NWChem theory token and it does not use Cap'n Proto.

## ABI

| Symbol | Role |
| --- | --- |
| `mopacc_set_params` | packed `MopacCParams` or NULL for AM1 defaults |
| `mopacc_energy_gradient` | energy (Hartree) and gradient (Hartree/Bohr) |
| `mopacc_energy` | energy only |
| `mopacc_energy_forces` | negated gradient |
| `mopacc_session_create` / `_destroy` / `_energy_gradient` | reuse `mopac_state` |
| `mopacc_available` | 0 stub, 1 OpenMOPAC embed |
| `mopacc_c_abi_version` | 1 |

`MopacCParams`: charge, spin (OpenMOPAC excitations), model (4 = AM1),
tolerance, max_time.

Geometry in Ångström. Heat of formation converted from kcal/mol.

Env: `MOPACC_LIBRARY` or `RGPOT_MOPACC_ENGINE` → `libmopacc.so`.
Link OpenMOPAC with `-Dwith_mopac=true -Dmopac_root=$CONDA_PREFIX`.
