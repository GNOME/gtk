private_headers = [
  'gskcairoblurprivate.h',
  'gskcairorendererprivate.h',
  'gskdebugprivate.h',
  'gskgldriverprivate.h',
  'gskglprofilerprivate.h',
  'gskglrendererprivate.h',
  'gskprivate.h',
  'gskprofilerprivate.h',
  'gskrendererprivate.h',
  'gskrendernodeprivate.h',
  'gskroundedrectprivate.h',
  'gskshaderbuilderprivate.h',
  'gsktextureprivate.h',
  'gskvulkanblendpipelineprivate.h',
  'gskvulkanborderpipelineprivate.h',
  'gskvulkanboxshadowpipelineprivate.h',
  'gskvulkanbufferprivate.h',
  'gskvulkanclipprivate.h',
  'gskvulkancolorpipelineprivate.h',
  'gskvulkancommandpoolprivate.h',
  'gskvulkaneffectpipelineprivate.h',
  'gskvulkanimageprivate.h',
  'gskvulkanlineargradientpipelineprivate.h',
  'gskvulkanmemoryprivate.h',
  'gskvulkanpipelineprivate.h',
  'gskvulkanpushconstantsprivate.h',
  'gskvulkanrendererprivate.h',
  'gskvulkanrenderpassprivate.h',
  'gskvulkanrenderprivate.h',
  'gskvulkanshaderprivate.h',
]

images = [
]

if get_option('gtk_doc')
  configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

  gnome.gtkdoc('gsk4',
               mode: 'none',
               main_xml: 'gsk4-docs.xml',
               src_dir: [
                 gskinc,
               ],
               dependencies: libgtk_dep,
               gobject_typesfile: join_paths(meson.current_source_dir(), 'gsk4.types'),
               scan_args: [
                 '--ignore-decorators=_GDK_EXTERN',
                 '--ignore-headers=' + ' '.join(private_headers),
               ],
               fixxref_args: [
                 '--html-dir=@0@'.format(docpath),
                 '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
                 '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
                 '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
                 '--extra-dir=@0@'.format(cairo_docpath),
                 '--extra-dir=../gdk',
               ],
               html_assets: images,
               install: true)
endif
