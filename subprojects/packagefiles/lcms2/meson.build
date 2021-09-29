project('lcms2', 'c',
  version : '2.12',
  meson_version : '>=0.56.0',
)

mod = import('unstable_external_project')

p = mod.add_project('configure',
  configure_options : ['--prefix=@PREFIX@',
                       '--libdir=@PREFIX@/@LIBDIR@',
                      ],
)

liblcms2_dep = p.dependency('lcms2')
