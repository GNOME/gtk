deprecated_gdk_sources = ['deprecated/gdkcolor.c']
gdk_sources = [
#  'fallback-c89.c',
  'gdk-private.c',
  'gdk.c',
  'gdkapplaunchcontext.c',
  'gdkcairo.c',
  'gdkcursor.c',
  'gdkdeprecated.c',
  'gdkdevice.c',
  'gdkdevicemanager.c',
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
]

gdk_public_h_sources = [
  'gdk.h',
  'gdk-autocleanup.h',
  'gdkapplaunchcontext.h',
  'gdkcairo.h',
  'gdkcursor.h',
  'gdkdevice.h',
  'gdkdevicemanager.h',
  'gdkdisplay.h',
  'gdkdisplaymanager.h',
  'gdkdnd.h',
  'gdkevents.h',
  'gdkframetimings.h',
  'gdkglcontext.h',
  'gdkkeys.h',
  'gdkkeysyms.h',
  'gdkkeysyms-compat.h',
  'gdkmain.h',
  'gdkpango.h',
  'gdkframeclock.h',
  'gdkpixbuf.h',
  'gdkprivate.h',
  'gdkproperty.h',
  'gdkrectangle.h',
  'gdkrgba.h',
  'gdkscreen.h',
  'gdkselection.h',
  'gdktestutils.h',
  'gdkthreads.h',
  'gdktypes.h',
  'gdkvisual.h',
  'gdkwindow.h']

# HACK to get it running.
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
  'x11/gdkdevicemanagerprivate-core.h',
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
  'x11/gdkprivate-x11.h',
  'x11/xsettings-client.h',
  'x11/xsettings-client.c',
]

deprecated_h_sources = ['deprecated/gdkcolor.h']

gdk_headers = gdk_public_h_sources + deprecated_h_sources

mkenum = find_program('build_enum.py')
perl = find_program('perl')
glib_mkenums = find_program('glib-mkenums')
mkmarshal = find_program('build_marshal.py')
glib_marshal = find_program('glib-genmarshal')

gdkenum_h = custom_target('gdkenum_h',
  output : 'gdkenumtypes.h',
  input : gdk_headers,
  command : [mkenum, perl, glib_mkenums, '@OUTPUT@', '@INPUT@'])

gdkenum_c = custom_target('gdkenum_c',
  output : 'gdkenumtypes.c',
  input : gdk_headers,
  depends : [gdkenum_h],
  command : [mkenum, perl, glib_mkenums, '@OUTPUT@', '@INPUT@'])

gdkmarshal_h = custom_target('gdkmarshal_h',
  output : 'gdkmarshall.h',
  input : 'gdkmarshalers.list',
  command : [mkmarshal, glib_marshal, '@INPUT@', '@OUTPUT@'])

gdkmarshal_c = custom_target('gdkmarshal_c',
  output : 'gdkmarshall.c',
  input : 'gdkmarshalers.list',
  command : [mkmarshal, glib_marshal, '@INPUT@', '@OUTPUT@'])

configure_file(input : 'gdkconfig.h.in',
  output : 'gdkconfig.h',
  configuration : configuration_data())

xinc = include_directories('x11')

libgdk = shared_library('gdk',
  gdk_sources, deprecated_gdk_sources,
  gdkenum_c, gdkenum_h,
  gdkmarshal_h, gdkmarshal_c,
  gdk_x_sources,
  c_args: ['-DHAVE_CONFIG_H', '-DGDK_COMPILATION', '-DGDK_WINDOWING_X11'],
  include_directories: [confinc, xinc],
  dependencies: [
    mlib,
    xrender_dep,
    xi_dep,
    xext_dep,
    x11_dep,
    fontconfig_dep,
    epoxy_dep,
    giounix_dep,
    glib_dep,
    pangocairo_dep,
    cairogobj_dep,
    pango_dep,
    cairo_dep,
    pixbuf_dep
  ])
