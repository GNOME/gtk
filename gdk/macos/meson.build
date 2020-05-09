gdk_macos_sources = files([
  'gdkdisplaylinksource.c',
  'GdkMacosWindow.c',
  'gdkmacoscairocontext.c',
  'gdkmacoscursor.c',
  'gdkmacosdevice.c',
  'gdkmacosdisplay.c',
  'gdkmacosdisplay-settings.c',
  'gdkmacosdisplay-translate.c',
  'gdkmacosdragsurface.c',
  'gdkmacoseventsource.c',
  'gdkmacoskeymap.c',
  'gdkmacosmonitor.c',
  'gdkmacospopupsurface.c',
  'gdkmacosseat.c',
  'gdkmacossurface.c',
  'gdkmacostoplevelsurface.c',

  'GdkMacosBaseView.c',
  'GdkMacosCairoView.c',
  # 'GdkMacosGLView.c',
])

gdk_macos_public_headers = files([
  'gdkmacosdevice.h',
  'gdkmacosdisplay.h',
  'gdkmacoskeymap.h',
  'gdkmacosmonitor.h',
  'gdkmacossurface.h',
])

install_headers(gdk_macos_public_headers, 'gdkmacos.h', subdir: 'gtk-4.0/gdk/macos/')

gdk_macos_frameworks = [
  'AppKit',
  'Carbon',
  'CoreVideo',
  'CoreServices',
]

gdk_macos_deps = [
  dependency('appleframeworks', modules: gdk_macos_frameworks)
]

libgdk_c_args += ['-xobjective-c']

libgdk_macos = static_library('gdk-macos',
                              gdk_macos_sources, gdkconfig, gdkenum_h,
                              include_directories: [ confinc, gdkinc, ],
                              c_args: libgdk_c_args + common_cflags,
                              link_args: common_ldflags,
                              link_with: [],
                              dependencies: gdk_deps + gdk_macos_deps)
