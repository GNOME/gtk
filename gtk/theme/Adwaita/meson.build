adwaita_scss_files = files([
  '_colors-public.scss',
  '_colors.scss',
  '_common.scss',
  '_drawing.scss',
])

adwaita_theme_variants = [
  'dark',
]

adwaita_theme_deps = [
  custom_target('Adwaita theme',
    input: 'Adwaita.scss',
    output: 'Adwaita.css',
    command: [
      sassc, sassc_opts, '@INPUT@', '@OUTPUT@',
    ],
    depend_files: adwaita_scss_files,
  ),
]

foreach variant: adwaita_theme_variants
  adwaita_theme_deps += custom_target('Adwaita theme variant: ' + variant,
    input: 'Adwaita-@0@.scss'.format(variant),
    output: 'Adwaita-@0@.css'.format(variant),
    command: [
      sassc, sassc_opts, '@INPUT@', '@OUTPUT@',
    ],
    depend_files: adwaita_scss_files,
  )
endforeach
