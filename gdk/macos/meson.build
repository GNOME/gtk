gdk_macos_public_sources = files([
  'gdkmacosdevice.c',
  'gdkmacosdisplay.c',
  'gdkmacosglcontext.c',
  'gdkmacoskeymap.c',
  'gdkmacosmonitor.c',
  'gdkmacossurface.c',
])

gdk_macos_sources = gdk_macos_public_sources + [
  'edgesnapping.c',

  'gdkdisplaylinksource.c',
  'gdkmacosbuffer.c',
  'gdkmacoscairocontext.c',
  'gdkmacosclipboard.c',
  'gdkmacoscursor.c',
  'gdkmacosdisplay-feedback.c',
  'gdkmacosdisplay-settings.c',
  'gdkmacosdisplay-translate.c',
  'gdkmacosdisplay-wm.c',
  'gdkmacosdrag.c',
  'gdkmacosdrop.c',
  'gdkmacosdragsurface.c',
  'gdkmacoseventsource.c',
  'gdkmacospopupsurface.c',
  'gdkmacosseat.c',
  'gdkmacostoplevelsurface.c',
  'GdkMacosBaseView.c',
  'GdkMacosLayer.c',
  'GdkMacosTile.c',
  'GdkMacosView.c',
  'GdkMacosWindow.c',
]

gdk_macos_public_headers = files([
  'gdkmacosdevice.h',
  'gdkmacosdisplay.h',
  'gdkmacosglcontext.h',
  'gdkmacoskeymap.h',
  'gdkmacosmonitor.h',
  'gdkmacosseat.h',
  'gdkmacossurface.h',
])

install_headers(gdk_macos_public_headers, 'gdkmacos.h', subdir: 'gtk-4.0/gdk/macos/')

gdk_macos_frameworks = [
  'AppKit',
  'Carbon',
  'CoreVideo',
  'CoreServices',
  'Foundation',
  'IOSurface',
  'OpenGL',
  'QuartzCore',
]

gdk_macos_deps = [
  dependency('appleframeworks', modules: gdk_macos_frameworks)
]

libgdk_c_args += ['-xobjective-c']

libgdk_macos = static_library('gdk-macos',
  gdk_macos_sources, gdkconfig, gdkenum_h,
  include_directories: [ confinc, gdkinc, ],
  c_args: [ libgdk_c_args, common_cflags, ],
  link_with: [],
  dependencies: [ gdk_deps, gdk_macos_deps, ],
)
