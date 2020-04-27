gdk_macos_sources = files([
  'GdkMacosWindow.c',
  'gdkmacoscairocontext.c',
  'gdkmacosdevice.c',
  'gdkmacosdisplay.c',
  'gdkmacosdragsurface.c',
  'gdkmacoseventsource.c',
  'gdkmacoskeymap.c',
  'gdkmacosmonitor.c',
  'gdkmacospopupsurface.c',
  'gdkmacosseat.c',
  'gdkmacossurface.c',
  'gdkmacostoplevelsurface.c',

  # 'GdkMacosView.c',
])

gdk_macos_public_headers = files([
  'gdkmacosdevice.h',
  'gdkmacosdisplay.h',
  'gdkmacoskeymap.h',
  'gdkmacosmonitor.h',
  'gdkmacossurface.h',
])

install_headers(gdk_macos_public_headers, 'gdkmacos.h', subdir: 'gtk-4.0/gdk/macos/')

gdk_macos_deps = [
]

libgdk_macos = static_library('gdk-macos',
                                gdk_macos_sources, gdkconfig, gdkenum_h,
                                include_directories: [ confinc, gdkinc, ],
                                c_args: [
                                  '-DGTK_COMPILATION',
                                  '-DG_LOG_DOMAIN="Gdk"',
                                  '-xobjective-c',
                                ] + common_cflags,
                                link_args: common_ldflags,
                                link_with: [],
                                dependencies: [ gdk_deps, gdk_macos_deps])
