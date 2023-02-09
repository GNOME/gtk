gdk_win32_sources = files(
  'gdkcursor-win32.c',
  'gdkdevicemanager-win32.c',
  'gdkdevice-virtual.c',
  'gdkdevice-win32.c',
  'gdkdevice-winpointer.c',
  'gdkdevice-wintab.c',
  'gdkdisplay-win32.c',
  'gdkdisplaymanager-win32.c',
  'gdkdnd-win32.c',
  'gdkevents-win32.c',
  'gdkgeometry-win32.c',
  'gdkglcontext-win32.c',
  'gdkglcontext-win32-private.c',
  'gdkglobals-win32.c',
  'gdkkeys-win32.c',
  'gdkkeys-win32-impl.c',
  'gdkkeys-win32-impl-wow64.c',
  'gdkmain-win32.c',
  'gdkmonitor-win32.c',
  'gdkproperty-win32.c',
  'gdkscreen-win32.c',
  'gdkselection-win32.c',
  'gdktestutils-win32.c',
  'gdkwin32id.c',
  'gdkwin32langnotification.c',
  'gdkwindow-win32.c',
)

gdk_win32_public_headers = files(
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
)

GDK_WIN32_CFLAGS = ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"', '-DINSIDE_GDK_WIN32']

if win32_has_egl
  GDK_WIN32_CFLAGS += '-DGDK_WIN32_ENABLE_EGL'
endif


install_headers(gdk_win32_public_headers, subdir: 'gtk-3.0/gdk/win32')
install_headers('gdkwin32.h', subdir: 'gtk-3.0/gdk')
gdk_pkg_deps = [pangowin32_dep] + gdk_pkg_deps

gdk_backend_extra_pkgs += dependency(
  'cairo-win32',
  version: cairo_req,
  required: false
)

gdk_win32_lib_deps = [
  cc.find_library('advapi32'),
  cc.find_library('dwmapi'),
  cc.find_library('gdi32'),
  cc.find_library('hid'),
  cc.find_library('imm32'),
  cc.find_library('ole32'),
  cc.find_library('opengl32'),
  cc.find_library('setupapi'),
  cc.find_library('shell32'),
  cc.find_library('user32'),
  cc.find_library('winmm'),
]

gdk_backend_extra_libs = gdk_win32_lib_deps
# For Go users, should check whether we should ask people to
# whitelist -Wl,-l.... items, since we are dynamically linking
# items
if cc.get_argument_syntax() != 'msvc'
  if get_option('default_library') == 'shared'
    gdk_backend_extra_libs += [ '-Wl,-luuid' ]
  else
    gdk_backend_extra_libs += cc.find_library('uuid')
  endif
endif
gdk_win32_deps = [gdk_pkg_deps, gdk_win32_lib_deps]

libgdk_win32 = static_library('gdk-win32',
  gdk_win32_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: GDK_WIN32_CFLAGS,
  dependencies: [gdk_deps, gdk_win32_deps])
