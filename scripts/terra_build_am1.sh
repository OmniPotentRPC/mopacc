#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build libmopacc against conda-forge OpenMOPAC on the remote builder.
# Not an ASE MOPAC path. Does not compile on the laptop.
set -euo pipefail
export PATH="${HOME}/.local/bin:${PATH}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCRATCH="${MOPACC_SCRATCH:-${HOME}/var/scratch/2026-09/mopacc-env}"
BUILD="${MOPACC_BUILD:-${ROOT}/build-mopac}"

if ! command -v micromamba >/dev/null 2>&1; then
  echo "micromamba not on PATH" >&2
  exit 2
fi

if [[ ! -x "${SCRATCH}/bin/meson" ]]; then
  micromamba create -y -p "${SCRATCH}" -c conda-forge \
    mopac meson ninja pkg-config compilers
fi
# shellcheck disable=SC1091
eval "$(micromamba shell hook -s bash)"
micromamba activate "${SCRATCH}"

meson setup "${BUILD}" \
  -Dwith_mopac=true \
  -Dmopac_root="${CONDA_PREFIX}" \
  --wipe 2>/dev/null || meson setup "${BUILD}" \
  -Dwith_mopac=true \
  -Dmopac_root="${CONDA_PREFIX}"
meson compile -C "${BUILD}"
meson test -C "${BUILD}" --print-errorlogs
"${BUILD}/host_hcn_am1"
