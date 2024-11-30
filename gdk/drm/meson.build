gdk_drm_deps = [
  dependency('libdrm'),
  dependency('gbm'),
  dependency('libinput', version: '>= 1.10'),
  dependency('libudev'),
  xkbdep,
]

gdk_drm_sources = files([
  'gdkdrmcairocontext.c',
  'gdkdrmclipboard.c',
  'gdkdrmdevice.c',
  'gdkdrmdisplay.c',
  'gdkdrmglcontext.c',
  'gdkdrminput.c',
  'gdkdrmkeymap.c',
  'gdkdrmmonitor.c',
  'gdkdrmpopupsurface.c',
  'gdkdrmseat.c',
  'gdkdrmsurface.c',
  'gdkdrmtoplevelsurface.c',
])

gdk_drm_public_headers = files([
  'gdkdrm.h',
  'gdkdrmdevice.h',
  'gdkdrmdisplay.h',
  'gdkdrmglcontext.h',
  'gdkdrmkeymap.h',
  'gdkdrmmonitor.h',
  'gdkdrmseat.h',
  'gdkdrmsurface.h',
])

install_headers(gdk_drm_public_headers, 'gdkdrm.h', subdir: 'gtk-4.0/gdk/drm/')

libgdk_drm = static_library('gdk-drm',
              sources: [ gdk_drm_sources, gdk_gen_headers ],
  include_directories: [ confinc, gdkinc, ],
               c_args: [ libgdk_c_args, common_cflags, ],
            link_with: [],
         dependencies: [ gdk_deps, gdk_drm_deps, ],
)
