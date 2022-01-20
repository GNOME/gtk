
add_languages(['objc'])

gdk_quartz_sources = files(
  'GdkQuartzView.c',
  'GdkQuartzNSWindow.c',
  'gdkcursor-quartz.c',
  'gdkdevice-core-quartz.c',
  'gdkdevicemanager-core-quartz.c',
  'gdkdisplay-quartz.c',
  'gdkdisplaylinksource.c',
  'gdkdisplaymanager-quartz.c',
  'gdkdnd-quartz.c',
  'gdkevents-quartz.c',
  'gdkeventloop-quartz.c',
  'gdkglcontext-quartz.c',
  'gdkglobals-quartz.c',
  'gdkkeys-quartz.c',
  'gdkmonitor-quartz.c',
  'gdkproperty-quartz.c',
  'gdkscreen-quartz.c',
  'gdkselection-quartz.c',
  'gdktestutils-quartz.c',
  'gdkutils-quartz.c',
  'gdkvisual-quartz.c',
  'gdkwindow-quartz.c',
)

gdk_quartz_public_headers = files(
  'gdkquartz-cocoa-access.h',
  'gdkquartzcursor.h',
  'gdkquartzdevice-core.h',
  'gdkquartzdevicemanager-core.h',
  'gdkquartzdisplay.h',
  'gdkquartzdisplaymanager.h',
  'gdkquartzdnd.h',
  'gdkquartzkeys.h',
  'gdkquartzmonitor.h',
  'gdkquartzscreen.h',
  'gdkquartzutils.h',
  'gdkquartzvisual.h',
  'gdkquartzwindow.h',
)

install_headers(gdk_quartz_public_headers, subdir: 'gtk-3.0/gdk/quartz')
install_headers('gdkquartz.h', subdir: 'gtk-3.0/gdk')

core_graphics_dep = dependency('appleframeworks', modules : 'CoreGraphics', required : true)
appkit_dep = dependency('appleframeworks', modules : 'AppKit', required : true)
cocoa_dep = dependency('appleframeworks', modules : 'Cocoa', required : true)
carbon_dep = dependency('appleframeworks', modules : 'Carbon', required : true)
quartzcore_dep = dependency('appleframeworks', modules : 'QuartzCore', required : true)
iosurface_dep = dependency('appleframeworks', modules: 'IOSurface', required: true)

gdk_quartz_deps = [ core_graphics_dep, appkit_dep, cocoa_dep, carbon_dep, quartzcore_dep, iosurface_dep ]

libgdk_quartz = static_library('gdk-quartz',
  gdk_quartz_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"', '-xobjective-c'],
  dependencies: [gdk_deps, gdk_quartz_deps])
