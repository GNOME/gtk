default_scss_files = files([
  '_colors-public.scss',
  '_colors.scss',
  '_common.scss',
  '_drawing.scss',
])

default_theme_variants = [
  'light',
  'dark',
  'hc',
  'hc-dark',
]

# ... build the theme files
sassc = find_program('sassc', native: true, required: false)
if sassc.found()
  scss_tool = sassc
  scss_tool_opts = [ '-a', '-M', '-t', 'compact' ]
else
  grass = find_program('grass', native: true, required: true)
  scss_tool = grass
  scss_tool_opts = [ '--style', 'expanded' ]
endif

foreach variant: default_theme_variants
  theme_deps += custom_target('Default theme variant ' + variant,
    input: 'Default-@0@.scss'.format(variant),
    output: 'Default-@0@.css'.format(variant),
    command: [
      scss_tool, scss_tool_opts, '@INPUT@', '@OUTPUT@',
    ],
    depend_files: default_scss_files,
  )
endforeach
