#!/usr/bin/env python3
"""Configure -Dwith_mopac=true with relative and dotted -Dmopac_root values.

A stand-in prefix (an empty mopac.h and an empty libmopac.so) lives outside
the source tree. Meson is run from an unrelated working directory, so the
relative spelling only resolves if the option is taken from the source root.
Configure only; nothing is compiled against the stand-in.
"""
import argparse
import os
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(cmd, **kw):
    print("+", shlex.join(str(c) for c in cmd), flush=True)
    proc = subprocess.run(
        [str(c) for c in cmd],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        **kw,
    )
    if proc.returncode != 0:
        print(proc.stdout, flush=True)
        raise SystemExit(f"exit status {proc.returncode}")
    return proc.stdout


def fake_prefix(root: Path, cc: str) -> Path:
    prefix = root / "nested" / "mopac-prefix"
    (prefix / "include").mkdir(parents=True)
    (prefix / "lib").mkdir()
    (prefix / "include" / "mopac.h").write_text("", encoding="utf-8")
    empty_c = root / "empty.c"
    empty_c.write_text("int mopacc_fake_libmopac;\n", encoding="utf-8")
    run([cc, "-shared", "-fPIC", empty_c, "-o", prefix / "lib" / "libmopac.so"])
    return prefix


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--meson", default="meson")
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    args = parser.parse_args()

    source = Path(args.source_dir).resolve()
    work = Path(tempfile.mkdtemp(prefix="mopacc-relroot-")).resolve()
    try:
        prefix = fake_prefix(work, args.cc)
        if prefix.is_relative_to(source):
            raise SystemExit(f"temporary prefix {prefix} is inside {source}")
        cwd = work / "elsewhere"
        cwd.mkdir()
        spellings = {
            "relative": os.path.relpath(prefix, source),
            "relative-dotted": os.path.relpath(prefix.parent, source)
            + "/../nested/./mopac-prefix",
            "absolute-dotted": f"{prefix.parent}/../nested/mopac-prefix",
            "absolute": str(prefix),
        }
        for name, value in spellings.items():
            out = run(
                [
                    args.meson,
                    "setup",
                    work / f"build-{name}",
                    source,
                    "-Dwith_mopac=true",
                    "-Dwith_tests=false",
                    f"-Dmopac_root={value}",
                ],
                cwd=cwd,
            )
            lines = [line.strip() for line in out.splitlines()]
            resolved = ""
            if "OpenMOPAC" in lines:
                after = lines[lines.index("OpenMOPAC") + 1]
                resolved = after.partition("mopac_root:")[2].strip()
            if resolved != str(prefix):
                raise SystemExit(
                    f"{name}: -Dmopac_root={value} did not resolve to {prefix}"
                )
        print("relative mopac_root ok")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
