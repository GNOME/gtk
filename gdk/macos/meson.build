gdk_macos_sources = files([
  # 'gdkapplaunchcontext-macos.c',
  # 'gdkcairocontext-macos.c',
  # 'gdkclipboard-macos.c',
  # 'gdkcursor-macos.c',
  # 'gdkdevice-macos.c',
  'gdkdisplay-macos.c',
  # 'gdkdrag-macos.c',
  # 'gdkdrop-macos.c',
  # 'gdkeventsource.c',
  # 'gdkglcontext-macos.c',
  # 'gdkkeys-macos.c',
  # 'gdkmonitor-macos.c',
  # 'gdkprimary-macos.c',
  # 'gdksurface-macos.c',
])

gdk_macos_public_headers = files([
  # 'gdkmacosdevice.h',
  'gdkmacosdisplay.h',
  # 'gdkmacosglcontext.h',
  # 'gdkmacosmonitor.h',
  # 'gdkmacossurface.h'
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
