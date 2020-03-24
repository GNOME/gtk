hc_scss_files = files([
  '_colors-hc.scss',
])

hc_theme_variants = [
  'inverse',
]

hc_theme_deps = [
  custom_target('HighContrast theme',
    input: 'HighContrast.scss',
    output: 'HighContrast.css',
    command: [
      sassc, sassc_opts, '@INPUT@', '@OUTPUT@',
    ],
    depend_files: hc_scss_files,
  )
]

foreach variant: hc_theme_variants
  hc_theme_deps += custom_target('HighContrast theme variant: ' + variant,
    input: 'HighContrast-@0@.scss'.format(variant),
    output: 'HighContrast-@0@.css'.format(variant),
    command: [
      sassc, sassc_opts, '@INPUT@', '@OUTPUT@',
    ],
    depend_files: hc_scss_files,
  )
endforeach
