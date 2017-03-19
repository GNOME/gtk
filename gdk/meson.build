gdk_sources = files([
  'gdk.c',
  'gdkapplaunchcontext.c',
  'gdkcairo.c',
  'gdkcursor.c',
  'gdkdeprecated.c',
  'gdkdevice.c',
  'gdkdevicemanager.c',
  'gdkdevicepad.c',
  'gdkdevicetool.c',
  'gdkdisplay.c',
  'gdkdisplaymanager.c',
  'gdkdnd.c',
  'gdkdrawcontext.c',
  'gdkdrawingcontext.c',
  'gdkevents.c',
  'gdkframeclock.c',
  'gdkframeclockidle.c',
  'gdkframetimings.c',
  'gdkgl.c',
  'gdkglcontext.c',
  'gdkglobals.c',
  'gdkkeys.c',
  'gdkkeyuni.c',
  'gdkmonitor.c',
  'gdkpango.c',
  'gdkpixbuf-drawable.c',
  'gdkproperty.c',
  'gdkrectangle.c',
  'gdkrgba.c',
  'gdkscreen.c',
  'gdkseat.c',
  'gdkseatdefault.c',
  'gdkselection.c',
  'gdkvisual.c',
  'gdkvulkancontext.c',
  'gdkwindow.c',
  'gdkwindowimpl.c',
])

gdk_public_headers = files([
  'gdk-autocleanup.h',
  'gdk.h',
  'gdkapplaunchcontext.h',
  'gdkcairo.h',
  'gdkcursor.h',
  'gdkdevice.h',
  'gdkdevicemanager.h',
  'gdkdevicepad.h',
  'gdkdevicetool.h',
  'gdkdisplay.h',
  'gdkdisplaymanager.h',
  'gdkdnd.h',
  'gdkdrawingcontext.h',
  'gdkevents.h',
  'gdkframeclock.h',
  'gdkframetimings.h',
  'gdkglcontext.h',
  'gdkkeys.h',
  'gdkkeysyms-compat.h',
  'gdkkeysyms.h',
  'gdkmain.h',
  'gdkmonitor.h',
  'gdkpango.h',
  'gdkpixbuf.h',
  'gdkprivate.h',
  'gdkproperty.h',
  'gdkrectangle.h',
  'gdkrgba.h',
  'gdkscreen.h',
  'gdkseat.h',
  'gdkselection.h',
  'gdkthreads.h',
  'gdktypes.h',
  'gdkvisual.h',
  'gdkwindow.h',
])
install_headers(gdk_public_headers, subdir: 'gtk-4.0/gdk/')

gdk_private_h_sources = files([
  'gdkdevicetoolprivate.h',
  'gdkdrawingcontextprivate.h',
  'gdkmonitorprivate.h',
  'gdkprivate.h',
  'gdkseatdefaultprivate.h',
])


gdk_wayland_sources = files([
  'wayland/gdkapplaunchcontext-wayland.c',
  'wayland/gdkcursor-wayland.c',
  'wayland/gdkdevice-wayland.c',
  'wayland/gdkdisplay-wayland.c',
  'wayland/gdkdisplay-wayland.h',
  'wayland/gdkdnd-wayland.c',
  'wayland/gdkeventsource.c',
  'wayland/gdkglcontext-wayland.c',
  'wayland/gdkglcontext-wayland.h',
  'wayland/gdkkeys-wayland.c',
  'wayland/gdkmonitor-wayland.c',
  'wayland/gdkmonitor-wayland.h',
  'wayland/gdkscreen-wayland.c',
  'wayland/gdkseat-wayland.h',
  'wayland/gdkselection-wayland.c',
  'wayland/gdkvulkancontext-wayland.c',
  'wayland/gdkwayland.h',
  'wayland/gdkwaylanddevice.h',
  'wayland/gdkwaylanddisplay.h',
  'wayland/gdkwaylandglcontext.h',
  'wayland/gdkwaylandmonitor.h',
  'wayland/gdkwaylandselection.h',
  'wayland/gdkwaylandwindow.h',
  'wayland/gdkwindow-wayland.c',
  'wayland/wm-button-layout-translation.c',
])

gdk_wayland_public_headers = files([
  'wayland/gdkwaylanddevice.h',
  'wayland/gdkwaylanddisplay.h',
  'wayland/gdkwaylandglcontext.h',
  'wayland/gdkwaylandselection.h',
  'wayland/gdkwaylandwindow.h'
])

gdk_wayland_private_sources = files([
  'wayland/gdkprivate-wayland.h',
])



gdk_broadway_sources = files([
  'broadway/broadway-buffer.c',
  'broadway/broadway-buffer.h',
  'broadway/broadway-output.c',
  'broadway/broadway-output.h',
  'broadway/broadway-server.c',
  'broadway/broadway-server.h',
  'broadway/broadwayd.c',
  'broadway/gdkbroadway-server.c',
  'broadway/gdkcursor-broadway.c',
  'broadway/gdkdevice-broadway.c',
  'broadway/gdkdevice-broadway.h',
  'broadway/gdkdevicemanager-broadway.c',
  'broadway/gdkdevicemanager-broadway.h',
  'broadway/gdkdisplay-broadway.c',
  'broadway/gdkdisplay-broadway.h',
  'broadway/gdkdnd-broadway.c',
  'broadway/gdkeventsource.c',
  'broadway/gdkeventsource.h',
  'broadway/gdkglobals-broadway.c',
  'broadway/gdkkeys-broadway.c',
  'broadway/gdkmonitor-broadway.c',
  'broadway/gdkmonitor-broadway.h',
  'broadway/gdkproperty-broadway.c',
  'broadway/gdkscreen-broadway.c',
  'broadway/gdkscreen-broadway.h',
  'broadway/gdkselection-broadway.c',
  'broadway/gdkwindow-broadway.c',
  'broadway/gdkwindow-broadway.h',
])

gdk_broadway_private_sources = files([
  'broadway/gdkprivate-broadway.h'
])

gdk_gresource_xml = configure_file(output : 'gdk.gresource.xml',
  input : 'gen-gsk-resources-xml.py',
  command : [find_program('gen-gdk-gresources-xml.py'),
             meson.current_source_dir(), '@OUTPUT@'])

gdkresources = gnome.compile_resources('gdkresources',
  gdk_gresource_xml,
  source_dir: '.',
  c_name: '_gdk',
  extra_args: '--manual-register')

gdk_headers = gdk_public_headers  + gdk_private_h_sources

gdk_enums = gnome.mkenums('gdkenumtypes',
  sources: gdk_public_headers,
  c_template : 'gdkenumtypes.c.template',
  h_template : 'gdkenumtypes.h.template',
  install_dir : 'gtk-4.0/gdk',
  install_header : true)

gdkenum_h = gdk_enums[1]

gdk_marshalers = gnome.genmarshal('gdkmarshalers',
  sources : 'gdkmarshalers.list',
  prefix : '_gdk_marshal',
  valist_marshallers : true)

gdkmarshal_h = gdk_marshalers[1]

gdkconfig_cdata = configuration_data()
gdkconfig_cdata.set('GDK_WINDOWING_X11', x11_enabled)
gdkconfig_cdata.set('GDK_WINDOWING_WAYLAND', wayland_enabled)
gdkconfig_cdata.set('GDK_WINDOWING_BROADWAY', broadway_enabled)
gdkconfig_cdata.set('GDK_RENDERING_VULKAN', have_vulkan)

gdkconfig = configure_file(
  input  : 'gdkconfig.h.meson',
  output : 'gdkconfig.h',
  configuration : gdkconfig_cdata,
  install_dir: 'include/gtk-4.0/gdk/')


gdkversion_cdata = configuration_data()
gdkversion_cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
gdkversion_cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
gdkversion_cdata.set('GTK_MICRO_VERSION', gtk_micro_version)

gdkversionmacros = configure_file(
  input  : 'gdkversionmacros.h.in',
  output : 'gdkversionmacros.h',
  configuration: gdkversion_cdata,
  install_dir: 'include/gtk-4.0/gdk/')

xinc = include_directories('x11')
wlinc = include_directories('.')


gdk_deps = [
  mlib,
  pixbuf_dep,
  cairo_dep,
  pango_dep,
  cairogobj_dep,
  glib_dep,
  epoxy_dep,
  fontconfig_dep,
  giounix_dep,
  pangocairo_dep,
  vulkan_lib
]

# add generated gdk sources
gdk_sources += [
  gdkconfig,
  gdk_enums,
  gdk_marshalers,
  gdkresources,
  gdkversionmacros,
  gdk_headers
]

if wayland_enabled or broadway_enabled
  if cc.has_function('shm_open', name : 'shm_open() in libc')
    shmlib = []
  elif cc.has_function('shm_open', args : '-lrt', name : 'shm_open() in librt')
    shmlib = cc.find_library('rt')
  else
    shmlib = []
  endif
endif

if x11_enabled
  subdir('x11')
  gdk_deps += gdk_x11_deps
  gdk_sources += gdk_x11_sources
endif

if wayland_enabled
  subdir('wayland')
  gdk_deps += [
    shmlib,
    xkbdep,
    wlclientdep,
    wlprotocolsdep,
    wlcursordep,
    wlegldep
  ]
  gdk_sources += [
    gdk_wayland_sources,
    gdk_wayland_private_sources
  ]

  # install_headers(gdk_wayland_public_headers, subdir: 'gtk-3.0/gdk/wayland/')
endif

if broadway_enabled
  gdk_deps += [
    shmlib,
  ]
  gdk_sources += [
    gdk_broadway_sources,
    gdk_broadway_private_sources
  ]
endif

libgdk = static_library('gdk',
  sources: [gdk_sources, gdkconfig],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"'],
  include_directories: [confinc, xinc, wlinc],
  dependencies: gdk_deps,
)


libgdk_dep = declare_dependency(
  sources: ['gdk.h', gdkconfig, gdkenum_h],
  depends: gdk_sources,
  include_directories: [confinc, xinc, wlinc],
  dependencies: gdk_deps,
  link_with: libgdk,
  link_args: ['-Bsymbolic'],
)
