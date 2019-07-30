project('gtk', 'c',
        version: '3.96.0',
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1',
          # We only need c99, but glib needs GNU-specific features
          # https://github.com/mesonbuild/meson/issues/2289
          'c_std=gnu99',
        ],
        meson_version : '>= 0.48.0',
        license: 'LGPLv2.1+')

glib_major_req = 2
glib_minor_req = 59
glib_micro_req = 0

if glib_minor_req.is_odd()
  glib_min_required = 'GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req - 1)
else
  glib_min_required = 'GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req)
endif

if glib_minor_req.is_odd()
  glib_max_allowed = 'GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req + 1)
else
  glib_max_allowed = 'GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req)
endif

glib_req           = '>= @0@.@1@.@2@'.format(glib_major_req, glib_minor_req, glib_micro_req)
pango_req          = '>= 1.41.0'
fribidi_req        = '>= 0.19.7'
atk_req            = '>= 2.15.1'
cairo_req          = '>= 1.14.0'
gdk_pixbuf_req     = '>= 2.30.0'
introspection_req  = '>= 1.39.0'
wayland_proto_req  = '>= 1.14'
wayland_req        = '>= 1.14.91'
graphene_req       = '>= 1.9.1'
epoxy_req          = '>= 1.4'
cloudproviders_req = '>= 0.2.5'
xkbcommon_req      = '>= 0.2.0'

gnome = import('gnome')

add_project_arguments('-DG_LOG_USE_STRUCTURED=1', language: 'c')
add_project_arguments('-DGLIB_MIN_REQUIRED_VERSION=' + glib_min_required, language: 'c')
add_project_arguments('-DGLIB_MAX_ALLOWED_VERSION=' + glib_max_allowed, language: 'c')

# Making releases:
#  1. gtk_micro_version += 1;
#  2. gtk_interface_age += 1;
#  3. if any functions have been added, set gtk_interface_age to 0.
#  4. if backwards compatibility has been broken, we're in trouble
gtk_version       = meson.project_version()
gtk_major_version = gtk_version.split('.')[0].to_int()
gtk_minor_version = gtk_version.split('.')[1].to_int()
gtk_micro_version = gtk_version.split('.')[2].to_int()
gtk_interface_age = 0
add_project_arguments('-DGTK_VERSION="@0@"'.format(meson.project_version()), language: 'c')

add_project_arguments('-D_GNU_SOURCE', language: 'c')

gtk_debug_cflags = []
buildtype = get_option('buildtype')
if buildtype.startswith('debug')
  gtk_debug_cflags += '-DG_ENABLE_DEBUG'
  if buildtype == 'debug'
    gtk_debug_cflags += '-DG_ENABLE_CONSISTENCY_CHECKS'
  endif
elif buildtype == 'release'
  gtk_debug_cflags += '-DG_DISABLE_CAST_CHECKS'
endif

add_project_arguments(gtk_debug_cflags, language: 'c')

# Define a string for the earliest version that this release has
# backwards binary compatibility with for all interfaces a module
# might. Unless we add module-only API with lower stability
# guarantees, this should be unchanged until we break binary compat
# for GTK+.
gtk_binary_version = '4.0.0'

gtk_binary_age = 100 * gtk_minor_version + gtk_micro_version

gtk_soversion = '0.@0@.@1@'.format(gtk_binary_age - gtk_interface_age, gtk_interface_age)

gtk_api_version = '4.0'

x11_enabled            = get_option('x11-backend')
wayland_enabled        = get_option('wayland-backend')
broadway_enabled       = get_option('broadway-backend')
quartz_enabled         = get_option('quartz-backend')
win32_enabled          = get_option('win32-backend')

os_unix   = false
os_linux  = false
os_win32  = false
os_darwin = false

# Some windowing system backends depend on the platform we're
# building for, so we need to ensure they are disabled; in other
# cases, they are the only windowing system available, so we need
# to ensure they are enabled
if host_machine.system() == 'darwin'
  os_darwin = true
elif host_machine.system() == 'windows'
  os_win32 = true
elif host_machine.system() == 'linux'
  os_linux = true
endif
os_unix = not os_win32

if os_darwin
  wayland_enabled = false
else
  quartz_enabled = false
endif

if os_win32
  wayland_enabled = false
  x11_enabled = false
else
  win32_enabled = false
endif

gtk_prefix = get_option('prefix')
gtk_includedir = join_paths(gtk_prefix, get_option('includedir'))
gtk_libdir = join_paths(gtk_prefix, get_option('libdir'))
gtk_datadir = join_paths(gtk_prefix, get_option('datadir'))
gtk_localedir = join_paths(gtk_prefix, get_option('localedir'))
gtk_sysconfdir = join_paths(gtk_prefix, get_option('sysconfdir'))
gtk_applicationsdir = join_paths(gtk_datadir, 'applications')
gtk_schemasdir = join_paths(gtk_datadir, 'glib-2.0/schemas')
gtk_appdatadir = join_paths(gtk_datadir, 'metainfo')

cc = meson.get_compiler('c')

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('GTK_LOCALEDIR', gtk_localedir)
cdata.set_quoted('GTK_DATADIR', gtk_datadir)
cdata.set_quoted('GTK_LIBDIR', gtk_libdir)
cdata.set_quoted('GTK_SYSCONFDIR', gtk_sysconfdir)
cdata.set_quoted('GETTEXT_PACKAGE', 'gtk40')
cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
cdata.set('GTK_MICRO_VERSION', gtk_micro_version)
cdata.set('GTK_BINARY_AGE', gtk_binary_age)
cdata.set('GTK_INTERFACE_AGE', gtk_interface_age)

check_headers = [
  'crt/externs.h',
  'dev/evdev/input.h',
  'dlfcn.h',
  'ftw.h',
  'inttypes.h',
  'linux/input.h',
  'linux/memfd.h',
  'locale.h',
  'memory.h',
  'stdint.h',
  'stdlib.h',
  'strings.h',
  'string.h',
  'sys/mman.h',
  'sys/param.h',
  'sys/stat.h',
  'sys/sysinfo.h',
  'sys/systeminfo.h',
  'sys/time.h',
  'sys/types.h',
  'unistd.h',
]

foreach h : check_headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

# Maths functions might be implemented in libm
libm = cc.find_library('m', required: false)

check_functions = [
  'dcgettext',
  'getpagesize',
  'getresuid',
  'lstat',
  'mmap',
  'nearbyint',
  'posix_fallocate',
  '_lock_file',
  'flockfile',
  'mkstemp',
  'mallinfo',
  'round',
  'rint',
  'log2',
  'exp2',
  'sincos',
]

foreach func : check_functions
  if cc.has_function(func, dependencies: libm)
    cdata.set('HAVE_' + func.underscorify().to_upper(), 1)
  endif
endforeach

cdata.set('HAVE_DECL_ISINF', cc.has_header_symbol('math.h', 'isinf'))
cdata.set('HAVE_DECL_ISNAN', cc.has_header_symbol('math.h', 'isnan'))

# Disable deprecation checks for all libraries we depend on on stable branches.
# This is so newer versions of those libraries don't cause more warnings with
# a stable GTK version.
# We don't ever want to turn off deprecation warnings for master however, because
# that's where we get rid of deprecated API we use.
if gtk_minor_version.is_even()
  cdata.set('GLIB_DISABLE_DEPRECATION_WARNINGS', 1)
endif

# Compiler flags
if cc.get_id() == 'msvc'
  # Compiler options taken from msvc_recommended_pragmas.h
  # in GLib, based on _Win32_Programming_ by Rector and Newcomer
  test_cflags = []
  add_project_arguments('-FImsvc_recommended_pragmas.h', language: 'c')
  add_project_arguments('-D_USE_MATH_DEFINES', language: 'c')
elif cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  test_cflags = [
    '-fno-strict-aliasing',
    '-Wcast-align',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wignored-qualifiers',
    '-Wimplicit-function-declaration',
    '-Wlogical-op',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wpointer-arith',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wswitch-default',
    '-Wswitch-enum',
    '-Wundef',
    '-Wuninitialized',
    '-Wunused',
    '-Werror=address',
    '-Werror=array-bounds',
    '-Werror=empty-body',
    '-Werror=implicit',
    '-Werror=implicit-fallthrough',
    '-Werror=init-self',
    '-Werror=int-to-pointer-cast',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=missing-declarations',
    '-Werror=missing-prototypes',
    '-Werror=nonnull',
    '-Werror=pointer-to-int-cast',
    '-Werror=redundant-decls',
    '-Werror=return-type',
    '-Werror=sequence-point',
    '-Werror=trigraphs',
    '-Werror=vla',
    '-Werror=write-strings',
  ]
else
  test_cflags = []
endif

common_cflags = cc.get_supported_arguments(test_cflags)

# Symbol visibility
if get_option('default_library') != 'static'
  if os_win32
    cdata.set('DLL_EXPORT', true)
    cdata.set('_GDK_EXTERN', '__declspec(dllexport) extern')
    if cc.get_id() != 'msvc'
      common_cflags += ['-fvisibility=hidden']
    endif
  else
    cdata.set('_GDK_EXTERN', '__attribute__((visibility("default"))) extern')
    common_cflags += ['-fvisibility=hidden']
  endif
endif

common_ldflags = []

if os_unix and not os_darwin
  foreach ldflag: [ '-Wl,-Bsymbolic', '-Wl,-z,relro', '-Wl,-z,now', ]
    if cc.links('int main () { return 0; }', name: ldflag, args: ldflag)
      common_ldflags += [ ldflag ]
    endif
  endforeach
endif

# Maintain compatibility with autotools
if os_darwin
  common_ldflags += [ '-compatibility_version 1', '-current_version 1.0', ]
endif

confinc = include_directories('.')
gdkinc = include_directories('gdk')
gskinc = include_directories('gsk')
gtkinc = include_directories('gtk')
testinc = include_directories('tests')

# Dependencies
glib_dep       = dependency('glib-2.0', version: glib_req,
                            fallback : ['glib', 'libglib_dep'])
gobject_dep    = dependency('gobject-2.0', version: glib_req,
                            fallback : ['glib', 'libgobject_dep'])
if os_win32
  giowin32_dep = dependency('gio-windows-2.0', version: glib_req, required: win32_enabled,
                            fallback : ['glib', 'libgio_dep'])
endif
if os_unix
  giounix_dep  = dependency('gio-unix-2.0', version: glib_req, required: false,
                            fallback : ['glib', 'libgio_dep'])
endif
gmodule_dep    = dependency('gmodule-2.0', version: glib_req,
                            fallback : ['glib', 'libgmodule_dep'])
cairo_dep      = dependency('cairo', version: cairo_req, required : cc.get_id() != 'msvc')
cairogobj_dep  = dependency('cairo-gobject', version: cairo_req, required : cc.get_id() != 'msvc')
pango_dep      = dependency('pango', version: pango_req,
                            fallback : ['pango', 'libpango_dep'])
fribidi_dep    = dependency('fribidi', version: fribidi_req,
                            fallback : ['fribidi', 'libfribidi_dep'])

# Require PangoFT2 if on X11 or wayland
require_pangoft2 = wayland_enabled or x11_enabled
pangoft_dep    = dependency('pangoft2', required: false)

if require_pangoft2
  pangoft_dep    = dependency('pangoft2', fallback : ['pango', 'libpangoft2_dep'])
endif

if win32_enabled
  # for GTK_IM_CONTEXT_IME
  pangowin32_dep  = dependency('pangowin32')
endif

pangocairo_dep = dependency('pangocairo', version: cairo_req,
                            fallback : ['pango', 'libpangocairo_dep'])
pixbuf_dep     = dependency('gdk-pixbuf-2.0', version: gdk_pixbuf_req,
                            fallback : ['gdk-pixbuf', 'gdkpixbuf_dep'])
epoxy_dep      = dependency('epoxy', version: epoxy_req,
                            fallback: ['libepoxy', 'libepoxy_dep'])
atk_dep        = dependency('atk', version: atk_req)
harfbuzz_dep   = dependency('harfbuzz', version: '>= 0.9', required: false)
xkbdep         = dependency('xkbcommon', version: xkbcommon_req, required: wayland_enabled)
graphene_dep   = dependency('graphene-gobject-1.0', version: graphene_req,
                            fallback: ['graphene', 'graphene_dep'])
iso_codes_dep  = dependency('iso-codes', required: false)

fontconfig_dep = [] # only used in x11 backend
atkbridge_dep  = [] # only used in x11 backend

if os_win32
  platform_gio_dep = giowin32_dep
endif
if os_unix
  platform_gio_dep = giounix_dep
endif

if iso_codes_dep.found()
  cdata.set_quoted('ISO_CODES_PREFIX', iso_codes_dep.get_pkgconfig_variable('prefix'))
else
  cdata.set_quoted('ISO_CODES_PREFIX', '/usr')
endif

backend_immodules = []

pc_gdk_extra_libs = []

cairo_backends = []
foreach backend: [ ['cairo-xlib', cairo_req, x11_enabled],
                   ['cairo-win32', cairo_req, win32_enabled],
                   ['cairo-quartz', cairo_req, quartz_enabled],
                   ['cairo', cairo_req, broadway_enabled or wayland_enabled], ]
 backend_enabled = backend.get(2)
 cairo_backend_req = backend.get(1)
 cairo_backend = backend.get(0)
 if backend_enabled
   if dependency(cairo_backend, version: cairo_backend_req, required : cc.get_id() != 'msvc').found()
     cairo_backends += [ cairo_backend ]
   endif
 endif
endforeach

cairo_pkg_found = false
cairogobj_pkg_found = false

if cairo_dep.found()
  cairo_pkg_found = true
endif
if cairogobj_dep.found()
  cairogobj_pkg_found = true
endif

cairo_libs = []
if cc.get_id() == 'msvc'
  # Fallback depedency discovery for those on Visual Studio that do not generate
  # pkg-config files in their build systems for MSVC
  # Fallback for Cairo
  if not cairo_dep.found()
    if (cc.has_header('cairo.h') and cc.has_header('cairo-win32.h'))
      cairo_dep = cc.find_library('cairo')
      if cairo_dep.found()
        cairo_libs += '-lcairo'
      endif
    endif
  endif

  # Fallback for Cairo-GObject
  if not cairogobj_dep.found()
    if cc.has_header('cairo-gobject.h')
      cairogobj_dep = cc.find_library('cairo-gobject')
      if cairogobj_dep.found()
        cairo_libs += '-lcairo-gobject'
      endif
    endif
  endif

  # Fallback for HarfBuzz
  if not harfbuzz_dep.found()
    if cc.has_header('hb.h')
      harfbuzz_dep = cc.find_library('harfbuzz', required : false)
    endif
  endif
endif

cairo_csi_dep = cc.find_library('cairo-script-interpreter',
                                required: get_option('build-tests'))

if not harfbuzz_dep.found()
  harfbuzz_dep = dependency('harfbuzz', version: '>= 0.9', required: false,
                            fallback: ['harfbuzz', 'libharfbuzz_dep'])
endif

cdata.set('HAVE_CAIRO_SCRIPT_INTERPRETER', cairo_csi_dep.found())
cdata.set('HAVE_HARFBUZZ', harfbuzz_dep.found())
cdata.set('HAVE_PANGOFT', pangoft_dep.found())

atk_pkgs = ['atk']

wayland_pkgs = []
if wayland_enabled
  wlclientdep    = dependency('wayland-client', version:  wayland_req)
  wlprotocolsdep = dependency('wayland-protocols', version: wayland_proto_req)
  wlcursordep    = dependency('wayland-cursor', version: wayland_req)
  wlegldep       = dependency('wayland-egl')
  backend_immodules += ['wayland']

  wayland_pkgs = [
    'wayland-client', wayland_req,
    'wayland-protocols', wayland_proto_req,
    'xkbcommon', xkbcommon_req,
    'wayland-cursor', wayland_req,
    'wayland-egl',
  ]
endif

x11_pkgs = []
if x11_enabled
  xrandr_dep     = dependency('xrandr', version: '>= 1.2.99')
  xrandr15_dep   = dependency('xrandr', version: '>= 1.5', required: false)
  x11_dep        = dependency('x11')
  xrender_dep    = dependency('xrender')
  xi_dep         = dependency('xi')
  xext_dep       = dependency('xext')
  xcursor_dep    = dependency('xcursor', required: false)
  xdamage_dep    = dependency('xdamage', required: false)
  xfixes_dep     = dependency('xfixes', required: false)
  xcomposite_dep = dependency('xcomposite', required: false)
  fontconfig_dep = dependency('fontconfig')
  atkbridge_dep  = dependency('atk-bridge-2.0', version: atk_req)

  backend_immodules += ['xim']

  x11_pkgs = ['fontconfig', 'x11', 'xext', 'xi', 'xrandr']

  if xcursor_dep.found()
    x11_pkgs += ['xcursor']
  endif
  if xfixes_dep.found()
    x11_pkgs += ['xfixes']
  endif
  if xcomposite_dep.found()
    x11_pkgs += ['xcomposite']
  endif
  if xdamage_dep.found()
    x11_pkgs += ['xdamage']
  endif

  atk_pkgs += ['atk-bridge-2.0']

  cdata.set('HAVE_XDAMAGE', xdamage_dep.found())
  cdata.set('HAVE_XCURSOR', xcursor_dep.found())
  cdata.set('HAVE_XCOMPOSITE', xcomposite_dep.found())
  cdata.set('HAVE_XFIXES', xfixes_dep.found())

  if cc.has_function('XkbQueryExtension', dependencies: x11_dep,
                     prefix : '#include <X11/XKBlib.h>')
    cdata.set('HAVE_XKB', 1)
  endif

  if cc.has_function('XSyncQueryExtension', dependencies: xext_dep,
                     prefix: '''#include <X11/Xlib.h>
                                #include <X11/extensions/sync.h>''')
    cdata.set('HAVE_XSYNC', 1)
  endif

  if cc.has_function('XGetEventData', dependencies: x11_dep)
    cdata.set('HAVE_XGENERICEVENTS', 1)
  endif

  if not xi_dep.found() or not cc.has_header('X11/extensions/XInput2.h', dependencies: xi_dep)
    error('X11 backend enabled, but XInput2 not found.')
  endif

  # Note that we also check that the XIScrollClassInfo struct is defined,
  # because at least Ubuntu Oneiric seems to have XIAllowTouchEvents(),
  #   but not the XIScrollClassInfo struct
  has_allow_touch_evens = cc.has_function('XIAllowTouchEvents', dependencies: xi_dep)
  has_scroll_class_info = cc.has_member('XIScrollClassInfo', 'number', dependencies: xi_dep,
                                        prefix: '''#include <X11/Xlib.h>
                                                   #include <X11/extensions/XInput2.h>''')
  if has_allow_touch_evens and has_scroll_class_info
    cdata.set('XINPUT_2_2', 1)
  endif

  enable_xinerama = get_option('xinerama')
  if enable_xinerama != 'no'
    want_xinerama = enable_xinerama == 'yes'
    xinerama_dep = dependency('xinerama', required: want_xinerama)
    if xinerama_dep.found() and cc.has_header_symbol('X11/extensions/Xinerama.h', 'XineramaQueryExtension', dependencies: xinerama_dep)
      cdata.set('HAVE_XFREE_XINERAMA', 1)
      x11_pkgs += ['xinerama']
    endif
  else
    xinerama_dep = []
  endif

  cdata.set('HAVE_RANDR', xrandr_dep.found())
  cdata.set('HAVE_RANDR15', xrandr15_dep.found())
endif

if broadway_enabled
  pc_gdk_extra_libs += ['-lz']
  backend_immodules += ['broadway']
endif

if quartz_enabled
  pc_gdk_extra_libs += ['-framework Cocoa', '-framework Carbon']
  backend_immodules += ['quartz']
endif

extra_demo_ldflags = []
if win32_enabled
  pc_gdk_extra_libs += ['-lgdi32', '-limm32', '-lshell32', '-lole32']
  if cc.get_id() == 'msvc'
    # Since the demo programs are now built as pure GUI programs, we
    # need to pass in /entry:mainCRTStartup so that they will properly
    # link on Visual Studio builds
    extra_demo_ldflags = ['/entry:mainCRTStartup']
  else
    pc_gdk_extra_libs += ['-Wl,-luuid']
  endif
  pc_gdk_extra_libs += ['-lwinmm', '-ldwmapi', '-lsetupapi', '-lcfgmgr32']
  backend_immodules += ['ime']
endif

# Check for bind_textdomain_codeset, including -lintl if GLib brings it in by
# doing the same check as glib. We can't check that by linking to glib because
# it might be a subproject and hence not built yet.
if cc.has_function('ngettext')
  libintl_dep = []
  cdata.set('HAVE_BIND_TEXTDOMAIN_CODESET', 1)
else
  libintl_dep = cc.find_library('intl', required : false)
  if cc.has_function('bind_textdomain_codeset', dependencies: libintl_dep)
    cdata.set('HAVE_BIND_TEXTDOMAIN_CODESET', 1)
  else
    # Don't use subproject('proxy-libintl').get_variable('intl_dep') because that
    # makes the dependency unconditional. This way, people have the option of
    # either not providing the subproject or disabling it entirely with
    # --wrap-mode=nodownload or nofallback.
    libintl_dep = dependency('', required : false,
                             fallback: ['proxy-libintl', 'intl_dep'])
    if libintl_dep.found()
      cdata.set('HAVE_BIND_TEXTDOMAIN_CODESET', 1)
    endif
  endif
endif

if os_unix
  cdata.set('HAVE_GIO_UNIX', giounix_dep.found())
endif

# Check for Vulkan support
# TODO: move to gsk subfolder maybe? Or will it be used elsewhere too?
have_vulkan = false
vulkan_pkg_found = false
vulkan_dep = []

enable_vulkan = get_option('vulkan')
if enable_vulkan == 'no'
  message('Vulkan support explicitly disabled')
else
  vulkan_dep = dependency('vulkan', required: false)
  if vulkan_dep.found()
    have_vulkan = true
    vulkan_pkg_found = true
  else
    if cc.get_id() == 'msvc'
      vulkan_libname = 'vulkan-1'
    else
      vulkan_libname = 'vulkan'
    endif
    vulkan_dep = cc.find_library(vulkan_libname, required: false)
    if vulkan_dep.found() and cc.has_function('vkCreateInstance', dependencies: vulkan_dep) and cc.has_header('vulkan/vulkan.h')
      have_vulkan = true
      pc_gdk_extra_libs += ['-l@0@'.format(vulkan_libname)]
    endif
  endif
  if enable_vulkan == 'yes' and not have_vulkan
    error('Vulkan support not found, but was explicitly requested.')
  endif
endif

cloudproviders_enabled = get_option('cloudproviders')
if cloudproviders_enabled
  cloudproviders_dep = dependency('cloudproviders', required: true)
  if cloudproviders_dep.found()
    cdata.set('HAVE_CLOUDPROVIDERS', cloudproviders_dep.found())
  else
    error('Cloudproviders support not found, but was explicitly requested.')
  endif
endif

profiler_enabled = get_option('profiler')
if profiler_enabled
  profiler_dep = dependency('sysprof-capture-3', static: true, required: true)
  if profiler_dep.found()
    cdata.set('HAVE_SYSPROF_CAPTURE', profiler_dep.found())
  else
    error('Profiler support not found, but was explicitly requested.')
  endif
endif

graphene_dep_type = graphene_dep.type_name()
if graphene_dep_type == 'pkgconfig'
  graphene_has_sse2 = graphene_dep.get_pkgconfig_variable('graphene_has_sse2') == '1'
  graphene_has_gcc = graphene_dep.get_pkgconfig_variable('graphene_has_gcc') == '1'
else
  graphene_simd = subproject('graphene').get_variable('graphene_simd')
  graphene_has_sse2 = graphene_simd.contains('sse2')
  graphene_has_gcc = graphene_simd.contains('gcc')
endif

if graphene_has_sse2 or graphene_has_gcc
  message('Need aligned memory due to the use of SSE2 or GCC vector instructions')

  if os_win32 and cc.get_id() == 'gcc'
    add_project_arguments(['-mstackrealign'], language: 'c')
  endif
endif

subdir('gtk/css')
subdir('gdk')
subdir('gsk')
subdir('gtk')
subdir('modules')
if get_option('demos')
  subdir('demos')
endif
if get_option('build-tests')
  subdir('tests')
  subdir('testsuite')
endif
if get_option('build-examples')
  subdir('examples')
endif

# config.h
configure_file(input: 'config.h.meson',
               output: 'config.h',
               configuration: cdata)

# pkg-config files - bit of a mess all of this
pkgconf = configuration_data()

pkgconf.set('prefix', get_option('prefix'))
pkgconf.set('exec_prefix', '${prefix}')
pkgconf.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
pkgconf.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
pkgconf.set('GTK_API_VERSION', gtk_api_version)
pkgconf.set('VERSION', meson.project_version())
pkgconf.set('GTK_BINARY_VERSION', gtk_binary_version)
pkgconf.set('host', '@0@-@1@'.format(host_machine.cpu_family(), host_machine.system())) # FIXME

# Requires
pango_pkgname = win32_enabled ? 'pangowin32' : 'pango'
gdk_packages = ' '.join([ pango_pkgname, pango_req,
                       'pangocairo', pango_req,
                       'gdk-pixbuf-2.0', gdk_pixbuf_req ])

if cairo_pkg_found
  gdk_packages += ' '.join([ ' cairo', cairo_req ])
endif
if cairogobj_pkg_found
  gdk_packages += ' '.join([ ' cairo-gobject', cairo_req ])
endif

if vulkan_pkg_found
  gdk_packages += 'vulkan'
endif

pkgconf.set('GDK_PACKAGES', gdk_packages)
pkgconf.set('GSK_PACKAGES',
            ' '.join([ 'graphene-gobject-1.0', graphene_req ]))
pkgconf.set('GTK_PACKAGES',
            ' '.join([ 'atk', atk_req,
                       'gio-2.0', glib_req ]))

# Requires.private
pc_gdk_extra_libs += cairo_libs

gio_pkgname = os_unix ? 'gio-unix-2.0' : 'gio-2.0'
pkgconf.set('GDK_PRIVATE_PACKAGES',
            ' '.join([ gio_pkgname, glib_req,
                       'epoxy', epoxy_req ] + x11_pkgs + wayland_pkgs + cairo_backends))
pkgconf.set('GSK_PRIVATE_PACKAGES', '') # all already in GDK_PRIVATE_PACKAGES
pangoft2_pkgs = (wayland_enabled or x11_enabled) ? ['pangoft2'] : []
pkgconf.set('GTK_PRIVATE_PACKAGES', ' '.join(atk_pkgs + pangoft2_pkgs))

pkgconf.set('GDK_EXTRA_LIBS', ' '.join(pc_gdk_extra_libs))
pkgconf.set('GSK_EXTRA_LIBS', '')
pkgconf.set('GTK_EXTRA_LIBS', '')

pkgconf.set('GDK_EXTRA_CFLAGS', '')
pkgconf.set('GSK_EXTRA_CFLAGS', '')
pkgconf.set('GTK_EXTRA_CFLAGS', '')

pkg_install_dir = join_paths(get_option('libdir'), 'pkgconfig')

pkgs = [ 'gtk4.pc' ]

pkg_targets = ''
foreach backend: [ 'broadway', 'quartz', 'wayland', 'win32', 'x11', ]
  if get_variable('@0@_enabled'.format(backend))
    pkgs += ['gtk4-@0@.pc'.format(backend)]
    pkg_targets += ' ' + backend
  endif
endforeach
pkgconf.set('GDK_BACKENDS', pkg_targets.strip())

foreach pkg: pkgs
  configure_file(input: 'gtk4.pc.in',
                 output: pkg,
                 configuration: pkgconf,
                 install_dir: pkg_install_dir)
endforeach

if os_unix
  configure_file(input: 'gtk4-unix-print.pc.in',
                 output: 'gtk4-unix-print.pc',
                 configuration: pkgconf,
                 install_dir: pkg_install_dir)
endif

subdir('po')
subdir('po-properties')

subdir('docs/tools')
subdir('docs/reference')

# Keep this in sync with post-install.sh expected arguments
meson.add_install_script('build-aux/meson/post-install.py',
                         gtk_api_version,
                         gtk_binary_version,
                         gtk_libdir,
                         gtk_datadir)

summary = [
  '',
  '------',
  'GTK @0@ (@1@)'.format(gtk_version, gtk_api_version),
  '',
  '  Display backends: @0@'.format(pkg_targets.strip()),
  '    Print backends: @0@'.format(' '.join(print_backends)),
  '    Media backends: @0@'.format(' '.join(media_backends)),
  '    Vulkan support: @0@'.format(have_vulkan),
  '     Cloud support: @0@'.format(get_option('cloudproviders')),
  '    Colord support: @0@'.format(get_option('colord')),
  '          Profiler: @0@'.format(get_option('profiler')),
  '     Introspection: @0@'.format(get_option('introspection')),
  '     Documentation: @0@'.format(get_option('gtk_doc')),
  '         Man pages: @0@'.format(get_option('man-pages')),
  '       Build tests: @0@'.format(get_option('build-tests')),
  '     Install tests: @0@'.format(get_option('install-tests')),
  '             Demos: @0@'.format(get_option('demos')),
  '          Examples: @0@'.format(get_option('build-examples')),
  'Directories:',
  '            prefix: @0@'.format(gtk_prefix),
  '        includedir: @0@'.format(gtk_includedir),
  '            libdir: @0@'.format(gtk_libdir),
  '           datadir: @0@'.format(gtk_datadir),
  '------',
  ''
]

message('\n'.join(summary))
