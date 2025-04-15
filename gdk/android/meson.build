gdk_android_sources = files([
  'gdkandroidinit.c',
  'gdkandroiddisplay.c',
  'gdkandroidmonitor.c',
  'gdkandroidevents.c',
  'gdkandroidsurface.c',
  'gdkandroidtoplevel.c',
  'gdkandroidpopup.c',
  'gdkandroidcairocontext.c',
  'gdkandroidclipboard.c',
  'gdkandroiddnd.c',
  'gdkandroidglcontext.c',
  'gdkandroidseat.c',
  'gdkandroiddevice.c',
  'gdkandroidkeymap.c',

  'gdkandroidcontentfile.c',

  'gdkandroidutils.c'
])

gdk_android_public_headers = files([
  'gdkandroidinit.h',
  'gdkandroiddisplay.h',
  'gdkandroidmonitor.h',
  'gdkandroidsurface.h',
  'gdkandroidtoplevel.h',
  'gdkandroidpopup.h',
  'gdkandroidseat.h',
  'gdkandroiddevice.h'
])

install_headers(gdk_android_public_headers, 'gdkandroid.h', subdir: 'gtk-4.0/gdk/android/')

if not cc.has_header('jni.h')
  error('the android backend requires a properly configured ndk')
endif

gdk_android_deps = [
  #dependency('jni'),
  cc.find_library('android'),
  cc.find_library('jnigraphics'),
  cc.find_library('nativewindow')
]

android_runtime = get_option('android-runtime')
if android_runtime.allowed()
  gdk_android_sources += files(['gdkandroidruntime.c'])
  gdk_android_deps += cc.find_library('log')
endif

libgdk_android = static_library('gdk-android',
  sources: [ gdk_android_sources, gdk_gen_headers ],
  include_directories: [ confinc, gdkinc, ],
  c_args: [ libgdk_c_args, common_cflags, ],
  dependencies: [ gdk_deps, gdk_android_deps ],
)
