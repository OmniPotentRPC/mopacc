#!/usr/bin/env python3
"""Install mopacc into a prefix, then compile a pkg-config consumer.

The consumer links the installed package and exercises the rgpot surface:
mopacc_energy_gradient, mopacc_session_*, mopacc_available. Params are a
packed MopacCParams blob, not Cap'n Proto.
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


CONSUMER_C = r"""
#include "mopacc.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  const char *version = mopacc_version();
  if (version == NULL || version[0] == '\0') {
    fprintf(stderr, "empty version\n");
    return 1;
  }
  if (mopacc_c_abi_version() != RGPOT_MOPACC_C_ABI_VERSION) {
    fprintf(stderr, "C ABI version mismatch\n");
    return 1;
  }
  if (mopacc_abi_version() != MOPACC_ABI_VERSION) {
    fprintf(stderr, "ABI version mismatch\n");
    return 1;
  }

  int available = mopacc_available();
  double pos[9] = {0.0};
  int z[3] = {1, 1, 1};
  double grad[9] = {0.0};
  MopacCParams params;
  memset(&params, 0, sizeof(params));
  params.model = MOPACC_MODEL_AM1;
  params.tolerance = 1.0;
  params.max_time = 1;

  MopacCResult energy = mopacc_energy_gradient(
      3, pos, z, &params, sizeof(params), grad);
  MopacCSession *session = mopacc_session_create(&params, sizeof(params));
  if (mopacc_session_set_params(NULL, &params, sizeof(params)) == 0) {
    fprintf(stderr, "null session_set_params accepted\n");
    mopacc_session_destroy(session);
    return 2;
  }
  MopacCResult session_grad = mopacc_session_energy_gradient(
      NULL, 3, pos, z, grad);
  if (session_grad.ok) {
    fprintf(stderr, "null session_energy_gradient succeeded\n");
    mopacc_session_destroy(session);
    return 2;
  }

  if (available) {
    if (session == NULL) {
      fprintf(stderr, "real embed refused a packed MopacCParams session\n");
      return 3;
    }
  } else if (session != NULL) {
    fprintf(stderr, "stub session_create must fail\n");
    mopacc_session_destroy(session);
    return 3;
  } else if (energy.ok) {
    fprintf(stderr, "stub energy_gradient must fail\n");
    return 3;
  }

  mopacc_session_destroy(session);
  mopacc_session_destroy(NULL);
  mopacc_finalize();
  printf("installed-consumer ok: %s available=%d\n", version, available);
  return 0;
}
"""


def run(cmd, **kw):
    print("+", shlex.join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], check=True, **kw)


def output(cmd, env=None):
    print("+", shlex.join(str(c) for c in cmd), flush=True)
    return subprocess.check_output(
        [str(c) for c in cmd], text=True, env=env
    ).strip()


def lib_dirs(prefix: Path):
    return [d for d in (prefix / "lib", prefix / "lib64") if d.is_dir()] + list(
        (prefix / "lib").glob("x86_64*")
    )


def pkg_config_env(prefix: Path):
    env = os.environ.copy()
    pkg_dirs = [d / "pkgconfig" for d in lib_dirs(prefix) if (d / "pkgconfig").is_dir()]
    if not pkg_dirs:
        raise SystemExit(f"no pkgconfig dir under {prefix}")
    existing = env.get("PKG_CONFIG_PATH", "")
    paths = [str(d) for d in pkg_dirs]
    if existing:
        paths.append(existing)
    env["PKG_CONFIG_PATH"] = os.pathsep.join(paths)
    return env


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--install-prefix", required=True)
    parser.add_argument("--meson", default="meson")
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--pkg-config", default="pkg-config")
    args = parser.parse_args()

    source = Path(args.source_dir).resolve()
    build_root = Path(args.build_root).resolve()
    prefix = Path(args.install_prefix).resolve()
    for stale in (build_root, prefix):
        if stale.exists():
            shutil.rmtree(stale)
    build_root.mkdir(parents=True)

    inner_build = build_root / "package-build"
    run(
        [
            args.meson,
            "setup",
            inner_build,
            source,
            f"--prefix={prefix}",
            "-Dwith_tests=false",
        ]
    )
    run([args.meson, "compile", "-C", inner_build])
    run([args.meson, "install", "-C", inner_build])

    so_links = list(prefix.rglob("libmopacc.so.1"))
    if not so_links:
        raise SystemExit("libmopacc.so.1 not installed (soversion 1)")
    if not list(prefix.rglob("mopacc.h")):
        raise SystemExit("mopacc.h not installed")
    if not list(prefix.rglob("mopacc.pc")):
        raise SystemExit("mopacc.pc not installed")

    env = pkg_config_env(prefix)
    output([args.pkg_config, "--modversion", "mopacc"], env=env)
    output([args.pkg_config, "--cflags", "--libs", "mopacc"], env=env)
    cflags = shlex.split(output([args.pkg_config, "--cflags", "mopacc"], env=env))
    libs = shlex.split(output([args.pkg_config, "--libs", "mopacc"], env=env))

    consumer_dir = build_root / "consumer"
    consumer_dir.mkdir()
    (consumer_dir / "main.c").write_text(CONSUMER_C, encoding="utf-8")
    consumer_bin = consumer_dir / "consumer"
    run([args.cc, consumer_dir / "main.c", "-o", consumer_bin] + cflags + libs)

    run_env = dict(os.environ)
    run_env["LD_LIBRARY_PATH"] = os.pathsep.join(
        [str(d) for d in lib_dirs(prefix)] + [run_env.get("LD_LIBRARY_PATH", "")]
    )
    run([consumer_bin], env=run_env)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
