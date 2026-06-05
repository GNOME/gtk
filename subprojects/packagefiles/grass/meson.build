project('grass', version: '0.13.4')

cargo = find_program('cargo', native: true)

message('Building grass sass compiler...')
run_command(
  cargo, 'build', '--release',
  '--manifest-path', meson.current_source_dir() / 'Cargo.toml',
  '--target-dir', meson.current_build_dir() / 'cargo-target',
  check: true,
)

if host_machine.system() == 'windows'
  grass_name = 'grass.exe'
else
  grass_name = 'grass'
endif

grass = find_program(
  meson.current_build_dir() / 'cargo-target' / 'release' / grass_name
)
meson.override_find_program('grass', grass)
