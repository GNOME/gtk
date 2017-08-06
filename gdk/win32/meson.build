gdk_win32_sources = files([
  'gdkcursor-win32.c',
  'gdkdevicemanager-win32.c',
  'gdkdevice-virtual.c',
  'gdkdevice-win32.c',
  'gdkdevice-wintab.c',
  'gdkdisplay-win32.c',
  'gdkdisplaymanager-win32.c',
  'gdkdnd-win32.c',
  'gdkevents-win32.c',
  'gdkgeometry-win32.c',
  'gdkglcontext-win32.c',
  'gdkglobals-win32.c',
  'gdkkeys-win32.c',
  'gdkmain-win32.c',
  'gdkmonitor-win32.c',
  'gdkproperty-win32.c',
  'gdkscreen-win32.c',
  'gdkselection-win32.c',
  'gdkvulkancontext-win32.c',
  'gdkwin32cursor.h',
  'gdkwin32display.h',
  'gdkwin32id.c',
  'gdkwindow-win32.c',
])

gdk_win32_public_headers = files([
  'gdkwin32cursor.h',
  'gdkwin32display.h',
  'gdkwin32displaymanager.h',
  'gdkwin32dnd.h',
  'gdkwin32glcontext.h',
  'gdkwin32keys.h',
  'gdkwin32misc.h',
  'gdkwin32monitor.h',
  'gdkwin32screen.h',
  'gdkwin32window.h',
])

install_headers(gdk_win32_public_headers, subdir: 'gtk-4.0/gdk/win32/')
install_headers('gdkwin32.h', subdir: 'gtk-4.0/gdk/')

gdk_win32_deps = [ # FIXME
]

libgdk_win32 = static_library('gdk-win32',
  gdk_win32_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"', '-DINSIDE_GDK_WIN32'],
  dependencies: [gdk_deps, gdk_win32_deps])
