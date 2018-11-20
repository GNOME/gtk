deprecated_gdk_sources = files('deprecated/gdkcolor.c')
gdk_sources = files(
  'gdk-private.c',
  'gdk.c',
  'gdkapplaunchcontext.c',
  'gdkcairo.c',
  'gdkcursor.c',
  'gdkdeprecated.c',
  'gdkdevice.c',
  'gdkdevicemanager.c',
  'gdkdevicepad.c',
  'gdkdisplay.c',
  'gdkdisplaymanager.c',
  'gdkdnd.c',
  'gdkevents.c',
  'gdkframetimings.c',
  'gdkgl.c',
  'gdkglcontext.c',
  'gdkglobals.c',
  'gdkkeys.c',
  'gdkkeyuni.c',
  'gdkoffscreenwindow.c',
  'gdkframeclock.c',
  'gdkframeclockidle.c',
  'gdkpango.c',
  'gdkpixbuf-drawable.c',
  'gdkproperty.c',
  'gdkrectangle.c',
  'gdkrgba.c',
  'gdkscreen.c',
  'gdkselection.c',
  'gdkvisual.c',
  'gdkwindow.c',
  'gdkwindowimpl.c',
  'gdkseat.c',
  'gdkseatdefault.c',
  'gdkdevicetool.c',
  'gdkdrawingcontext.c',
  'gdkmonitor.c'
)

gdk_public_headers = files(
  'gdk.h',
  'gdk-autocleanup.h',
  'gdkapplaunchcontext.h',
  'gdkcairo.h',
  'gdkcursor.h',
  'gdkdevice.h',
  'gdkdevicetool.h',
  'gdkdevicemanager.h',
  'gdkdevicepad.h',
  'gdkdisplay.h',
  'gdkdisplaymanager.h',
  'gdkdnd.h',
  'gdkdrawingcontext.h',
  'gdkevents.h',
  'gdkframetimings.h',
  'gdkglcontext.h',
  'gdkkeys.h',
  'gdkkeysyms.h',
  'gdkkeysyms-compat.h',
  'gdkmain.h',
  'gdkmonitor.h',
  'gdkpango.h',
  'gdkframeclock.h',
  'gdkpixbuf.h',
  'gdkprivate.h',
  'gdkproperty.h',
  'gdkrectangle.h',
  'gdkrgba.h',
  'gdkscreen.h',
  'gdkseat.h',
  'gdkselection.h',
  'gdktestutils.h',
  'gdkthreads.h',
  'gdktypes.h',
  'gdkvisual.h',
  'gdkwindow.h',
)

deprecated_h_sources = files('deprecated/gdkcolor.h')

# This variable is unused, but added just for completeness
gdk_private_headers = files(
  'gdkseatdefaultprivate.h',
  'gdkdevicetoolprivate.h',
  'gdkdrawingcontextprivate.h',
  'gdkmonitorprivate.h',
  'gdkprivate.h',
)

gdk_wayland_sources = files(
  'wayland/gdkapplaunchcontext-wayland.c',
  'wayland/gdkcursor-wayland.c',
  'wayland/gdkdevice-wayland.c',
  'wayland/gdkdisplay-wayland.c',
  'wayland/gdkdnd-wayland.c',
  'wayland/gdkeventsource.c',
  'wayland/gdkglcontext-wayland.c',
  'wayland/gdkkeys-wayland.c',
  'wayland/gdkmonitor-wayland.c',
  'wayland/gdkscreen-wayland.c',
  'wayland/gdkselection-wayland.c',
  'wayland/gdkwindow-wayland.c',
  'wayland/wm-button-layout-translation.c',
)

gdk_broadway_sources = files([
  'broadway/gdkdisplay-broadway.c',
  'broadway/gdkscreen-broadway.c',
  'broadway/broadway-output.c',
  'broadway/broadway-server.c',
  'broadway/gdkbroadway-server.c',
  'broadway/gdkcursor-broadway.c',
  'broadway/gdkvisual-broadway.c',
  'broadway/gdkselection-broadway.c',
  'broadway/gdkwindow-broadway.c',
  'broadway/gdkmonitor-broadway.c',
  'broadway/gdkkeys-broadway.c',
  'broadway/gdkglobals-broadway.c',
  'broadway/gdkeventsource.c',
  'broadway/gdkdnd-broadway.c',
  'broadway/broadwayd.c',
  'broadway/gdkdevicemanager-broadway.c',
  'broadway/gdkdevice-broadway.c',
  'broadway/broadway-buffer.c',
  'broadway/gdktestutils-broadway.c',
  'broadway/gdkproperty-broadway.c'
])

gdk_gresource_xml = configure_file(output : 'gdk.gresource.xml',
  input : 'gen-gdk-gresources-xml.py',
  command : [find_program('gen-gdk-gresources-xml.py'),
             meson.current_source_dir(), '@OUTPUT@'])

gdkresources = gnome.compile_resources('gdkresources',
  gdk_gresource_xml,
  source_dir: '.',
  c_name: '_gdk',
  extra_args: '--manual-register'
)

gdk_headers = gdk_public_headers

gdk_enums = gnome.mkenums('gdkenumtypes',
  sources: gdk_public_headers,
  c_template : 'gdkenumtypes.c.template',
  h_template : 'gdkenumtypes.h.template',
  install_dir: join_paths(gtk_includedir, 'gtk-3.0/gdk'),
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
gdkconfig_cdata.set('GDK_WINDOWING_WIN32', win32_enabled)
gdkconfig_cdata.set('GDK_WINDOWING_BROADWAY', broadway_enabled)

gdkconfig = configure_file(
  input  : 'gdkconfig.h.meson',
  output : 'gdkconfig.h',
  configuration : gdkconfig_cdata,
  install_dir: join_paths(gtk_includedir, 'gtk-3.0/gdk'),
  install : true)


gdkversion_cdata = configuration_data()
gdkversion_cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
gdkversion_cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
gdkversion_cdata.set('GTK_MICRO_VERSION', gtk_micro_version)

gdkversionmacros = configure_file(
  input  : 'gdkversionmacros.h.in',
  output : 'gdkversionmacros.h',
  configuration: gdkversion_cdata,
  install_dir: join_paths(gtk_includedir, 'gtk-3.0/gdk'))

gdkinc = include_directories('.')
gdkx11_inc = include_directories('x11')
gdkwayland_inc = include_directories('wayland')

wlinc = include_directories('.')

gdk_gen_headers = [gdkenum_h, gdkmarshal_h, gdkconfig, gdkversionmacros]

gdk_deps = [
  libm,
  pixbuf_dep,
  cairo_dep,
  pango_dep,
  cairogobj_dep,
  glib_dep,
  epoxy_dep,
  fontconfig_dep,
  platform_gio_dep,
  pangocairo_dep
]

if win32_enabled
  gdk_deps += [cc.find_library('advapi32'),
               cc.find_library('comctl32'),
               cc.find_library('dwmapi'),
               cc.find_library('imm32'),
               cc.find_library('setupapi'),
               cc.find_library('winmm')]
endif

gdk_sources = [
  # Generated
  gdkconfig,
  gdk_enums,
  gdk_marshalers,
  gdkresources,
  gdkversionmacros,
  # Static
  gdk_sources,
  deprecated_gdk_sources,
]

if wayland_enabled or broadway_enabled
  if cc.has_function('shm_open')
    shmlib = []
  elif cc.has_function('shm_open', args : '-lrt')
    shmlib = cc.find_library('rt')
  else
    shmlib = []
  endif
endif

gdk_backends = []
gdk_backends_gen_headers = []  # non-public generated headers
foreach backend : ['broadway', 'quartz', 'wayland', 'win32', 'x11']
  if get_variable('@0@_enabled'.format(backend))
    subdir(backend)
    gdk_deps += get_variable('gdk_@0@_deps'.format(backend))
    gdk_backends += get_variable('libgdk_@0@'.format(backend))
    # Special-case this for now to work around Meson bug with get_variable()
    # fallback being an empty array, or any array (#1481)
    if backend == 'wayland'
      gdk_backends_gen_headers += get_variable('gdk_@0@_gen_headers'.format(backend))
    endif
  endif
endforeach

if gdk_backends.length() == 0
  error('No backends enabled')
endif

libgdk = shared_library('gdk-3',
                        sources: [gdk_sources, gdk_backends_gen_headers, gdkconfig],
                        dependencies: gdk_deps,
                        include_directories: [confinc, gdkx11_inc, wlinc],
                        c_args: [
                          '-DGDK_COMPILATION',
                          '-DG_LOG_DOMAIN="Gdk"',
                        ] + common_cflags,
                        link_whole: gdk_backends,
                        link_args: common_ldflags,
                        install: true)

libgdk_dep = declare_dependency(
  link_with: libgdk,
  sources: ['gdk.h', gdkconfig, gdkenum_h],
  include_directories: [confinc, gdkx11_inc, wlinc],
  dependencies: gdk_deps)
