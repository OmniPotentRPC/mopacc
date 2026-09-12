#!/bin/bash
#SBATCH --job-name=mopacc-am1
#SBATCH --partition=cpu
#SBATCH --time=00:40:00
#SBATCH --cpus-per-task=4
#SBATCH --mem=8G
#SBATCH --output=build-mopac/slurm-%j.out
#SBATCH --error=build-mopac/slurm-%j.err
set -euo pipefail
export PATH="${HOME}/.local/bin:${PATH}"
export SLURM_CONF=/etc/slurm-llnl/slurm.conf
ROOT="${HOME}/Git/Github/OmniPotentRPC/mopacc"
cd "$ROOT"
mkdir -p build-mopac
bash scripts/terra_build_am1.sh
