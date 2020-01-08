gdk_tests_sources = files(
  'gdkunittestmain.c',
  'gdkinterputils.c',
  'gdkeventaccessors.c',
  'gdkeventinterp.c',
)

libgdk_unit_tests = static_library('gdk-3-unit-tests',
                                   gdk_tests_sources, gdkconfig, gdkenum_h,
                                   include_directories: [ confinc, gdkinc, ],
                                   c_args: [
                                     '-DGDK_COMPILATION',
                                     '-DG_LOG_DOMAIN="Gdk"',
                                   ] + common_cflags,
                                   link_args: common_ldflags,
                                   dependencies: [ gdk_deps, ])

gdk_unit_tests = executable('gdk-3-unit-tests',
                            sources: [gdk_sources, gdk_backends_gen_headers, gdkconfig],
                            dependencies: gdk_deps,
                            include_directories: [confinc, gdkx11_inc, wlinc],
                            c_args: [
                              '-DGDK_COMPILATION',
                              '-DG_LOG_DOMAIN="Gdk"',
                            ] + common_cflags,
                            link_whole: [gdk_backends, libgdk_unit_tests],
                            link_args: gdk_link_args,
                            install: get_option('installed_tests'))

test('gdk-3-unit-tests', gdk_unit_tests,
     args: [ '--tap', '-k' ],
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GTK_CSD=1',
            'G_ENABLE_DIAGNOSTIC=0',
            'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
            'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
          ],
     suite: 'gdk')
