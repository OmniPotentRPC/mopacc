project(
  'mopacc',
  'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: ['warning_level=2', 'c_std=c11'],
)

cc = meson.get_compiler('c')
python = find_program('python3')
pkgconfig = import('pkgconfig')
inc = include_directories('include')
math_dep = cc.find_library('m', required: false)

common_c_args = ['-DMOPACC_VERSION_STRING="' + meson.project_version() + '"']
if cc.get_id() != 'msvc'
  common_c_args += ['-Wall', '-Wextra', '-Wpedantic']
endif

fs = import('fs')
mopac_root = get_option('mopac_root')
if mopac_root == ''
  mopac_root = run_command(
    'sh',
    '-c',
    'printf %s "${MOPAC_ROOT:-${CONDA_PREFIX:-}}"',
    check: true,
  ).stdout().strip()
endif

has_mopac = get_option('with_mopac')
mopac_inc = []
mopac_lib = []
if has_mopac
  if mopac_root == ''
    error('with_mopac requires -Dmopac_root= or MOPAC_ROOT/CONDA_PREFIX')
  endif
  if not fs.is_file(mopac_root / 'include' / 'mopac.h')
    error('mopac.h not found under ' + mopac_root + '/include')
  endif
  mopac_inc = include_directories(mopac_root / 'include')
  mopac_lib = cc.find_library(
    'mopac',
    dirs: [mopac_root / 'lib', mopac_root / 'lib64'],
    required: true,
  )
endif

if has_mopac
  lib_sources = ['src/mopacc.c']
  lib_inc = [inc, mopac_inc]
  lib_deps = [math_dep, mopac_lib]
else
  lib_sources = ['src/mopacc_stub.c']
  lib_inc = [inc]
  lib_deps = [math_dep]
endif

libmopacc = library(
  'mopacc',
  lib_sources,
  include_directories: lib_inc,
  dependencies: lib_deps,
  c_args: common_c_args,
  install: true,
  version: meson.project_version(),
)

mopacc_dep = declare_dependency(
  link_with: libmopacc,
  include_directories: inc,
)

install_headers('include/mopacc.h')

pkg = import('pkgconfig')
pkg.generate(
  libmopacc,
  name: 'mopacc',
  description: 'C ABI for OpenMOPAC AM1 (mopac_scf, model 4)',
  version: meson.project_version(),
)

if get_option('with_tests')
  test_stub = executable(
    'test_stub_abi',
    'tests/test_stub_abi.c',
    include_directories: inc,
    link_with: libmopacc,
    c_args: common_c_args,
  )
  test('stub_abi', test_stub)

  test_energy_gradient = executable(
    'test_mopacc_energy_gradient',
    'tests/test_mopacc_energy_gradient.c',
    include_directories: inc,
    link_with: libmopacc,
    dependencies: [math_dep],
    c_args: common_c_args,
  )
  test('energy_gradient', test_energy_gradient)

  test_session = executable(
    'test_mopacc_session',
    'tests/test_mopacc_session.c',
    include_directories: inc,
    link_with: libmopacc,
    dependencies: [math_dep],
    c_args: common_c_args,
  )
  test('session', test_session)

  host_hcn = executable(
    'host_hcn_am1',
    'examples/host_hcn_am1.c',
    include_directories: inc,
    link_with: libmopacc,
    c_args: common_c_args,
  )
endif
