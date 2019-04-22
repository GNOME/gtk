gdk_public_sources = files([
  'gdk.c',
  'gdkapplaunchcontext.c',
  'gdkcairo.c',
  'gdkcairocontext.c',
  'gdkclipboard.c',
  'gdkcontentdeserializer.c',
  'gdkcontentformats.c',
  'gdkcontentprovider.c',
  'gdkcontentproviderimpl.c',
  'gdkcontentserializer.c',
  'gdkcursor.c',
  'gdkdevice.c',
  'gdkdevicepad.c',
  'gdkdevicetool.c',
  'gdkdisplay.c',
  'gdkdisplaymanager.c',
  'gdkdrag.c',
  'gdkdrawcontext.c',
  'gdkdrop.c',
  'gdkevents.c',
  'gdkframeclock.c',
  'gdkframeclockidle.c',
  'gdkframetimings.c',
  'gdkgl.c',
  'gdkglcontext.c',
  'gdkglobals.c',
  'gdkgltexture.c',
  'gdkkeys.c',
  'gdkkeyuni.c',
  'gdkmemorytexture.c',
  'gdkmonitor.c',
  'gdkpaintable.c',
  'gdkpango.c',
  'gdkpixbuf-drawable.c',
  'gdkpipeiostream.c',
  'gdkproperty.c',
  'gdkrectangle.c',
  'gdkrgba.c',
  'gdkseat.c',
  'gdkseatdefault.c',
  'gdkselection.c',
  'gdksnapshot.c',
  'gdktexture.c',
  'gdkvulkancontext.c',
  'gdksurface.c',
  'gdkprofiler.c'
])

if not win32_enabled
  gdk_public_sources += files([
    'capture/sp-capture-writer.c',
    'capture/sp-clock.c'
  ])
endif

gdk_public_headers = files([
  'gdk-autocleanup.h',
  'gdk.h',
  'gdkapplaunchcontext.h',
  'gdkcairo.h',
  'gdkcairocontext.h',
  'gdkclipboard.h',
  'gdkcontentdeserializer.h',
  'gdkcontentformats.h',
  'gdkcontentprovider.h',
  'gdkcontentproviderimpl.h',
  'gdkcontentserializer.h',
  'gdkcursor.h',
  'gdkdevice.h',
  'gdkdevicepad.h',
  'gdkdevicetool.h',
  'gdkdisplay.h',
  'gdkdisplaymanager.h',
  'gdkdrag.h',
  'gdkdrawcontext.h',
  'gdkdrop.h',
  'gdkevents.h',
  'gdkframeclock.h',
  'gdkframetimings.h',
  'gdkglcontext.h',
  'gdkgltexture.h',
  'gdkkeys.h',
  'gdkkeysyms.h',
  'gdkmemorytexture.h',
  'gdkmonitor.h',
  'gdkpaintable.h',
  'gdkpango.h',
  'gdkpixbuf.h',
  'gdkproperty.h',
  'gdkrectangle.h',
  'gdkrgba.h',
  'gdkseat.h',
  'gdksnapshot.h',
  'gdktexture.h',
  'gdktypes.h',
  'gdkvulkancontext.h',
  'gdksurface.h',
])
install_headers(gdk_public_headers, subdir: 'gtk-4.0/gdk/')

gdk_sources = gdk_public_sources

gdk_private_h_sources = files([
  'gdkeventsprivate.h',
  'gdkdevicetoolprivate.h',
  'gdkmonitorprivate.h',
  'gdkseatdefaultprivate.h',
])

gdk_gresource_xml = configure_file(output : 'gdk.gresource.xml',
  input : 'gen-gdk-gresources-xml.py',
  command : [find_program('gen-gdk-gresources-xml.py'),
             meson.current_source_dir(), '@OUTPUT@'])

gdkresources = gnome.compile_resources('gdkresources',
  gdk_gresource_xml,
  source_dir: '.',
  c_name: '_gdk',
  extra_args: '--manual-register')

gdk_headers = gdk_public_headers

gdk_enums = gnome.mkenums('gdkenumtypes',
  sources: gdk_public_headers,
  c_template : 'gdkenumtypes.c.template',
  h_template : 'gdkenumtypes.h.template',
  install_dir: join_paths(gtk_includedir, 'gtk-4.0/gdk'),
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
gdkconfig_cdata.set('GDK_RENDERING_VULKAN', have_vulkan)

gdkconfig = configure_file(
  input  : 'gdkconfig.h.meson',
  output : 'gdkconfig.h',
  configuration : gdkconfig_cdata,
  install_dir: join_paths(gtk_includedir, 'gtk-4.0/gdk'))


gdkversion_cdata = configuration_data()
gdkversion_cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
gdkversion_cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
gdkversion_cdata.set('GTK_MICRO_VERSION', gtk_micro_version)

gdkversionmacros = configure_file(
  input  : 'gdkversionmacros.h.in',
  output : 'gdkversionmacros.h',
  configuration: gdkversion_cdata,
  install_dir: join_paths(gtk_includedir, 'gtk-4.0/gdk'))

gdkinc = include_directories('.')
gdkx11_inc = include_directories('x11')
gdkwayland_inc = include_directories('wayland')

wlinc = include_directories('.')
win32rcinc = include_directories('win32/rc')

gdk_gen_headers = [gdkenum_h, gdkmarshal_h, gdkconfig, gdkversionmacros]

gdk_deps = [
  libm,
  pixbuf_dep,
  cairo_dep,
  pango_dep,
  fribidi_dep,
  cairogobj_dep,
  glib_dep,
  gobject_dep,
  graphene_dep,
  epoxy_dep,
  fontconfig_dep,
  platform_gio_dep,
  pangocairo_dep,
  vulkan_dep,
]

# add generated gdk sources
gdk_sources += [
  gdkconfig,
  gdk_enums,
  gdk_marshalers,
  gdkresources,
  gdkversionmacros,
  gdk_private_h_sources,
  gdk_public_headers
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

# FIXME: might have to add '-xobjective-c' to c_args for quartz backend?
libgdk = static_library('gdk',
  sources: [gdk_sources, gdk_backends_gen_headers, gdkconfig],
  dependencies: gdk_deps + [libgtk_css_dep],
  link_with: [libgtk_css, ],
  include_directories: [confinc, gdkx11_inc, wlinc],
  c_args: [
    '-DGDK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  link_whole: gdk_backends,
  link_args: common_ldflags)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgdk_dep = declare_dependency(
  sources: ['gdk.h', gdkconfig, gdkenum_h],
  include_directories: [confinc, gdkx11_inc, wlinc],
  dependencies: gdk_deps + [libgtk_css_dep])
