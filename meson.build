# SPDX-FileCopyrightText: 2021 GNOME Foundation
# SPDX-License-Identifier: LGPL-2.1-or-later

project('gtk-docs',
  version: '1.0',
  meson_version: '>= 0.55.0',
  license: 'LGPL-2.1-or-later',
)

gidocgen_dep = dependency('gi-docgen', version: '>= 2021.6',
                          fallback: ['gi-docgen', 'dummy_dep'],
                          required: true)

toml_conf = configuration_data()
toml_conf.set('VERSION', meson.project_version())

gidocgen = find_program('gi-docgen', required: true)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
  '--fatal-warnings',
]

subdir('glib/glib')
subdir('glib/gmodule')
subdir('glib/gobject')
subdir('glib/gio')

subdir('atk/atk')
subdir('atk/atspi2')

subdir('gtk3/gdk')
subdir('gtk3/gtk')
