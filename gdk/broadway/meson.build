gdk_broadway_sources = files([
  'broadway-buffer.c',
  'broadway-output.c',
  'broadway-server.c',
  'broadwayd.c',
  'gdkbroadway-server.c',
  'gdkcursor-broadway.c',
  'gdkdevice-broadway.c',
  'gdkdevicemanager-broadway.c',
  'gdkdisplay-broadway.c',
  'gdkdnd-broadway.c',
  'gdkeventsource.c',
  'gdkglobals-broadway.c',
  'gdkkeys-broadway.c',
  'gdkmonitor-broadway.c',
  'gdkproperty-broadway.c',
  'gdkscreen-broadway.c',
  'gdkselection-broadway.c',
  'gdkwindow-broadway.c',
])

gdk_broadway_public_headers = [
  'gdkbroadwaydisplay.h',
  'gdkbroadwaywindow.h',
  'gdkbroadwaycursor.h',
  'gdkbroadwaymonitor.h',
]

# Broadway backend headers aren't installed it seems
#install_headers(gdk_broadway_public_headers, subdir: 'gtk-4.0/gdk/broadway/')
#install_headers('gdkbroadway.h', subdir: 'gtk-4.0/gdk/')

gdk_broadway_deps = [shmlib]

libgdk_broadway = static_library('gdk-broadway',
  gdk_broadway_sources, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"'],
  dependencies: [gdk_deps, gdk_broadway_deps])

# gtk4-broadwayd

broadwayd_syslib = os_win32 ? find_library('ws2_32') : shmlib

clienthtml_h = custom_target('clienthtml.h',
  input : 'client.html',
  output : 'clienthtml.h',
  command : [find_program('toarray.pl'), 'client_html', '@INPUT@'],
  capture : true)

broadwayjs_h = custom_target('broadwayjs.h',
  input : ['broadway.js', 'rawinflate.min.js'],
  output : 'broadwayjs.h',
  command : [find_program('toarray.pl'), 'broadway_js', '@INPUT0@', '@INPUT1@'],
  capture : true)

executable('gtk4-broadwayd',
  clienthtml_h, broadwayjs_h,
  'broadwayd.c', 'broadway-server.c', 'broadway-buffer.c', 'broadway-output.c',
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"'],
  dependencies : [broadwayd_syslib, gdk_deps],
  install : true)
