#!/usr/bin/env python3
import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = Path(os.environ.get("MOPACC_HEADER_PATH", ROOT / "include" / "mopacc.h"))
README = Path(os.environ.get("MOPACC_README_PATH", ROOT / "README.md"))
RGPOT_GUIDE = Path(
    os.environ.get("MOPACC_RGPOT_GUIDE_PATH", ROOT / "docs" / "rgpot-integration.md")
)

DECL_RE = re.compile(
    r"""
    (?:^|\n)\s*
    (?:
      MopacCResult\s+|
      MopacCSession\s+\*\s*|
      const\s+char\s+\*\s*|
      int\s+|
      void\s+
    )
    (mopacc_[A-Za-z0-9_]+)\s*\(
    """,
    re.VERBOSE,
)

REQUIRED_SURFACE = (
    "mopacc_energy_gradient",
    "mopacc_session_create",
    "mopacc_session_destroy",
    "mopacc_session_set_params",
    "mopacc_session_energy_gradient",
    "mopacc_available",
)


def unique_in_order(names):
    seen = set()
    result = []
    for name in names:
        if name in seen:
            continue
        seen.add(name)
        result.append(name)
    return result


def declared_abi_names(text):
    return unique_in_order(DECL_RE.findall(text))


def readme_abi_block(text):
    match = re.search(r"```c\n(?P<body>.*?)\n```", text, re.S)
    if not match:
        raise AssertionError("README.md does not contain a C ABI code block")
    return match.group("body")


class ReadmeAbiSurfaceTest(unittest.TestCase):
    maxDiff = None

    def test_readme_c_block_matches_header_exports(self):
        header_names = declared_abi_names(HEADER.read_text(encoding="utf-8"))
        readme_names = declared_abi_names(
            readme_abi_block(README.read_text(encoding="utf-8"))
        )

        missing = sorted(set(header_names) - set(readme_names))
        extra = sorted(set(readme_names) - set(header_names))
        self.assertEqual(missing, [])
        self.assertEqual(extra, [])
        self.assertEqual(readme_names, header_names)

    def test_required_rgpot_symbols_are_exported(self):
        header_names = set(declared_abi_names(HEADER.read_text(encoding="utf-8")))
        missing = [name for name in REQUIRED_SURFACE if name not in header_names]
        self.assertEqual(missing, [])

        readme_names = set(
            declared_abi_names(readme_abi_block(README.read_text(encoding="utf-8")))
        )
        missing_readme = [name for name in REQUIRED_SURFACE if name not in readme_names]
        self.assertEqual(missing_readme, [])

    def test_rgpot_integration_guide_covers_packed_params_contract(self):
        readme = README.read_text(encoding="utf-8")
        self.assertIn("docs/rgpot-integration.md", readme)
        self.assertTrue(RGPOT_GUIDE.exists())

        guide = RGPOT_GUIDE.read_text(encoding="utf-8")
        required_terms = [
            "MopacCParams",
            "model=4",
            "AM1",
            "Hartree",
            "Hartree/Bohr",
            "MOPACC_LIBRARY",
            "RGPOT_MOPACC_ENGINE",
            "mopacc_energy_gradient",
            "mopacc_session_create",
            "mopacc_session_destroy",
            "mopacc_session_set_params",
            "mopacc_session_energy_gradient",
            "mopacc_available",
            "not Cap'n Proto",
        ]
        for term in required_terms:
            with self.subTest(term=term):
                self.assertIn(term, guide)
        self.assertNotIn("PotentialConfig.nwchem", guide)
        self.assertNotIn("ForceInput", guide)
        self.assertNotIn("TCE", guide)
        self.assertNotIn("NWPW", guide)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
