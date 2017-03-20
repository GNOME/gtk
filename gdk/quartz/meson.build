# FIXME: do we need to do an add_language() for objective-c here?
gdk_quartz_sources = files([
  'GdkQuartzView.c',
  'GdkQuartzNSWindow.c',
  'gdkcursor-quartz.c',
  'gdkdevice-core-quartz.c',
  'gdkdevicemanager-core-quartz.c',
  'gdkdisplay-quartz.c',
  'gdkdisplaymanager-quartz.c',
  'gdkdnd-quartz.c',
  'gdkevents-quartz.c',
  'gdkeventloop-quartz.c',
  'gdkglcontext-quartz.c',
  'gdkglobals-quartz.c',
  'gdkkeys-quartz.c',
  'gdkproperty-quartz.c',
  'gdkscreen-quartz.c',
  'gdkselection-quartz.c',
  'gdkutils-quartz.c',
  'gdkwindow-quartz.c',
])

gdk_quartz_public_headers = files([
  'gdkquartzcursor.h',
  'gdkquartzdevice-core.h',
  'gdkquartzdevicemanager-core.h',
  'gdkquartzdisplay.h',
  'gdkquartzdisplaymanager.h',
  'gdkquartzdnd.h',
  'gdkquartzkeys.h',
  'gdkquartzscreen.h',
  'gdkquartzutils.h',
  'gdkquartzwindow.h',
])

install_headers(gdk_quartz_public_headers, subdir: 'gtk-4.0/gdk/quartz/')
install_headers('gdkquartz.h', subdir: 'gtk-4.0/gdk/')

gdk_quartz_deps = [ # FIXME
]

libgdk_quartz = static_library('libgdk-quartz',
  gdk_quartz_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"', '-xobjective-c'],
  dependencies: [gdk_deps, gdk_quartz_deps])
