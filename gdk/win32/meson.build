gdk_win32_public_sources = files([
  'gdkcursor-win32.c',
  'gdkd3d12texture.c',
  'gdkd3d12texturebuilder.c',
  'gdkdisplay-win32.c',
  'gdkdisplaymanager-win32.c',
  'gdkdrag-win32.c',
  'gdkglcontext-win32.c',
  'gdkglcontext-win32-wgl.c',
  'gdkwin32misc.c',
  'gdksurface-win32.c',
  'gdkevents-win32.c',
  'gdkmonitor-win32.c',
])

gdk_win32_sources = gdk_win32_public_sources + files([
  'gdkcairocontext-win32.c',
  'gdkclipboard-win32.c',
  'gdkclipdrop-win32.c',
  'gdkd3d12utils.c',
  'gdkdevicemanager-win32.c',
  'gdkdevice-virtual.c',
  'gdkdevice-win32.c',
  'gdkdevice-winpointer.c',
  'gdkdevice-wintab.c',
  'gdkdrop-win32.c',
  'gdkglcontext-win32-wgl-private.c',
  'gdkhdataoutputstream-win32.c',
  'gdkinput-dmanipulation.c',
  'gdkinput-winpointer.c',
  'gdkkeys-win32.c',
  'gdkkeys-win32-impl.c',
  'gdkkeys-win32-impl-wow64.c',
  'gdkmain-win32.c',
  'gdkvulkancontext-win32.c',
  'gdkwin32cursor.h',
  'gdkwin32display.h',
  'gdkwin32messagesource.c',
  'gdkwin32keys.h',
])

gdk_win32_public_headers = files([
  'gdkd3d12texture.h',
  'gdkd3d12texturebuilder.h',
  'gdkwin32cursor.h',
  'gdkwin32display.h',
  'gdkwin32displaymanager.h',
  'gdkwin32dnd.h',
  'gdkwin32glcontext.h',
  'gdkwin32misc.h',
  'gdkwin32monitor.h',
  'gdkwin32surface.h',
])

install_headers(gdk_win32_public_headers, 'gdkwin32.h', subdir: 'gtk-4.0/gdk/win32/')

if have_egl
  gdk_win32_sources += ['gdkglcontext-win32-egl.c']
endif

d3d12_dep = [
  cc.find_library('d3d11'),
  cc.find_library('d3d12'),
  cc.find_library('dcomp'),
  cc.find_library('dxgi'),
  cc.find_library('dxguid'),
  dependency('DirectX-Headers', version: '>= 1.611'),
]

gdk_win32_deps = [
  pangowin32_dep, # FIXME
  cc.find_library('hid'),
  cc.find_library('opengl32'),
  cc.find_library('shcore'),
  d3d12_dep,
]

libgdk_win32 = static_library('gdk-win32',
  sources: [ gdk_win32_sources, gdk_gen_headers ],
  include_directories: [ confinc, gdkinc ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
    '-DINSIDE_GDK_WIN32',
  ],
  dependencies: [ gdk_deps, gdk_win32_deps ],
)
