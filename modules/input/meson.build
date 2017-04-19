# Note: this file is included from the top-level before gtk/meson.build.
# The actual input modules are then built in gtk/meson.build based on the
# defs we provide here. It has to be that way because included input methods
# need to be built before libgtk-4.so is built, so before gtk/meson.build, but
# all input methods also rely on gtk generated headers to be created first, so
# there is a bit of an ordering problem which we solve by collecting all the
# information here but moving the actual build definitions to gtk/meson.build.
build_dynamic_modules = false
disable_modules = get_option('disable-modules')
if not disable_modules
  runcmd = run_command('pkg-config', '--variable=gmodule_supported', 'gmodule-no-export-2.0')
  if runcmd.returncode() == 0
    gmodule_supported = runcmd.stdout().strip()
    if gmodule_supported == 'true'
      build_dynamic_modules = true
    else
      message('Modules are not supported according to gmodule-no-export-2.0.pc')
    endif
  else
    message('WARNING: failed to query if modules are supported!')
  endif
endif

all_immodules = ['am-et', 'cedilla', 'cyrillic-translit', 'inuktitut',
                 'ipa', 'multipress', 'thai', 'ti-er', 'ti-et', 'viqr']

all_immodules += backend_immodules

# Allow building some or all immodules included
included_immodules = get_option('with-included-immodules')
if included_immodules == 'none'
  included_immodules = ''
elif included_immodules == 'all' or included_immodules == 'yes'
  included_immodules = ','.join(all_immodules)
endif

if included_immodules == ''
  have_included_immodules = false
  included_immodules = []
else
  have_included_immodules = true
  included_immodules = included_immodules.split(',')
endif

foreach im : included_immodules
  if not all_immodules.contains(im)
    error('The specified input method "@0@" is not available (available methods: @1@)'.format(im, ', '.join(all_immodules)))
  endif
endforeach

immodules_subdir = 'gtk-4.0/@0@/immodules'.format(gtk_binary_version)
immodules_install_dir = join_paths(get_option('libdir'), immodules_subdir)

sysconfdir = join_paths(get_option('prefix'),get_option('sysconfdir'))
mp_confdir = join_paths(sysconfdir, 'gtk-4.0')
mp_cargs = [
 '-DMULTIPRESS_LOCALEDIR=""', # FIXME: where is $(mplocaledir) ever set?
 '-DMULTIPRESS_CONFDIR="@0@"'.format(mp_confdir),
 '-DGDK_DISABLE_DEPRECATION_WARNINGS',
]
install_data('im-multipress.conf', install_dir : mp_confdir)

method_defs = [
  ['am-et', files('imam-et.c')],
  ['cedilla', files('imcedilla.c')],
  ['cyrillic-translit', files('imcyrillic-translit.c')],
  ['ti-er', files('imti-er.c')],
  ['ti-et', files('imti-et.c')],
  ['thai', files('thai-charprop.c', 'gtkimcontextthai.c', 'imthai.c')],
  ['viqr', files('imviqr.c')],
  ['inuktitut', files('iminuktitut.c')],
  ['ipa', files('imipa.c')],
  # backend modules
  ['broadway', files('imbroadway.c')],
  ['multipress', files('gtkimcontextmultipress.c', 'immultipress.c'), [], mp_cargs],
  ['quartz', ('imquartz.c'), [], ('-xobjective-c')],
  ['xim', files('gtkimcontextxim.c', 'imxim.c')],
  ['ime', files('gtkimcontextime.c', 'imime.c'), ['imm32']],
]

inc_im_method_defs = []
dyn_im_method_defs = []

foreach m : method_defs
  im = m.get(0)
  srcs = m.get(1)
  cargs = m.get(3, [])
  libs = []

  # only use backend-specific input methods for backends that are enabled
  if all_immodules.contains(im)
    # check for extra libs lazily
    foreach libname : m.get(2, [])
      libs += [cc.find_library(libname)]
    endforeach

    if included_immodules.contains(im)
      cdata.set('INCLUDE_IM_@0@'.format(im.underscorify()), true)
      inc_im_method_defs += [[im, srcs, cargs, libs]]
    elif build_dynamic_modules
      dyn_im_method_defs += [[im, srcs, cargs, libs]]
    endif
  endif
endforeach

# TODO: post-install gtk4-query-immodules run to create immodules.c
