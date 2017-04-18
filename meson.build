project('gtk+-4.0', 'c',
        version: '3.89.50',
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1'
        ],
        meson_version : '>= 0.39.1',
        license: 'LGPLv2.1+')

glib_req          = '>= 2.49.4'
glib_min_required = 'GLIB_VERSION_2_48'
glib_max_allowed  = 'GLIB_VERSION_2_50'
pango_req         = '>= 1.37.3'
atk_req           = '>= 2.15.1'
cairo_req         = '>= 1.14.0'
gdk_pixbuf_req    = '>= 2.30.0'
introspection_req = '>= 1.39.0'
wayland_proto_req = '>= 1.7'
wayland_req       = '>= 1.9.91'
mirclient_req     = '>= 0.22.0'
mircookie_req     = '>= 0.17.0'
graphene_req      = '>= 1.5.1'
epoxy_req         = '>= 1.0'

gnome = import('gnome')

add_project_arguments('-DG_LOG_USE_STRUCTURED=1', language: 'c')
add_project_arguments('-DG_ENABLE_DEBUG', language: 'c')
add_project_arguments('-DG_ENABLE_CONSISTENCY_CHECKS', language: 'c')
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
add_project_arguments('-DGTK_VERSION="' + meson.project_version() + '"', language: 'c')

add_project_arguments('-D_GNU_SOURCE', language : 'c')

# Define a string for the earliest version that this release has
# backwards binary compatibility with for all interfaces a module
# might. Unless we add module-only API with lower stability
# guarantees, this should be unchanged until we break binary compat
# for GTK+.
gtk_binary_version = '4.0.0'

gtk_binary_age = 100 * gtk_minor_version + gtk_micro_version

gtk_api_version = '4.0'

x11_enabled      = get_option('enable-x11-backend')
wayland_enabled  = get_option('enable-wayland-backend')
broadway_enabled = get_option('enable-broadway-backend')
mir_enabled      = get_option('enable-mir-backend')
quartz_enabled   = false # FIXME
win32_enabled    = false # FIXME

os_unix   = false
os_linux  = false
os_win32  = false
os_darwin = false

if host_machine.system().contains('darwin')
  os_darwin = true
elif host_machine.system().contains('mingw')
  os_win32 = true
elif host_machine.system().contains('linux')
  os_linux = true
endif

os_unix = not os_win32

gtk_prefix = get_option('prefix')
gtk_libdir = join_paths(gtk_prefix, get_option('libdir'))
gtk_datadir = join_paths(gtk_prefix, get_option('datadir'))
gtk_localedir = join_paths(gtk_prefix, get_option('localedir'))
gtk_sysconfdir = join_paths(gtk_prefix, get_option('sysconfdir'))

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
  'dlfcn.h',
  'ftw.h',
  'inttypes.h',
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
libm = cc.find_library('m', required : false)

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
  if cc.has_function(func, dependencies : libm)
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
if gtk_minor_version % 2 == 0
  cdata.set('GLIB_DISABLE_DEPRECATION_WARNINGS', 1)
endif


confinc = include_directories('.')
gdkinc = include_directories('gdk')
gtkinc = include_directories('gtk')

glib_dep       = dependency('glib-2.0', version: glib_req)
giounix_dep    = dependency('gio-unix-2.0', version: glib_req, required : false)
pango_dep      = dependency('pango', version: pango_req)
pangoft_dep    = dependency('pangoft2', required: wayland_enabled or x11_enabled)
cairo_dep      = dependency('cairo', version: cairo_req)
pangocairo_dep = dependency('pangocairo', version: cairo_req)
cairogobj_dep  = dependency('cairo-gobject', version: cairo_req)
pixbuf_dep     = dependency('gdk-pixbuf-2.0', version: gdk_pixbuf_req)
epoxy_dep      = dependency('epoxy', version: epoxy_req)
atk_dep        = dependency('atk', version: atk_req)
gmodule_dep    = dependency('gmodule-2.0', version: glib_req)
harfbuzz_dep   = dependency('harfbuzz', version: '>= 0.9', required: false)
xkbdep         = dependency('xkbcommon', version: '>= 0.2.0')
graphene_dep   = dependency('graphene-1.0', version: graphene_req,
                            fallback : ['graphene', 'graphene_dep'])

fontconfig_dep = [] # only used in x11 backend
atkbridge_dep  = [] # only used in x11 backend

backend_immodules = []

pc_gdk_extra_libs = []

cairo_backends = []
foreach backend : [['cairo-xlib', x11_enabled],
                   ['cairo-win32', win32_enabled],
                   ['cairo-quartz', quartz_enabled],
                   ['cairo', broadway_enabled or wayland_enabled or mir_enabled]]
 if backend[1]
   dependency(backend[0], version : cairo_req)
   cairo_backends += [backend[0]]
 endif
endforeach

atk_pkgs = ['atk']

wayland_pkgs = []
if wayland_enabled
  wlclientdep    = dependency('wayland-client', version:  wayland_req)
  wlprotocolsdep = dependency('wayland-protocols', version: wayland_proto_req)
  wlcursordep    = dependency('wayland-cursor', version: wayland_req)
  wlegldep       = dependency('wayland-egl')

  wayland_pkgs = [
    'wayland-client', wayland_req,
    'wayland-protocols', wayland_proto_req,
    'xkbcommon', '>= 0.2.0',
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

  if cc.has_function('XkbQueryExtension', dependencies : x11_dep,
                     prefix : '#include <X11/XKBlib.h>')
    cdata.set('HAVE_XKB', 1)
  endif

  if cc.has_function('XSyncQueryExtension', dependencies : xext_dep,
                     prefix : '''#include <X11/Xlib.h>
                                 #include <X11/extensions/sync.h>''')
    cdata.set('HAVE_XSYNC', 1)
  endif

  if cc.has_function('XGetEventData', dependencies : x11_dep)
    cdata.set('HAVE_XGENERICEVENTS', 1)
  endif

  if xi_dep.found() and cc.has_header('X11/extensions/XInput2.h', dependencies : xi_dep)
    cdata.set('XINPUT_2', 1)
    # Note that we also check that the XIScrollClassInfo struct is defined,
    # because at least Ubuntu Oneiric seems to have XIAllowTouchEvents(),
    # but not the XIScrollClassInfo struct
    prefix = '#include<X11/Xlib.h>\n#include<X11/extensions/XInput2.h>'
    if cc.has_function('XIAllowTouchEvents', dependencies : xi_dep) and cc.has_member('XIScrollClassInfo', 'number', prefix: prefix, dependencies : xi_dep)
      cdata.set('XINPUT_2_2', 1)
    endif
  endif

  enable_xinerama = get_option('enable-xinerama')
  if enable_xinerama != 'no'
    want_xinerama = enable_xinerama == 'yes'
    xinerama_dep = dependency('xinerama', required : want_xinerama)
    if xinerama_dep.found() and cc.has_header_symbol('X11/extensions/Xinerama.h', 'XineramaQueryExtension', dependencies : xinerama_dep)
      cdata.set('HAVE_XFREE_XINERAMA', 1)
      x11_pkgs += ['xinerama']
    elif want_xinerama
      error('No function XineramaQueryExtension in xinerama dependency which was explicitly requested.')
    endif
  else
    xinerama_dep = []
  endif

  cdata.set('HAVE_RANDR', xrandr_dep.found())
  cdata.set('HAVE_RANDR15', xrandr15_dep.found())
endif

mir_pkgs = []
if mir_enabled
  mirclient_dep = dependency('mirclient', version : mirclient_req)
  mircookie_dep = dependency('mircookie', version : mircookie_req)
  libcontent_hub_glib_dep = dependency('libcontenthub-glib')

  mir_pkgs = [
    'mirclient', mirclient_req,
    'mircookie', mircookie_req,
    'libcontent-hub-glib',
  ]
endif

if broadway_enabled
  pc_gdk_extra_libs += ['-lz']
  backend_immodules += ['broadway']
endif

if quartz_enabled
  pc_gdk_extra_libs += ['-framework Cocoa', '-framework Carbon']
  backend_immodules += ['quartz']
endif

if win32_enabled
  pc_gdk_extra_libs += ['-lgdi32', '-limm32', '-lshell32', '-lole32',
                        '-Wl,-luuid', '-lwinmm', '-ldwmapi', '-lsetupapi',
                        '-lcfgmgr32']
  backend_immodules += ['ime']
endif

# Check for bind_textdomain_codeset, including -lintl if GLib brings it in
if cc.has_function('bind_textdomain_codeset', dependencies: glib_dep)
  cdata.set('HAVE_BIND_TEXTDOMAIN_CODESET', 1)
endif

cdata.set('HAVE_GIO_UNIX', giounix_dep.found())

# Check for Vulkan support
# TODO: move to gsk subfolder maybe? Or will it be used elsewhere too?
have_vulkan = false
vulkan_lib = []
enable_vulkan = get_option('enable-vulkan')
if enable_vulkan != 'no'
  vulkan_lib = cc.find_library('vulkan', required : false)
  if vulkan_lib.found() and cc.has_function('vkCreateInstance', dependencies : vulkan_lib) and cc.has_header('vulkan/vulkan.h')
    have_vulkan = true
    pc_gdk_extra_libs += ['-lvulkan']
  elif enable_vulkan == 'yes'
    error('Vulkan support not found, but was explicitly requested.')
  endif
else
  message('Vulkan support explicitly disabled')
endif

subdir('modules/input')
subdir('gdk')
subdir('gsk')
subdir('gtk')
subdir('demos')
subdir('modules')
subdir('tests')
subdir('testsuite')
subdir('examples')

# config.h

configure_file(
  input : 'config.h.meson',
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
hm = host_machine
pkgconf.set('host', '@0@-@1@'.format(hm.cpu_family(), hm.system())) # FIXME

# Requires
pango_pkgname = win32_enabled ? 'pangowin32' : 'pango'
pkgconf.set('GDK_PACKAGES', ' '.join([
  pango_pkgname, pango_req,
  'pangocairo', pango_req,
  'gdk-pixbuf-2.0', gdk_pixbuf_req,
  'cairo', cairo_req,
  'cairo-gobject', cairo_req]))
pkgconf.set('GSK_PACKAGES', ' '.join([
  #'gdk-pixbuf-2.0', gdk_pixbuf_req,
  #'cairo', cairo_req,
  #'cairo-gobject', cairo_req,
  'graphene-gobject-1.0', graphene_req]))
pkgconf.set('GTK_PACKAGES', ' '.join([
  'atk', atk_req,
  #'cairo', cairo_req,
  #'cairo-gobject', cairo_req,
  #'gdk-pixbuf-2.0', gdk_pixbuf_req,
  'gio-2.0', glib_req]))

# Requires.private
gio_pkgname = os_unix ? 'gio-unix-2.0' : 'gio-2.0'
pkgconf.set('GDK_PRIVATE_PACKAGES', ' '.join([
  gio_pkgname, glib_req,
  'epoxy', epoxy_req] + x11_pkgs + wayland_pkgs + mir_pkgs + cairo_backends))
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

pkgs = ['gtk+-4.0.pc']

pkg_targets = ''
foreach backend : ['broadway', 'mir', 'quartz', 'wayland', 'win32', 'x11']
  if get_variable('@0@_enabled'.format(backend))
    pkgs += ['gtk+-@0@-4.0.pc'.format(backend)]
    pkg_targets += ' ' + backend
  endif
endforeach
pkgconf.set('GDK_BACKENDS', pkg_targets.strip())

foreach pkg : pkgs
  configure_file(input : 'gtk+-4.0.pc.in',
    output : pkg,
    configuration : pkgconf,
    install_dir : pkg_install_dir)
endforeach

if host_machine.system() != 'windows'
  configure_file(input : 'gtk+-unix-print-4.0.pc.in',
    output : 'gtk+-unix-print-4.0.pc',
    configuration : pkgconf,
    install_dir : pkg_install_dir)
endif

subdir('po')
subdir('po-properties')
