private_headers = [
  'gsk-autocleanup.h',

  'gskcairoblurprivate.h',
  'gskcairorendererprivate.h',
  'gskdebugprivate.h',
  'gskdiffprivate.h',
  'gskglshaderprivate.h',
  'gskprivate.h',
  'gskprofilerprivate.h',
  'gskrendererprivate.h',
  'gskrendernodeprivate.h',
  'gskrendernodeparserprivate.h',
  'gskroundedrectprivate.h',
  'gsktransformprivate.h',

  # gsk/gl
  'glutilsprivate.h',
  'gskgldriverprivate.h',
  'gskglglyphcacheprivate.h',
  'gskgliconcacheprivate.h',
  'gskglimageprivate.h',
  'gskglnodesampleprivate.h',
  'gskglprofilerprivate.h',
  'gskglrendererprivate.h',
  'gskglrenderopsprivate.h',
  'gskglshaderbuilderprivate.h',
  'gskglshadowcacheprivate.h',
  'gskgltextureatlasprivate.h',
  'opbuffer.h',
  'stb_rect_pack.h',

  # gsk/vulkan
  'gskvulkanblendmodepipelineprivate.h',
  'gskvulkanblurpipelineprivate.h',
  'gskvulkanborderpipelineprivate.h',
  'gskvulkanboxshadowpipelineprivate.h',
  'gskvulkanbufferprivate.h',
  'gskvulkanclipprivate.h',
  'gskvulkancolorpipelineprivate.h',
  'gskvulkancolortextpipelineprivate.h',
  'gskvulkancommandpoolprivate.h',
  'gskvulkancrossfadepipelineprivate.h',
  'gskvulkaneffectpipelineprivate.h',
  'gskvulkanglyphcacheprivate.h',
  'gskvulkanimageprivate.h',
  'gskvulkanlineargradientpipelineprivate.h',
  'gskvulkanmemoryprivate.h',
  'gskvulkanpipelineprivate.h',
  'gskvulkanpushconstantsprivate.h',
  'gskvulkanrendererprivate.h',
  'gskvulkanrenderpassprivate.h',
  'gskvulkanrenderprivate.h',
  'gskvulkanshaderprivate.h',
  'gskvulkantextpipelineprivate.h',
  'gskvulkantexturepipelineprivate.h',
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
    mkdb_args: [
      '--ignore-files=' + ' '.join(private_headers),
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
    install: true,
  )
endif
