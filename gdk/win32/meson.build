gdk_win32_sources = files([
  'gdkcairocontext-win32.c',
  'gdkcursor-win32.c',
  'gdkclipboard-win32.c',
  'gdkclipdrop-win32.c',
  'gdkdevicemanager-win32.c',
  'gdkdevice-virtual.c',
  'gdkdevice-win32.c',
  'gdkdevice-wintab.c',
  'gdkdisplay-win32.c',
  'gdkdisplaymanager-win32.c',
  'gdkdrag-win32.c',
  'gdkdrop-win32.c',
  'gdkevents-win32.c',
  'gdkglcontext-win32.c',
  'gdkglobals-win32.c',
  'gdkhdataoutputstream-win32.c',
  'gdkkeys-win32.c',
  'gdkwin32langnotification.c',
  'gdkmain-win32.c',
  'gdkmonitor-win32.c',
  'gdkproperty-win32.c',
  'gdkscreen-win32.c',
  'gdkvulkancontext-win32.c',
  'gdkwin32cursor.h',
  'gdkwin32display.h',
  'gdkwin32id.c',
  'gdksurface-win32.c',
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
  'gdkwin32surface.h',
])

install_headers(gdk_win32_public_headers, 'gdkwin32.h', subdir: 'gtk-4.0/gdk/win32/')

gdk_win32_deps = [ # FIXME
    pangowin32_dep
]

libgdk_win32 = static_library('gdk-win32',
  gdk_win32_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"', '-DINSIDE_GDK_WIN32'],
  dependencies: [gdk_deps, gdk_win32_deps])
