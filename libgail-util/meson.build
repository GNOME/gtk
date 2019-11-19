gailutil_sources = files(
  'gailmisc.c',
  'gailtextutil.c',
)

gailutil_public_headers = files(
  'gailmisc.h',
  'gailtextutil.h',
  'gail-util.h',
  )

install_headers(gailutil_public_headers, subdir : 'gail-3.0/libgail-util')

gailutil_link_args = common_ldflags

if cc.get_id() == 'msvc'
  gailutil_link_args += '-def:@0@/gailutil.def'.format(meson.current_source_dir())
endif

libgailutil = library('gailutil-3',
                      soversion: gtk_soversion,
                      version: gail_library_version,
                      darwin_versions: ['1', '1.0'],
                      sources: [gailutil_sources],
                      dependencies: libgtk_dep,
                      include_directories: [confinc, gtkinc],
                      c_args: [
                        '-DGDK_DISABLE_DEPRECATED',
                        '-DGTK_DISABLE_DEPRECATED',
                      ] + common_cflags,
                      link_args: gailutil_link_args,
                      install: true)
