deprecated_gdk_sources = ['deprecated/gdkcolor.c']
gdk_sources = files([
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
])

gdk_public_h_sources = files([
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
])

gdk_private_h_sources = files([
  'gdkseatdefaultprivate.h',
  'gdkdevicetoolprivate.h',
  'gdkdrawingcontextprivate.h',
  'gdkmonitorprivate.h',
  'gdkprivate.h',
])

gdk_x_sources = [
  'x11/MwmUtil.h',
  'x11/gdkapplaunchcontext-x11.c',
  'x11/gdkasync.c',
  'x11/gdkasync.h',
  'x11/gdkcursor-x11.c',
  'x11/gdkdevice-core-x11.c',
  'x11/gdkdevice-xi2.c',
  'x11/gdkdevicemanager-core-x11.c',
  'x11/gdkdevicemanager-x11.c',
  'x11/gdkdevicemanager-xi2.c',
  'x11/gdkdisplaymanager-x11.c',
  'x11/gdkdisplay-x11.c',
  'x11/gdkdisplay-x11.h',
  'x11/gdkdnd-x11.c',
  'x11/gdkeventsource.c',
  'x11/gdkeventsource.h',
  'x11/gdkeventtranslator.c',
  'x11/gdkeventtranslator.h',
  'x11/gdkgeometry-x11.c',
  'x11/gdkglcontext-x11.c',
  'x11/gdkglcontext-x11.h',
  'x11/gdkkeys-x11.c',
  'x11/gdkmain-x11.c',
  'x11/gdkproperty-x11.c',
  'x11/gdkscreen-x11.c',
  'x11/gdkscreen-x11.h',
  'x11/gdkselection-x11.c',
  'x11/gdktestutils-x11.c',
  'x11/gdkvisual-x11.c',
  'x11/gdkwindow-x11.c',
  'x11/gdkwindow-x11.h',
  'x11/gdkxftdefaults.c',
  'x11/gdkxid.c',
  'x11/gdkx.h',
  'x11/xsettings-client.h',
  'x11/xsettings-client.c',
  'x11/gdkmonitor-x11.c',
  'x11/gdkmonitor-x11.h',
  'x11/gdkx11monitor.h'
]

gdk_x_private_sources = [
  'x11/gdkprivate-x11.h',
  'x11/gdkdevicemanagerprivate-core.h',
]

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
  'wayland/gdkwaylanddevice.h',
  'wayland/gdkwaylanddisplay.h',
  'wayland/gdkwaylandglcontext.h',
  'wayland/gdkwayland.h',
  'wayland/gdkwaylandmonitor.h',
  'wayland/gdkwaylandselection.h',
  'wayland/gdkwaylandwindow.h',
  'wayland/gdkwindow-wayland.c',
  'wayland/wm-button-layout-translation.c',
])

gdk_wayland_private_sources = files([
  'wayland/gdkprivate-wayland.h',
])



gdk_broadway_sources = files([
  'broadway/gdkdisplay-broadway.c',
  'broadway/gdkdisplay-broadway.h',
  'broadway/gdkscreen-broadway.c',
  'broadway/gdkscreen-broadway.h',
  'broadway/broadway-output.c',
  'broadway/broadway-output.h',
  'broadway/broadway-server.h',
  'broadway/broadway-server.c',
  'broadway/gdkbroadway-server.c',
  'broadway/gdkcursor-broadway.c',
  'broadway/gdkvisual-broadway.c',
  'broadway/gdkselection-broadway.c',
  'broadway/gdkwindow-broadway.c',
  'broadway/gdkwindow-broadway.h',
  'broadway/gdkmonitor-broadway.c',
  'broadway/gdkmonitor-broadway.h',
  'broadway/gdkkeys-broadway.c',
  'broadway/gdkglobals-broadway.c',
  'broadway/gdkeventsource.c',
  'broadway/gdkeventsource.h',
  'broadway/gdkdnd-broadway.c',
  'broadway/broadwayd.c',
  'broadway/gdkdevicemanager-broadway.c',
  'broadway/gdkdevicemanager-broadway.h',
  'broadway/gdkdevice-broadway.c',
  'broadway/gdkdevice-broadway.h',
  'broadway/broadway-buffer.c',
  'broadway/broadway-buffer.h',
  'broadway/gdktestutils-broadway.c',
  'broadway/gdkproperty-broadway.c'
])

gdk_broadway_private_sources = files([
  'broadway/gdkprivate-broadway.h'
])

# glsl_sources = [
  # 'gl3-texture-2d.fs.glsl',
  # 'gl3-texture-2d.vs.glsl',
  # 'gl3-texture-rect.fs.glsl',
  # 'gl3-texture-rect.vs.glsl',
  # 'gl2-texture-2d.fs.glsl',
  # 'gl2-texture-2d.fs.glsl',
  # 'gl2-texture-rect.vs.glsl',
  # 'gl2-texture-rect.vs.glsl',
  # 'gles2-texture.fs.glsl',
  # 'gles2-texture.vs.glsl',
# ]
# resource_xml = ''
# foreach glsl_file : glsl_sources
  # resource_xml += '    <file alias="glsl/@0@">resources/glsl/@0@</file>\n'.format(glsl_file)
# endforeach


# cdata = configuration_data()
# cdata.set('GDK_GRESOURCE_XML_FILES', resource_xml)
# gdkresources_xml = configure_file(
  # input  : 'gdk.gresource.xml.meson',
  # output : 'gdk.gresource.xml',
  # configuration : cdata
# )

gdkresources = gnome.compile_resources(
  'gdkresources',
  'gdk.gresource.xml',
  source_dir: '.',
  c_name: '_gdk',
  extra_args: '--manual-register'
)


deprecated_h_sources = files(['deprecated/gdkcolor.h'])

gdk_headers = gdk_public_h_sources + deprecated_h_sources + gdk_private_h_sources

gdkenum_h = custom_target(
  'gdkenum_h',
  output  : 'gdkenumtypes.h',
  input   : gdk_headers,
  command : [mkenum, perl, glib_mkenums, meson.current_source_dir() + '/gdkenumtypes.h.template', '@OUTPUT@', '@INPUT@'])

gdkenum_c = custom_target(
  'gdkenum_c',
  output  : 'gdkenumtypes.c',
  input   : gdk_headers,
  depends : gdkenum_h,
  command : [mkenum, perl, glib_mkenums, meson.current_source_dir() + '/gdkenumtypes.c.template', '@OUTPUT@', '@INPUT@'])

gdkmarshal_h = custom_target(
  'gdkmarshal_h',
  output  : 'gdkmarshalers.h',
  input   : 'gdkmarshalers.list',
  command : [mkmarshal, glib_marshal, '_gdk_marshal', '@INPUT@', '@OUTPUT@']
)

gdkmarshal_c = custom_target(
  'gdkmarshal_c',
  output  : 'gdkmarshalers.c',
  input   : 'gdkmarshalers.list',
  command : [mkmarshal, glib_marshal, '_gdk_marshal', '@INPUT@', '@OUTPUT@']
)

cdata = configuration_data()
cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
cdata.set('GTK_MICRO_VERSION', gtk_micro_version)



if x11_enabled      cdata.set('GDK_WINDOWING_X11', '')      endif
if wayland_enabled  cdata.set('GDK_WINDOWING_WAYLAND', '')  endif
if broadway_enabled cdata.set('GDK_WINDOWING_BROADWAY', '') endif
gdkconfig = configure_file(
  input  : 'gdkconfig.h.meson',
  output : 'gdkconfig.h',
  configuration : cdata
)

gdkversionmacros = configure_file(
  input  : 'gdkversionmacros.h.in',
  output : 'gdkversionmacros.h',
  configuration: cdata
)

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
  pangocairo_dep
]

gdk_sources = [
  gdkconfig,
  gdk_sources,
  deprecated_gdk_sources,
  gdkenum_c, gdkenum_h,
  gdkmarshal_c, gdkmarshal_h,
  gdkresources,
  gdkversionmacros,
  gdk_headers
]

if x11_enabled
  gdk_deps += [
    xrender_dep,
    xi_dep,
    xext_dep,
    x11_dep,
    xcursor_dep,
    xdamage_dep,
    xfixes_dep,
    xcomposite_dep,
    xrandr_dep
  ]
  gdk_sources += [
    gdk_x_sources,
    gdk_x_private_sources
  ]
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


libgdk = shared_library('gdk',
  sources: [gdk_sources, gdkconfig, gdkenum_h],
  c_args: ['-DHAVE_CONFIG_H', '-DGDK_COMPILATION'],
  include_directories: [confinc, xinc, wlinc],
  dependencies: gdk_deps,
)


libgdk_dep = declare_dependency(
  sources: ['gdk.h', gdkconfig, gdkenum_h],
  depends: gdk_sources,
  include_directories: [confinc, xinc, wlinc],
  dependencies: gdk_deps,
  link_with: libgdk
)
