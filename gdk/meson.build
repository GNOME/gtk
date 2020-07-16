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
  'gdkeventhistory.c',
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
  'gdkprofiler.c',
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

gdk_gir_public_headers = files(
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
gdk_nogir_public_headers = [files('gdkkeysyms-compat.h')]
gdk_public_headers = gdk_gir_public_headers + gdk_nogir_public_headers
install_headers(gdk_public_headers, subdir : 'gtk-3.0/gdk')

deprecated_h_sources = files('deprecated/gdkcolor.h')
install_headers(deprecated_h_sources, subdir : 'gtk-3.0/gdk/deprecated')

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
gdkconfig_cdata.set('GDK_WINDOWING_QUARTZ', quartz_enabled)

gdkconfig = configure_file(
  input  : 'gdkconfig.h.meson',
  output : 'gdkconfig.h',
  configuration : gdkconfig_cdata,
  install_dir: join_paths(gtk_includedir, 'gtk-3.0/gdk'))


gdkversion_cdata = configuration_data()
gdkversion_cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
gdkversion_cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
gdkversion_cdata.set('GTK_MICRO_VERSION', gtk_micro_version)
gdkversion_cdata.set('GTK_API_VERSION', gtk_api_version)
gdkversion_cdata.set('GTK_BINARY_VERSION', gtk_binary_version)
gdkversion_cdata.set('GTK_VERSION', meson.project_version())
gdkversion_cdata.set('LT_CURRENT_MINUS_AGE', gtk_soversion)

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
  fribidi_dep,
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

  gdk_rc = configure_file(
    input: 'win32/rc/gdk.rc.in',
    output: 'gdk.rc',
    configuration: gdkversion_cdata,
  )
  gdk_res = import('windows').compile_resources(gdk_rc, include_directories: include_directories('win32/rc'))
  gdk_sources += gdk_res
endif

if profiler_enabled
  if profiler_dep.found()
    gdk_deps += [profiler_dep]
  endif
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
    if backend == 'quartz'
      common_cflags += ['-xobjective-c']
    endif
  endif
endforeach

if gdk_backends.length() == 0
  error('No backends enabled')
endif

gdk_link_args = common_ldflags

# Somehow gdk_win32_display_manager_get_type is not being exported as it
# seems that it was optimized out by the MSVC linker.  Force-export it here.
if cc.get_id() == 'msvc'
  gdk_link_args += [ '-export:gdk_win32_display_manager_get_type' ]
endif

libgdk = library('gdk-3',
                 soversion: gtk_soversion,
                 version: gtk_library_version,
                 sources: [gdk_sources, gdk_backends_gen_headers, gdkconfig],
                 dependencies: gdk_deps,
                 include_directories: [confinc, gdkx11_inc, wlinc],
                 c_args: [
                   '-DGDK_COMPILATION',
                   '-DG_LOG_DOMAIN="Gdk"',
                 ] + common_cflags,
                 link_whole: gdk_backends,
                 link_args: gdk_link_args,
                 darwin_versions : gtk_osxversions,
                 install: true)

# unit tests
subdir('unit-tests')

gdk_dep_sources = [gdkconfig, gdkenum_h]
# Introspection
if build_gir
  gir_args = ['--quiet']

  # We may build some of the dependencies as sub-projects; this means
  # that we need to depend on the built introspection data, instead of
  # the installed one
  gdk_gir_inc = [ 'cairo-1.0', 'Gio-2.0', ]

  if pixbuf_dep.type_name() == 'internal'
    gdk_gir_inc += subproject('gdk-pixbuf').get_variable('gdkpixbuf_gir').get(0)
  else
    gdk_gir_inc += 'GdkPixbuf-2.0'
  endif

  if pango_dep.type_name() == 'internal'
    gdk_gir_inc += subproject('pango').get_variable('pango_gir').get(0)
  else
    gdk_gir_inc += 'Pango-1.0'
  endif

  gdk_gir_sources = gdk_gir_public_headers + deprecated_h_sources
  gdk_gir_sources += gdk_sources + gdk_enums + [gdkconfig]
  gdk_gir = gnome.generate_gir(libgdk,
                               sources: gdk_gir_sources,
                               namespace: 'Gdk',
                               nsversion: gtk_api_version,
                               identifier_prefix: 'Gdk',
                               symbol_prefix: 'gdk',
                               export_packages: 'gdk-3.0',
                               includes: gdk_gir_inc,
                               header: 'gdk/gdk.h',
                               install: true,
                               extra_args: gir_args + [
                                 '-DGDK_COMPILATION',
                               ])
  gdk_gir_dep = declare_dependency(sources: gdk_gir)
  gdk_dep_sources += gdk_gir

  if x11_enabled
    gdk_x11_gir = gnome.generate_gir(libgdk,
                                     sources: gdk_x11_public_headers + gdk_x11_sources,
                                     namespace: 'GdkX11',
                                     nsversion: gtk_api_version,
                                     identifier_prefix: 'Gdk',
                                     symbol_prefix: 'gdk',
                                     export_packages: 'gdk-x11-3.0',
                                     includes: gdk_gir_inc + [ gdk_gir[0], 'xlib-2.0', ],
                                     install: true,
                                     dependencies: gdk_gir_dep,
                                     header: 'gdk/gdkx.h',
                                     extra_args: gir_args + [
                                       '-DGDK_COMPILATION',
                                     ])
    gdk_dep_sources += gdk_x11_gir
  endif

  if win32_enabled
    gdk_win32_gir = gnome.generate_gir(libgdk,
                                     sources: gdk_win32_public_headers + gdk_win32_sources,
                                     namespace: 'GdkWin32',
                                     nsversion: gtk_api_version,
                                     identifier_prefix: 'Gdk',
                                     symbol_prefix: 'gdk',
                                     export_packages: 'gdk-win32-3.0',
                                     includes: gdk_gir_inc + [gdk_gir[0]],
                                     install: true,
                                     dependencies: gdk_gir_dep,
                                     header: 'gdk/gdkwin32.h',
                                     extra_args: gir_args + [
                                       '-DGDK_COMPILATION',
                                     ])
    gdk_dep_sources += gdk_win32_gir
  endif
endif

libgdk_dep = declare_dependency(
  link_with: libgdk,
  sources: gdk_dep_sources,
  include_directories: [confinc, gdkx11_inc, wlinc],
  dependencies: gdk_deps)
