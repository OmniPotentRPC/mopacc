#!/usr/bin/env python3
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MESON = ROOT / "meson.build"
README = ROOT / "README.md"
RGPOT_GUIDE = ROOT / "docs" / "rgpot-integration.md"
PIXI = ROOT / "pixi.toml"
CONSUMER = ROOT / "tests" / "test_installed_pkgconfig_consumer.py"

PACKAGING_TESTS = [
    "readme-abi-surface",
    "meson-install-contract",
    "mopacc-installed-pkgconfig-consumer",
    "mopac-root-relative-configure",
]


class MesonInstallContractTest(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        self.text = MESON.read_text(encoding="utf-8")

    def test_public_header_is_installed(self):
        self.assertRegex(
            self.text,
            re.compile(
                r"install_headers\(\s*'include/mopacc\.h'\s*,\s*"
                r"subdir:\s*'mopacc'\s*,?\s*\)",
                re.S,
            ),
        )

    def test_pkg_config_file_exports_mopacc_library(self):
        self.assertIn("pkgconfig = import('pkgconfig')", self.text)
        self.assertRegex(
            self.text,
            re.compile(
                r"pkgconfig\.generate\(\s*"
                r"name:\s*'mopacc'\s*,.*"
                r"filebase:\s*'mopacc'\s*,.*"
                r"libraries:\s*libmopacc\s*,.*"
                r"subdirs:\s*'mopacc'\s*,.*"
                r"version:\s*meson\.project_version\(\)\s*,.*"
                r"description:\s*'Stable C ABI for embedding OpenMOPAC'"
                r"\s*,?\s*\)",
                re.S,
            ),
        )

    def test_shared_library_soversion_is_abi_two(self):
        self.assertRegex(self.text, r"soversion:\s*'2'")

    def test_project_version_tracks_abi_two_and_pixi(self):
        project = re.search(r"project\((.*?)\n\)", self.text, re.S)
        self.assertIsNotNone(project)
        version = re.search(r"version:\s*'([^']+)'", project.group(1))
        self.assertIsNotNone(version)
        self.assertEqual(version.group(1), "0.2.0")
        pixi = PIXI.read_text(encoding="utf-8")
        self.assertRegex(
            pixi, r'(?m)^version = "%s"$' % re.escape(version.group(1))
        )

    def test_stub_and_real_embed_stay_behind_with_mopac(self):
        self.assertIn("get_option('with_mopac')", self.text)
        self.assertIn("src/mopacc.c", self.text)
        self.assertIn("src/mopacc_stub.c", self.text)

    def test_packaging_tests_are_registered(self):
        registered = set(re.findall(r"test\(\s*'([^']+)'", self.text))
        missing = [name for name in PACKAGING_TESTS if name not in registered]
        self.assertEqual(missing, [])
        self.assertIn("tests/test_installed_pkgconfig_consumer.py", self.text)
        self.assertIn("tests/test_readme_abi_surface.py", self.text)
        self.assertIn("tests/test_meson_install_contract.py", self.text)
        self.assertIn("tests/test_relative_mopac_root.py", self.text)

    def test_installed_pkgconfig_consumer_exercises_rgpot_surface(self):
        consumer = CONSUMER.read_text(encoding="utf-8")
        required_terms = [
            "mopacc.h",
            "mopacc_energy_gradient",
            "mopacc_session_create",
            "mopacc_session_destroy",
            "mopacc_session_set_params",
            "mopacc_session_energy_gradient",
            "mopacc_available",
            "mopacc_c_abi_version",
            "mopacc_result_size",
            "mopacc_params_size",
            "pkg-config",
            "--cflags",
            "--libs",
            "PKG_CONFIG_PATH",
        ]
        missing = [term for term in required_terms if term not in consumer]
        self.assertEqual(missing, [])
        self.assertNotIn("PotentialConfig", consumer)
        self.assertNotIn("ForceInput", consumer)
        self.assertNotIn("nwchemc", consumer)

    def test_pixi_ships_packaging_toolchain_and_optional_mopac(self):
        pixi = PIXI.read_text(encoding="utf-8")
        required_terms = [
            'name = "mopacc"',
            'meson = ">=1.5.2,<2"',
            'ninja = ">=1.11,<2"',
            'pkgconf = ">=2.5.1,<3"',
            'compilers = ">=1.8.0,<2"',
            "[feature.mopac]",
            'mopac = ">=23.2,<24"',
        ]
        missing = [term for term in required_terms if term not in pixi]
        self.assertEqual(missing, [])
        self.assertIn("[feature.mopac]", pixi)
        deps = pixi.split("[dependencies]", 1)[1].split("[feature.mopac]", 1)[0]
        self.assertNotRegex(deps, r"(?m)^mopac\s*=")

    def test_docs_explain_installed_pkgconfig_and_packed_params(self):
        docs = README.read_text(encoding="utf-8") + "\n" + RGPOT_GUIDE.read_text(
            encoding="utf-8"
        )
        required_terms = [
            "PKG_CONFIG_PATH",
            "pkg-config --cflags --libs mopacc",
            "MopacCParams",
            "model=4",
            "Hartree",
            "Hartree/Bohr",
            "MOPACC_LIBRARY",
            "RGPOT_MOPACC_ENGINE",
            "not Cap'n Proto",
        ]
        missing = [term for term in required_terms if term not in docs]
        self.assertEqual(missing, [])

    def test_rgpot_guide_requires_exact_abi(self):
        guide = RGPOT_GUIDE.read_text(encoding="utf-8")
        self.assertIn("`mopacc_c_abi_version() == 2`", guide)
        self.assertNotRegex(guide, r"mopacc_c_abi_version\(\)\s*>=")


if __name__ == "__main__":
    raise SystemExit(unittest.main())
