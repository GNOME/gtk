gdk_mir_sources = files([
  'gdkmircursor.c',
  'gdkmirdevicemanager.c',
  'gdkmirdisplay.c',
  'gdkmireventsource.c',
  'gdkmirglcontext.c',
  'gdkmirkeyboard.c',
  'gdkmirkeymap.c',
  'gdkmirpointer.c',
  'gdkmirscreen.c',
  'gdkmirwindow.c',
  'gdkmirwindowimpl.c',
  'gdkmir-debug.c',
])

#gdk_mir_public_headers = []
#install_headers(gdk_mir_public_headers, subdir: 'gtk-4.0/gdk/mir/')

install_headers('gdkmir.h', subdir: 'gtk-4.0/gdk/')

gdk_mir_deps = [
  mirclient_dep,
  mircookie_dep,
  libcontent_hub_glib_dep,
]

libgdk_mir = static_library('gdk-mir',
  gdk_mir_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"'],
  dependencies: [gdk_deps, gdk_mir_deps])
