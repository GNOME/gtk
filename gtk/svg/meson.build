gtk_svg_public_sources = files([
  'gtksvg.c',
  'gtksvgclip.c',
  'gtksvgcolor.c',
  'gtksvgcolorstop.c',
  'gtksvgcontentfit.c',
  'gtksvgdasharray.c',
  'gtksvgelement.c',
  'gtksvgelementtype.c',
  'gtksvgenum.c',
  'gtksvgerror.c',
  'gtksvgfilter.c',
  'gtksvgfilterfunctions.c',
  'gtksvgfilterref.c',
  'gtksvgfiltertype.c',
  'gtksvghref.c',
  'gtksvgkeyword.c',
  'gtksvglanguage.c',
  'gtksvglocation.c',
  'gtksvgmask.c',
  'gtksvgnumber.c',
  'gtksvgnumbers.c',
  'gtksvgorient.c',
  'gtksvgpaint.c',
  'gtksvgpangoutils.c',
  'gtksvgpath.c',
  'gtksvgpathdata.c',
  'gtksvgpathutils.c',
  'gtksvgproperty.c',
  'gtksvgstring.c',
  'gtksvgstringlist.c',
  'gtksvgstringutils.c',
  'gtksvgtextdecoration.c',
  'gtksvgtransform.c',
  'gtksvgunit.c',
  'gtksvgutils.c',
  'gtksvgvalue.c',
  'gtksvgviewbox.c',
  'gtksvganimation.c',
])

gtk_svg_public_headers = files([
  'gtksvg.h',
])

install_headers(gtk_svg_public_headers, 'gtksvg.h', subdir: 'gtk-4.0/gtk/svg')

gtk_svg_deps = [
  libm,
  glib_dep,
  gobject_dep,
  platform_gio_dep,
  cairo_dep,
  pango_dep,
  pangoft_dep,
  pixbuf_dep,
  libgsk_dep,
]

gtk_svg_enums = gnome.mkenums('gtksvgenumtypes',
  sources: gtk_svg_public_headers,
  c_template: 'gtksvgenumtypes.c.template',
  h_template: 'gtksvgenumtypes.h.template',
  install_dir: join_paths(gtk_includedir, 'gtk-4.0/gtk/svg'),
  install_header: true,
)

gtk_svg_enum_h = gtk_svg_enums[1]

libgtk_svg = static_library('gtk_svg',
  override_options: [ 'unity=on', 'unity_size=1000' ],
  sources: [
    gtk_svg_public_sources,
    gtktypebuiltins_h,
    gtk_svg_enums,
    gdkversionmacros_h,
    gdk_visibility_h,
  ],
  dependencies: gtk_svg_deps,
  include_directories: [ confinc, gdkinc, gskinc, gtkinc ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gtk"',
  ] + common_cflags,
)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgtk_svg_dep = declare_dependency(include_directories: [ confinc, ],
  sources: [ gtk_svg_enum_h ],
  dependencies: gtk_svg_deps,
)
